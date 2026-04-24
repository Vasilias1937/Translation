// Модуль-препроцессор: чтение исходника, очистка, вывод на stdout/в файл

#include "source_cleaner.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

static std::string readFile(const char* path, bool& ok) {
  std::ifstream f(path, std::ios::in | std::ios::binary);
  if (!f) { ok = false; return {}; }
  ok = true;
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Использование: " << (argc > 0 ? argv[0] : "preprocessor")
              << " <входной_файл> [выходной_файл]\n";
    return 1;
  }
  const char* inPath = argv[1];
  const char* outPath = (argc >= 3) ? argv[2] : nullptr;

  bool fileOk = false;
  const std::string input = readFile(inPath, fileOk);
  if (!fileOk) {
    std::cerr << "preprocessor: не удалось открыть «" << inPath << "»" << std::endl;
    return 2;
  }

  const CleanResult r = cleanSourceLikeCpp(input, inPath);
  for (const auto& m : r.messages) { std::cout << m << std::endl; }

  if (outPath) {
    std::ofstream o(outPath, std::ios::out | std::ios::binary);
    if (!o) {
      std::cerr << "preprocessor: не удалось записать «" << outPath << "»" << std::endl;
      return 3;
    }
    o << r.text;
  } else {
    std::cout << "--- начало очищенного текста ---\n";
    std::cout << r.text;
    if (!r.text.empty() && r.text.back() != '\n') { std::cout << "\n"; }
    std::cout << "--- конец очищенного текста ---\n";
  }

  if (r.ok) { std::cout << "Ошибок не выявлено" << std::endl; }
  else { std::cerr << "Обработка завершена с предупреждениями/ошибками — см. сообщения выше." << std::endl; }

  return r.ok ? 0 : 4;
}
