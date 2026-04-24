#include "source_cleaner.hpp"

#include <regex>
#include <sstream>
#include <utility>

// Удаление // и /* */ в стиле C++ с учётом ", ', R"delim( )delim"
// Незакрытый /* -> сообщение об ошибке в messages, ok=false
static std::string stripCppComments(
    const std::string& s, std::vector<std::string>& messages, const char* label) {
  std::string out;
  out.reserve(s.size());
  const std::size_t n = s.size();
  std::size_t i = 0;

  auto err = [&](const std::string& m) { messages.push_back(std::string(label) + ": " + m); };

  while (i < n) {
    unsigned char c0 = static_cast<unsigned char>(s[i]);

    if (c0 < 32u && c0 != '\n' && c0 != '\r' && c0 != '\t') {
      err("недопустимый управляющий символ (код " +
          std::to_string(static_cast<int>(c0)) + ")");
    }

    // C++11 raw string: [u8|u|U|L]R"delim(  ...  )delim" — внутри не трогать // и /*
    {
      std::size_t j = 0;
      if (i + 3u < n && s[i] == 'u' && s[i + 1] == '8' && s[i + 2] == 'R' && s[i + 3] == '"') {
        j = i + 4u;
      } else if (i + 2u < n && s[i] == 'u' && s[i + 1] == 'R' && s[i + 2] == '"') {
        j = i + 3u;
      } else if (i + 2u < n && s[i] == 'U' && s[i + 1] == 'R' && s[i + 2] == '"') {
        j = i + 3u;
      } else if (i + 2u < n && s[i] == 'L' && s[i + 1] == 'R' && s[i + 2] == '"') {
        j = i + 3u;
      } else if (i + 1u < n && s[i] == 'R' && s[i + 1] == '"') {
        j = i + 2u;
      }
      if (j != 0) {
        std::string delim;
        if (j < n && s[j] == '(') {
          // пустой разделитель, открывающая '('
        } else {
          while (j < n && s[j] != '(') {
            if (s[j] == '"' || s[j] == '\n' || s[j] == '\r') { break; }
            delim.push_back(s[j++]);
          }
          if (j >= n || s[j] != '(') {
            err("синтаксис raw-строки: ожидается '(' после R\"…\"");
            out.push_back(static_cast<char>(c0));
            ++i;
            continue;
          }
        }
        if (j < n && s[j] == '(') { ++j; }
        for (std::size_t t = i; t < j; ++t) { out.push_back(s[t]); }
        i = j;
        const std::string tail = ')' + delim + '"';
        const std::size_t pos = s.find(tail, i);
        if (pos == std::string::npos) {
          err("незакрытая raw-строка (нет \"…\" завершения)");
          for (; i < n; ++i) { out.push_back(s[i]); }
          return out;
        }
        for (; i < pos + tail.size() && i < n; ++i) { out.push_back(s[i]); }
        continue;
      }
    }

    // обычная строка
    if (c0 == '"') {
      out.push_back(static_cast<char>(c0));
      ++i;
      for (; i < n; ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '\n' || c == '\r') {
          err("незавершённая строковая константа: перевод строки внутри \"…\"");
          break;
        }
        out.push_back(static_cast<char>(c));
        if (c == '"') { ++i; break; }
        if (c == '\\' && i + 1 < n) { out.push_back(s[++i]); }
      }
      continue;
    }
    if (c0 == '\'') {
      out.push_back(static_cast<char>(c0));
      ++i;
      for (; i < n; ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '\n' || c == '\r') {
          err("незавершённый символьный литерал: перевод строки внутри '…'");
          break;
        }
        out.push_back(static_cast<char>(c));
        if (c == '\'') { ++i; break; }
        if (c == '\\' && i + 1 < n) { out.push_back(s[++i]); }
      }
      continue;
    }

    if (c0 == '/' && i + 1 < n) {
      if (s[i + 1] == '/') { i += 2; while (i < n && s[i] != '\n' && s[i] != '\r') { ++i; } continue; }
      if (s[i + 1] == '*') {
        i += 2; bool closed = false;
        while (i < n) {
          if (s[i] == '*' && i + 1 < n && s[i + 1] == '/') { i += 2; closed = true; break; }
          ++i;
        }
        if (!closed) { err("незакрытый многострочный комментарий /* */"); }
        if (!out.empty() && out.back() != ' ' && out.back() != '\n' && out.back() != '\r') {
          out.push_back(' ');
        }
        continue;
      }
    }

    out.push_back(static_cast<char>(c0));
    ++i;
  }
  return out;
}

static std::string applyRegexSpaceAndBlankLines(const std::string& text) {
  static const std::regex reLeading{ R"(^\s+)" };
  static const std::regex reTrailing{ R"(\s+$)" };
  static const std::regex reOnlyBlank{ R"(^\s*$)" };

  std::istringstream in(text);
  std::string line;
  std::string joined;
  bool needNl = false;

  while (std::getline(in, line)) {
    std::string t = line;
    if (!t.empty() && t.back() == '\r') { t.pop_back(); }
    t = std::regex_replace(t, reLeading, "");
    t = std::regex_replace(t, reTrailing, "");
    if (std::regex_match(t, reOnlyBlank)) { continue; }
    if (needNl) { joined.push_back('\n'); } else { needNl = true; }
    joined += t;
  }

  return joined;
}

static void scanForInvalidBytes(const std::string& s, const char* label, std::vector<std::string>& m) {
  for (std::size_t p = 0; p < s.size(); ++p) {
    unsigned char c = static_cast<unsigned char>(s[p]);
    if (c == 0) {
      m.push_back(std::string(label) + ": байт 0x00 (нулевой символ) в исходном тексте");
    }
  }
}

CleanResult cleanSourceLikeCpp(const std::string& source, const char* fileLabel) {
  const char* L = (fileLabel && fileLabel[0] != '\0') ? fileLabel : "file";
  CleanResult r;
  r.messages.push_back(std::string("Обработка: ") + L);
  scanForInvalidBytes(source, L, r.messages);
  r.text = stripCppComments(source, r.messages, L);
  r.text = applyRegexSpaceAndBlankLines(r.text);

  bool hasError = false;
  for (const auto& s : r.messages) {
    if (s.find("незакрыт") != std::string::npos) { hasError = true; break; }
    if (s.find("недопустим") != std::string::npos) { hasError = true; break; }
    if (s.find("незаверш") != std::string::npos) { hasError = true; break; }
    if (s.find("байт 0x00") != std::string::npos) { hasError = true; break; }
    if (s.find("синтаксис raw-строки") != std::string::npos) { hasError = true; break; }
  }
  r.ok = !hasError;
  return r;
}
