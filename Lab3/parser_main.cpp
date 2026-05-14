// ЛР3: поток токенов (ЛР2) → синтаксический разбор и AST

#include "lexer.hpp"
#include "parser.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "source_cleaner.hpp"

#ifdef _WIN32
static void initConsoleUtf8() {
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
}
#endif

static std::string readAll(const char* path, bool& ok) {
  std::ifstream f(path, std::ios::in | std::ios::binary);
  if (!f) {
    ok = false;
    return {};
  }
  ok = true;
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
  initConsoleUtf8();
#endif
  if (argc < 2) {
    std::cerr << "Использование: " << (argc > 0 ? argv[0] : "parser")
              << " <исходник.cpp> [--raw]\n"
                 "  По умолчанию применяется очистка Lab1, затем лексический разбор Lab2.\n"
                 "  Ключ --raw: без очистки Lab1.\n";
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
    std::cerr << "parser: не удалось прочитать «" << inPath << "»" << std::endl;
    return 2;
  }

  if (!raw) {
    const CleanResult cr = cleanSourceLikeCpp(src, inPath);
    if (!cr.ok) {
      std::cerr << "Предупреждение: очистка Lab1 завершилась с сообщениями; разбор по очищенному тексту.\n";
    }
    src = cr.text;
  }

  const LexResult lr = tokenize(src, inPath);
  if (!lr.ok) {
    std::cerr << "Лексические ошибки — синтаксический анализ не выполняется.\n";
    for (const auto& e : lr.errors) { std::cerr << e.line << ":" << e.col << "  " << e.message << std::endl; }
    return 3;
  }

  ParseResult pr = parseTranslationUnit(lr.tokens);

  std::cout << "-------- Абстрактное синтаксическое дерево --------\n";
  if (pr.root) { printAst(std::cout, *pr.root); }

  std::cout << "\n-------- Отчёт --------\n";
  if (pr.ok && pr.errors.empty()) {
    std::cout << "Синтаксический анализ завершён успешно. Ошибок не найдено.\n";
    return 0;
  }

  std::cout << "Синтаксический анализ завершён с ошибками.\n";
  for (const auto& e : pr.errors) {
    std::cout << e.line << ":" << e.col << " [" << e.category << "] " << e.message << "\n";
    if (!e.expected.empty()) { std::cout << "  Ожидалось: " << e.expected << "\n"; }
  }
  return 4;
}
