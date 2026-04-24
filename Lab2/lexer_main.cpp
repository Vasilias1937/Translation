// Точка входа ЛР2: очищенный код (ЛР1) -> лексический разбор

#include "lexer.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>

// Lab1: очистка исходного текста
#include "source_cleaner.hpp"  // ../Lab1 при компиляции: -I../Lab1

static std::string escq(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (char c : s) { if (c == '\\' || c == '"') { o += '\\'; } o += c; }
  return o;
}

static std::string readAll(const char* path, bool& ok) {
  std::ifstream f(path, std::ios::in | std::ios::binary);
  if (!f) { ok = false; return {}; }
  ok = true;
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Использование: " << (argc > 0 ? argv[0] : "lexer")
              << " <исходник.cpp> [--raw]\n  По умолчанию сначала применяется очистка Lab1. "
                 "Ключ --raw: разбор как есть, без Lab1.\n";
    return 1;
  }
  const char* inPath = argv[1];
  bool raw = false;
  for (int a = 2; a < argc; ++a) {
    if (std::string(argv[a]) == "--raw") { raw = true; }
  }
  bool fileOk = false;
  std::string src = readAll(inPath, fileOk);
  if (!fileOk) {
    std::cerr << "lexer: не удалось прочитать «" << inPath << "»" << std::endl;
    return 2;
  }
  if (!raw) {
    const CleanResult cr = cleanSourceLikeCpp(src, inPath);
    if (!cr.ok) { std::cerr << "Предупреждение: очистка Lab1 с ошибками; разбор идёт по очищенной версии (если пуста — проверьте /*).\n"; }
    src = cr.text;
  }
  const LexResult lr = tokenize(src, inPath);

  std::cout << "-------- Таблица лексем (строка:токен | тип) --------" << std::endl;
  std::cout << std::left;
  for (const Token& t : lr.tokens) {
    std::cout << std::setw(24) << (std::to_string(t.line) + ":" + std::to_string(t.col)) + " " + t.value
              << " | " << t.type << std::endl;
  }
  std::cout << "-------- Последовательность для синтаксического анализатора --------" << std::endl;
  std::cout << "[";
  for (std::size_t k = 0; k < lr.tokens.size(); ++k) {
    if (k) { std::cout << ", "; }
    std::cout << "(" << lr.tokens[k].type << ", \"" << escq(lr.tokens[k].value) << "\")";
  }
  std::cout << "]" << std::endl;

  if (lr.ok) {
    std::cout << "\nЛексический анализ завершён успешно. Обнаружено " << lr.tokens.size()
              << " токенов. Ошибок не найдено." << std::endl;
  } else {
    std::cerr << "\nЛексический анализ с ошибками. Токенов: " << lr.tokens.size() << ".\n";
    for (const auto& e : lr.errors) { std::cerr << e.line << ":" << e.col << "  " << e.message << std::endl; }
    return 3;
  }
  return 0;
}
