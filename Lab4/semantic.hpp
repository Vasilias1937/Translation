#ifndef LAB4_SEMANTIC_HPP
#define LAB4_SEMANTIC_HPP

#include "ast.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct SymbolEntry {
  std::string name;
  std::string type;
  bool declared{true};
  bool initialized{false};
  std::string scope;
  std::size_t declLine{1};
};

struct SemanticError {
  std::string category;
  std::string message;
  std::size_t line{1};
  std::size_t col{1};
};

struct Triad {
  std::string op;
  std::string arg1;
  std::string arg2;
};

struct SemanticResult {
  std::vector<SymbolEntry> symbols;
  std::vector<Triad> triads;
  std::vector<SemanticError> errors;
  bool ok{true};
};

SemanticResult analyzeSemantics(const std::shared_ptr<AstNode>& root);

void printSymbolTable(std::ostream& os, const SemanticResult& r);
void printTriads(std::ostream& os, const SemanticResult& r);

#endif
