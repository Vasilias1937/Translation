#include "ast.hpp"

#include <ostream>

static void printAstRec(std::ostream& os, const AstNode& node, const std::string& prefix, bool isLast) {
  os << prefix << (isLast ? "└── " : "├── ") << node.kind;
  if (!node.attrs.empty()) {
    os << " [";
    for (std::size_t i = 0; i < node.attrs.size(); ++i) {
      if (i) { os << ", "; }
      os << node.attrs[i].first << ": " << node.attrs[i].second;
    }
    os << "]";
  }
  os << '\n';

  const std::string childPref = prefix + (isLast ? "    " : "│   ");
  const std::size_t n = node.children.size();
  for (std::size_t i = 0; i < n; ++i) {
    if (node.children[i]) { printAstRec(os, *node.children[i], childPref, i + 1 == n); }
  }
}

void printAst(std::ostream& os, const AstNode& node) {
  printAstRec(os, node, "", true);
}
