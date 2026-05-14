#ifndef LAB3_AST_HPP
#define LAB3_AST_HPP

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

/// Узел AST: тип конструкции, атрибуты (метки листьев) и дочерние узлы.
struct AstNode {
  std::string kind;
  std::vector<std::pair<std::string, std::string>> attrs;
  std::vector<std::shared_ptr<AstNode>> children;
};

void printAst(std::ostream& os, const AstNode& node);

#endif
