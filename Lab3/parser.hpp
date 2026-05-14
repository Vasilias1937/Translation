#ifndef LAB3_PARSER_HPP
#define LAB3_PARSER_HPP

#include "ast.hpp"
#include "lexer.hpp"

#include <memory>
#include <string>
#include <vector>

struct ParseError {
  std::size_t line{1};
  std::size_t col{1};
  /// Краткий код вида UNEXPECTED_TOKEN, MISSING_DELIMITER, STRUCTURE
  std::string category;
  std::string message;
  std::string expected;
};

struct ParseResult {
  std::shared_ptr<AstNode> root;
  std::vector<ParseError> errors;
  bool ok{true};
};

/// Разбор всего файла (единица трансляции): объявления и определения из тестовой программы Lab1.
ParseResult parseTranslationUnit(const std::vector<Token>& tokens);

#endif
