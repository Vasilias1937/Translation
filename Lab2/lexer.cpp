#include "lexer.hpp"

#include <cctype>
#include <unordered_set>
#include <vector>

namespace {

const std::string T_KEYWORD = "KEYWORD";
const std::string T_ID = "IDENTIFIER";
const std::string T_INT = "INTEGER_LITERAL";
const std::string T_FLOAT = "FLOAT_LITERAL";
const std::string T_STR = "STRING_LITERAL";
const std::string T_CHAR = "CHAR_LITERAL";
const std::string T_BOOL = "BOOL_LITERAL";
const std::string T_OP = "OPERATOR";
const std::string T_DELIM = "DELIMITER";
const std::string T_PRE = "PP_DIRECTIVE";

struct Pos {
  std::size_t i{0};
  std::size_t line{1};
  std::size_t col{1};
};

void adv(Pos& p, const std::string& s) {
  if (p.i >= s.size()) { return; }
  if (s[p.i] == '\n') { ++p.line, p.col = 1; } else { ++p.col; }
  ++p.i;
}
char gc(const std::string& s, const Pos& p) { return p.i < s.size() ? s[p.i] : '\0'; }

bool isId0(char c) { return c == '_' || (std::isalpha(static_cast<unsigned char>(c)) != 0); }
bool isId1(char c) { return c == '_' || (std::isalnum(static_cast<unsigned char>(c)) != 0); }

// Ключевые слова из test.cpp (C++ + static_cast; true/false в BOOL; include — в PP)
const char* KW[] = {"static_cast", "namespace",  "return",  "static",  "using",  "bool",  "break",
                    "double",  "else",  "for",  "if",  "int",  "void",  "while"};
const std::unordered_set<std::string> kw{std::begin(KW), std::end(KW)};

}  // namespace

static void pushT(std::vector<Token>& v, const std::string& typ, const std::string& val, size_t l, size_t c) {
  Token t;
  t.type = typ;
  t.value = val;
  t.line = l;
  t.col = c;
  v.push_back(t);
}
static void pushE(std::vector<LexError>& e, const std::string& m, size_t l, size_t c) {
  LexError x;
  x.message = m;
  x.line = l;
  x.col = c;
  e.push_back(x);
}

// Длинные операторы/пунктуаторы первыми; завершитель nullptr
static const char* g_multi[] = {">>=",  "<<=", "^=", "|=", "&=",
                                 "->*",  "->",  "::", "==",  "!=",  "<=",
                                 ">=",   "&&",  "||", "<<", ">>",  "++",
                                 "--",   "+=",  "-=", "*=",  "/=",  "%=",
                                 0,      0};

// Числа: 0x.., целое, .5, 1.5, 1e-3, ошибка две точки; «1abc» — одна лексема, ошибка
static void readNumber(Pos& p, const std::string& s, size_t l0, size_t c0, std::vector<Token>& t,
                       std::vector<LexError>& e) {
  if (p.i < s.size() && s[p.i] == '0' && p.i + 1 < s.size() && (s[p.i + 1] == 'x' || s[p.i + 1] == 'X')) {
    adv(p, s);
    adv(p, s);
    std::string w = "0x";
    if (p.i >= s.size() || !std::isxdigit(static_cast<unsigned char>(gc(s, p)))) {
      pushE(e, "Некорректно оформленная шестнадцатеричная константа: нет цифр после 0x/0X", l0, c0);
      return;
    }
    while (p.i < s.size() && std::isxdigit(static_cast<unsigned char>(gc(s, p)))) { w += gc(s, p), adv(p, s); }
    if (p.i < s.size() && (isId0(s[p.i]) && !std::isxdigit(static_cast<unsigned char>(s[p.i])))) {
      pushE(e, "Символы, недопустимые в константе сразу после 0x-цифр", p.line, p.col);
    }
    pushT(t, T_INT, w, l0, c0);
    return;
  }
  if (p.i < s.size() && s[p.i] == '.') {  // .5
    adv(p, s);
    std::string w = ".";
    if (p.i >= s.size() || !std::isdigit(static_cast<unsigned char>(gc(s, p)))) {
      pushE(e, "Некорректная вещественная константа: цифры ожидаются после ведущей «.»", p.line, p.col);
      return;
    }
    while (p.i < s.size() && std::isdigit(static_cast<unsigned char>(gc(s, p)))) { w += gc(s, p), adv(p, s); }
    if (p.i < s.size() && s[p.i] == '.') { pushE(e, "Вторая точка в вещественной константе", p.line, p.col); return; }
    if (p.i < s.size() && (s[p.i] == 'e' || s[p.i] == 'E')) {
      w += s[p.i], adv(p, s);
      if (p.i < s.size() && (s[p.i] == '+' || s[p.i] == '-')) { w += s[p.i], adv(p, s); }
      if (p.i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[p.i]))) { pushE(e, "Нет порядка в e/E-записи", p.line, p.col); } else { while (p.i < s.size() && std::isdigit(static_cast<unsigned char>(gc(s, p)))) { w += gc(s, p), adv(p, s); } }
    }
    if (p.i < s.size() && isId0(s[p.i])) { pushE(e, "Символы, недопустимые сразу после вещественной константы", p.line, p.col); }
    pushT(t, T_FLOAT, w, l0, c0);
    return;
  }

  if (p.i < s.size() && std::isdigit(static_cast<unsigned char>(s[p.i]))) {
    std::string w;
    while (p.i < s.size() && std::isdigit(static_cast<unsigned char>(gc(s, p)))) { w += gc(s, p), adv(p, s); }
    if (p.i < s.size() && s[p.i] == '.') {
      w += s[p.i], adv(p, s);
      while (p.i < s.size() && std::isdigit(static_cast<unsigned char>(gc(s, p)))) { w += gc(s, p), adv(p, s); }
      if (p.i < s.size() && s[p.i] == '.') { pushE(e, "Некорректная вещественная константа: вторая точка в одном числе", p.line, p.col); for (; p.i < s.size() && (s[p.i] == '.' || std::isdigit(static_cast<unsigned char>(s[p.i])));) { adv(p, s); } pushT(t, T_FLOAT, w, l0, c0); return; }
      if (p.i < s.size() && (s[p.i] == 'e' || s[p.i] == 'E')) {
        w += s[p.i], adv(p, s);
        if (p.i < s.size() && (s[p.i] == '+' || s[p.i] == '-')) { w += s[p.i], adv(p, s); }
        if (p.i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[p.i]))) { pushE(e, "Нет порядка в e/E-записи", p.line, p.col); } else { while (p.i < s.size() && std::isdigit(static_cast<unsigned char>(gc(s, p)))) { w += gc(s, p), adv(p, s); } }
      }
      if (p.i < s.size() && isId0(s[p.i]) && s[p.i] != 'e' && s[p.i] != 'E' && s[p.i] != '.') { pushE(e, "Символы, недопустимые сразу после вещественной константы", p.line, p.col); }
      pushT(t, T_FLOAT, w, l0, c0);
      return;
    }
    if (p.i < s.size() && (s[p.i] == 'e' || s[p.i] == 'E')) {  // 1e+3
      w += s[p.i], adv(p, s);
      if (p.i < s.size() && (s[p.i] == '+' || s[p.i] == '-')) { w += s[p.i], adv(p, s); }
      if (p.i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[p.i]))) { pushE(e, "Некорректная e/E-экспоненциальная константа", p.line, p.col); } else { while (p.i < s.size() && std::isdigit(static_cast<unsigned char>(gc(s, p)))) { w += gc(s, p), adv(p, s); } }
      if (p.i < s.size() && isId0(s[p.i])) { pushE(e, "Символы, недопустимые сразу после e/E-константы", p.line, p.col); }
      pushT(t, T_FLOAT, w, l0, c0);
      return;
    }
    if (p.i < s.size() && isId0(s[p.i])) { pushE(e, "Некорректная лексема: идентификатор не должен непосредственно следовать за целой константой (1abc)", p.line, p.col); }
    pushT(t, T_INT, w, l0, c0);
  }
}

static void readStr(Pos& p, const std::string& s, char q, size_t l0, size_t c0, std::vector<Token>& t,
                    std::vector<LexError>& e, const std::string& typ) {
  std::string out(1, q);
  adv(p, s);
  for (;;) {
    if (p.i >= s.size() || s[p.i] == '\n' || s[p.i] == '\r') {
      pushE(e, "Незакрытая строковая/символьная константа: нет закрывающей " + std::string(1, q), p.line, p.col);
      pushT(t, typ, out, l0, c0);
      return;
    }
    if (s[p.i] == q) { out += q, adv(p, s), pushT(t, typ, out, l0, c0); return; }
    if (s[p.i] == '\\' && p.i + 1 < s.size()) { out += s[p.i], out += s[p.i + 1], adv(p, s), adv(p, s); continue; }
    out += s[p.i], adv(p, s);
  }
}

LexResult tokenize(const std::string& s, const char* fileLabel) {
  (void)fileLabel;
  LexResult R;
  Pos p;
  for (;;) {
    if (p.i >= s.size()) { break; }
    char c0 = s[p.i];
    if (c0 == ' ' || c0 == '\t' || c0 == '\n' || c0 == '\r' || c0 == '\f' || c0 == '\v') { adv(p, s); continue; }

    size_t L0 = p.line, C0 = p.col;
    if (c0 == '/' && p.i + 1 < s.size() && s[p.i + 1] == '/') { while (p.i < s.size() && s[p.i] != '\n') { adv(p, s); } continue; }
    if (c0 == '/' && p.i + 1 < s.size() && s[p.i + 1] == '*') {
      adv(p, s);
      adv(p, s);
      bool cl = false;
      while (p.i + 1 < s.size()) { if (s[p.i] == '*' && s[p.i + 1] == '/') { adv(p, s), adv(p, s), cl = true; break; } adv(p, s); }
      if (!cl) { pushE(R.errors, "Незакрытый блочный комментарий /* */ (в «очищенном» коде не ожидается)", p.line, p.col); p.i = s.size(); }
      continue;
    }

    if (c0 == '"' || c0 == '\'') { readStr(p, s, c0, L0, C0, R.tokens, R.errors, (c0 == '"') ? T_STR : T_CHAR); continue; }
    if (c0 == '#') { pushT(R.tokens, T_DELIM, "#", L0, C0), adv(p, s); continue; }
    if (c0 == '(') { pushT(R.tokens, T_DELIM, "(", L0, C0), adv(p, s); continue; }
    if (c0 == ')') { pushT(R.tokens, T_DELIM, ")", L0, C0), adv(p, s); continue; }
    if (c0 == '{') { pushT(R.tokens, T_DELIM, "{", L0, C0), adv(p, s); continue; }
    if (c0 == '}') { pushT(R.tokens, T_DELIM, "}", L0, C0), adv(p, s); continue; }
    if (c0 == '[') { pushT(R.tokens, T_DELIM, "[", L0, C0), adv(p, s); continue; }
    if (c0 == ']') { pushT(R.tokens, T_DELIM, "]", L0, C0), adv(p, s); continue; }
    if (c0 == ';') { pushT(R.tokens, T_DELIM, ";", L0, C0), adv(p, s); continue; }
    if (c0 == ',') { pushT(R.tokens, T_DELIM, ",", L0, C0), adv(p, s); continue; }
    if (c0 == ':') { pushT(R.tokens, T_DELIM, ":", L0, C0), adv(p, s); continue; }

    if (std::isdigit(static_cast<unsigned char>(c0)) ||
        (c0 == '.' && p.i + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[p.i + 1])))) { readNumber(p, s, L0, C0, R.tokens, R.errors); continue; }
    if (c0 == '.') { pushT(R.tokens, T_OP, ".", L0, C0), adv(p, s); continue; }  // доступ .field (между идентификаторами)

    if (isId0(c0)) {
      std::string w;
      while (p.i < s.size() && isId1(gc(s, p))) { w += gc(s, p), adv(p, s); }
      if (w == "true" || w == "false") { pushT(R.tokens, T_BOOL, w, L0, C0); continue; }
      if (w == "include") { pushT(R.tokens, T_PRE, w, L0, C0); continue; }
      if (kw.find(w) != kw.end()) { pushT(R.tokens, T_KEYWORD, w, L0, C0); continue; }
      pushT(R.tokens, T_ID, w, L0, C0);
      continue;
    }

    bool mch = false;
    for (int m = 0; g_multi[m] != 0; ++m) {
      const char* u = g_multi[m];
      size_t n = 0;
      for (; u[n]; ++n) { }
      if (p.i + n <= s.size() && s.compare(p.i, n, u) == 0) {
        pushT(R.tokens, T_OP, std::string(u), L0, C0);
        for (size_t k = 0; k < n; ++k) { adv(p, s); }
        mch = true;
        break;
      }
    }
    if (mch) { continue; }
    if (c0 == '+' || c0 == '-' || c0 == '*' || c0 == '/' || c0 == '%' || c0 == '=' || c0 == '!' || c0 == '?' ||
        c0 == '<' || c0 == '>' || c0 == '&' || c0 == '^' || c0 == '~' || c0 == '|') { pushT(R.tokens, T_OP, std::string(1, c0), L0, C0), adv(p, s); continue; }
    if (c0 == '.') { pushT(R.tokens, T_OP, ".", L0, C0), adv(p, s); continue; }  // доступ к полю, редкие варианты
    if (c0 < 32) {
      pushE(R.errors, "Недопустимый управляющий символ (код " + std::to_string(static_cast<int>(c0)) + ")", L0, C0);
    } else { pushE(R.errors, "Неизвестный символ в позиции лексического разбора: «" + std::string(1, c0) + "»", L0, C0); }
    adv(p, s);
  }
  R.ok = R.errors.empty();
  return R;
}
