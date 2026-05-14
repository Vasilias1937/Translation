// ЛР1–4: полный конвейер — очистка → лексика → синтаксис → семантика и триады.

#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"

#include "source_cleaner.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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
    std::cerr << "Использование: " << (argc > 0 ? argv[0] : "compiler")
              << " <исходник.cpp> [--raw]\n"
                 "  Lab1: очистка (по умолчанию). Lab2: lexer. Lab3: parser. Lab4: semantics + триады.\n"
                 "  --raw — без очистки Lab1.\n";
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
    std::cerr << "Не удалось прочитать файл «" << inPath << "»\n";
    return 2;
  }

  if (!raw) {
    const CleanResult cr = cleanSourceLikeCpp(src, inPath);
    if (!cr.ok) {
      std::cerr << "Предупреждение: очистка Lab1 завершилась с сообщениями.\n";
    }
    src = cr.text;
  }

  const LexResult lr = tokenize(src, inPath);
  if (!lr.ok) {
    std::cerr << "Лексические ошибки — дальнейшие этапы не выполняются.\n";
    for (const auto& e : lr.errors) { std::cerr << e.line << ":" << e.col << "  " << e.message << "\n"; }
    return 3;
  }

  ParseResult pr = parseTranslationUnit(lr.tokens);
  if (!pr.ok || !pr.root) {
    std::cerr << "Синтаксические ошибки — семантический анализ не выполняется.\n";
    for (const auto& e : pr.errors) {
      std::cerr << e.line << ":" << e.col << " [" << e.category << "] " << e.message << "\n";
    }
    return 4;
  }

  SemanticResult sr = analyzeSemantics(pr.root);

  std::cout << "-------- Абстрактное синтаксическое дерево (фрагмент корня) --------\n";
  printAst(std::cout, *pr.root);

  std::cout << "\n-------- Таблица символов --------\n";
  printSymbolTable(std::cout, sr);

  std::cout << "\n-------- Триады --------\n";
  printTriads(std::cout, sr);

  std::cout << "\n-------- Отчёт семантики --------\n";
  if (sr.ok && sr.errors.empty()) {
    std::cout << "Семантический анализ завершён успешно. Ошибок не найдено.\n";
    return 0;
  }
  std::cout << "Семантический анализ завершён с ошибками.\n";
  for (const auto& e : sr.errors) {
    std::cout << "[" << e.category << "] " << e.message << " (строка " << e.line << ", столбец " << e.col << ")\n";
  }
  return 5;
}
