#ifndef LEXER_HPP
#define LEXER_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// Типы лексем (согласуются с требованием: таблицы + список для СА)
struct Token {
  std::string type;  // KEYWORD, IDENTIFIER, INTEGER_LITERAL, ...
  std::string value;
  std::size_t line{1};
  std::size_t col{1};
};

struct LexError {
  std::string message;
  std::size_t line{1};
  std::size_t col{1};
};

struct LexResult {
  std::vector<Token> tokens;
  std::vector<LexError> errors;
  /// true, если критичных лексических ошибок нет (токенизация сильно не нарушена)
  bool ok{true};
};

// Лексический разбор (очищенный C++-подобный исходник, как после Lab1)
LexResult tokenize(const std::string& source, const char* fileLabel = "");

#endif
