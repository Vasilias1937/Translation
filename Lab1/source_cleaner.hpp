#ifndef SOURCE_CLEANER_HPP
#define SOURCE_CLEANER_HPP

#include <cstddef>
#include <string>
#include <vector>

struct CleanResult {
  std::string text;
  bool ok = true;
  /// Сообщения: информация и предупреждения (успех — отдельная строка снаружи)
  std::vector<std::string> messages;
};

// Удаляет C/C++-комментарии (//, /* */) с учётом строк/символов, затем нормализует
// пробелы и пустые строки с помощью std::regex.
CleanResult cleanSourceLikeCpp(const std::string& source, const char* fileLabel);

#endif
