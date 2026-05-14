#include "parser.hpp"

namespace {

bool isTypeKeywordWord(const std::string& v) {
  return v == "void" || v == "int" || v == "double" || v == "bool";
}

bool isAssignOp(const std::string& op) {
  static const char* ops[] = {"=",  "+=", "-=", "*=", "/=", "%=", "^=", "|=", "&=", "<<=", ">>="};
  for (const char* p : ops) {
    if (op == p) { return true; }
  }
  return false;
}

int assignPrec(const std::string& op) {
  if (op == "=") { return 1; }
  if (op == "?") { return 0; }  // handled separately
  if (op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "^=" || op == "|=" ||
      op == "&=" || op == "<<=" || op == ">>=") {
    return 1;
  }
  return -1;
}

class Parser {
public:
  Parser(const std::vector<Token>& tokens, ParseResult& pr) : tokens_(tokens), pr_(pr) {}

  std::shared_ptr<AstNode> parseTranslationUnit() {
    auto root = std::make_shared<AstNode>();
    root->kind = "TranslationUnit";
    skipPreprocessorChain();
    while (!eof()) {
      skipPreprocessorChain();
      if (eof()) { break; }

      std::string storage;
      if (matchKw("static")) { storage = "static"; }

      std::string baseType;
      if (!parseBaseType(baseType)) {
        err(cur(), "STRUCTURE", "Ожидалось объявление переменной или определение функции на глобальном уровне",
            "ключевое слово типа или имя типа");
        recoverWeak();
        continue;
      }

      if (!requireType("IDENTIFIER", "имя объявляемой сущности")) {
        recoverWeak();
        continue;
      }
      const std::string name = prev_.value;

      if (peekDelim("(")) {
        advance();  // '('
        auto fn = parseFuncDefTail(storage, baseType, name);
        root->children.push_back(fn);
      } else {
        auto decls = parseVariableDeclarators(storage, baseType, name, /*needSemi=*/true);
        for (auto& d : decls) { root->children.push_back(d); }
      }
    }
    return root;
  }

private:
  const std::vector<Token>& tokens_;
  ParseResult& pr_;
  std::size_t pos_{0};
  Token prev_{};

  const Token& cur() const {
    if (pos_ >= tokens_.size()) {
      static Token eof;
      eof.type = "EOF";
      eof.value.clear();
      eof.line = 1;
      eof.col = 1;
      return eof;
    }
    return tokens_[pos_];
  }

  bool eof() const { return pos_ >= tokens_.size(); }

  void advance() {
    if (!eof()) {
      prev_ = tokens_[pos_];
      ++pos_;
    }
  }

  void err(const Token& at, const std::string& cat, const std::string& msg, const std::string& expected) {
    ParseError e;
    e.line = at.line;
    e.col = at.col;
    e.category = cat;
    e.message = msg;
    e.expected = expected;
    pr_.errors.push_back(e);
    pr_.ok = false;
  }

  void recoverWeak() {
    while (!eof()) {
      const Token& t = cur();
      if (t.type == "DELIMITER" && (t.value == ";" || t.value == "}")) { return; }
      advance();
    }
  }

  bool matchKw(const std::string& v) {
    if (!eof() && cur().type == "KEYWORD" && cur().value == v) {
      advance();
      return true;
    }
    return false;
  }

  bool peekKw(const std::string& v) const { return !eof() && cur().type == "KEYWORD" && cur().value == v; }

  bool matchDelim(const std::string& v) {
    if (!eof() && cur().type == "DELIMITER" && cur().value == v) {
      advance();
      return true;
    }
    return false;
  }

  bool peekDelim(const std::string& v) const {
    return !eof() && cur().type == "DELIMITER" && cur().value == v;
  }

  bool matchOp(const std::string& v) {
    if (!eof() && cur().type == "OPERATOR" && cur().value == v) {
      advance();
      return true;
    }
    return false;
  }

  bool peekOp(const std::string& v) const {
    return !eof() && cur().type == "OPERATOR" && cur().value == v;
  }

  bool requireDelim(const std::string& v, const char* what) {
    if (matchDelim(v)) { return true; }
    err(cur(), "MISSING_DELIMITER", std::string("Ожидался разделитель «") + v + "»", what);
    return false;
  }

  bool requireOp(const std::string& v, const char* what) {
    if (matchOp(v)) { return true; }
    err(cur(), "MISSING_OPERATOR", std::string("Ожидался оператор «") + v + "»", what);
    return false;
  }

  bool requireType(const std::string& typ, const char* what) {
    if (!eof() && cur().type == typ) {
      advance();
      return true;
    }
    err(cur(), "UNEXPECTED_TOKEN", "Неожиданная лексема в текущей позиции разбора", what);
    return false;
  }

  bool parseBaseType(std::string& out) {
    if (!eof() && cur().type == "KEYWORD" && isTypeKeywordWord(cur().value)) {
      out = cur().value;
      advance();
      return true;
    }
    if (!eof() && cur().type == "IDENTIFIER") {
      out = cur().value;
      advance();
      return true;
    }
    return false;
  }

  void skipPreprocessorChain() {
    while (peekDelim("#")) {
      advance();  // #
      if (!eof() && cur().type == "PP_DIRECTIVE" && cur().value == "include") {
        advance();
        if (peekOp("<")) {
          advance();
          while (!eof() && !(cur().type == "OPERATOR" && cur().value == ">")) { advance(); }
          matchOp(">");
        } else if (!eof() && cur().type == "STRING_LITERAL") {
          advance();
        } else {
          err(cur(), "PREPROCESSOR", "Некорректная форма директивы #include", "<…> или \"…\"");
          recoverWeak();
        }
        continue;
      }
      err(cur(), "PREPROCESSOR", "Неподдерживаемая директива препроцессора после «#»", "#include");
      recoverWeak();
      break;
    }
  }

  std::shared_ptr<AstNode> parseFuncDefTail(const std::string& storage, const std::string& retType,
                                            const std::string& name) {
    auto fn = std::make_shared<AstNode>();
    fn->kind = "FuncDef";
    if (!storage.empty()) { fn->attrs.push_back({"storage", storage}); }
    fn->attrs.push_back({"returnType", retType});
    fn->attrs.push_back({"name", name});

    auto params = std::make_shared<AstNode>();
    params->kind = "ParamList";
    if (!peekDelim(")")) {
      for (;;) {
        std::string pt;
        if (!parseBaseType(pt)) {
          err(cur(), "STRUCTURE", "Ожидался тип параметра функции", "int/double/bool/void или имя типа");
          recoverWeak();
          break;
        }
        if (!requireType("IDENTIFIER", "имя параметра")) {
          recoverWeak();
          break;
        }
        auto p = std::make_shared<AstNode>();
        p->kind = "ParamDecl";
        p->attrs.push_back({"type", pt});
        p->attrs.push_back({"name", prev_.value});
        params->children.push_back(p);
        if (matchDelim(",")) { continue; }
        break;
      }
    }
    requireDelim(")", "закрывающая скобка списка параметров");
    fn->children.push_back(params);

    fn->children.push_back(parseCompoundStatement());
    return fn;
  }

  std::vector<std::shared_ptr<AstNode>> parseVariableDeclarators(const std::string& storage,
                                                                   const std::string& baseType,
                                                                   const std::string& firstName,
                                                                   bool needSemi) {
    std::vector<std::shared_ptr<AstNode>> out;
    std::string name = firstName;
    for (;;) {
      auto decl = std::make_shared<AstNode>();
      decl->kind = "VarDecl";
      if (!storage.empty()) { decl->attrs.push_back({"storage", storage}); }
      decl->attrs.push_back({"type", baseType});
      decl->attrs.push_back({"name", name});

      if (matchOp("=")) {
        decl->children.push_back(parseExpression());
      }

      out.push_back(decl);

      if (matchDelim(",")) {
        if (!requireType("IDENTIFIER", "имя следующей переменной в списке объявлений")) { break; }
        name = prev_.value;
        continue;
      }
      break;
    }
    if (needSemi) { requireDelim(";", "конец объявления переменной"); }
    return out;
  }

  std::shared_ptr<AstNode> parseCompoundStatement() {
    auto blk = std::make_shared<AstNode>();
    blk->kind = "BlockStmt";
    if (!requireDelim("{", "начало блока операторов")) {
      recoverWeak();
      return blk;
    }
    while (!eof()) {
      skipPreprocessorChain();
      if (peekDelim("}")) { break; }
      blk->children.push_back(parseStatement());
    }
    requireDelim("}", "закрытие блока begin/end в стиле C++");
    return blk;
  }

  bool isSimpleDeclarationStart(std::size_t i) const {
    if (i >= tokens_.size()) { return false; }
    std::size_t j = i;
    if (tokens_[j].type == "KEYWORD" && tokens_[j].value == "static") {
      ++j;
      if (j >= tokens_.size()) { return false; }
    }
    bool typeOk = false;
    if (tokens_[j].type == "KEYWORD" && isTypeKeywordWord(tokens_[j].value)) {
      typeOk = true;
      ++j;
    } else if (tokens_[j].type == "IDENTIFIER") {
      typeOk = true;
      ++j;
    }
    if (!typeOk || j >= tokens_.size()) { return false; }
    if (tokens_[j].type != "IDENTIFIER") { return false; }
    ++j;
    if (j >= tokens_.size()) { return false; }
    const Token& nx = tokens_[j];
    if (nx.type == "OPERATOR" && isAssignOp(nx.value)) { return true; }
    if (nx.type == "DELIMITER" && (nx.value == ";" || nx.value == ",")) { return true; }
    return false;
  }

  std::shared_ptr<AstNode> parseStatement() {
    skipPreprocessorChain();

    if (peekDelim("{")) { return parseCompoundStatement(); }

    if (peekKw("if")) { return parseIfStatement(); }
    if (peekKw("for")) { return parseForStatement(); }
    if (peekKw("while")) { return parseWhileStatement(); }
    if (peekKw("return")) { return parseReturnStatement(); }
    if (peekKw("break")) { return parseBreakStatement(); }
    if (peekKw("using")) { return parseUsingStatement(); }

    if (isSimpleDeclarationStart(pos_)) {
      std::string storage;
      if (matchKw("static")) { storage = "static"; }
      std::string bt;
      parseBaseType(bt);
      if (!requireType("IDENTIFIER", "имя переменной")) {
        recoverWeak();
        auto stub = std::make_shared<AstNode>();
        stub->kind = "ErrorStmt";
        return stub;
      }
      const std::string nm = prev_.value;
      auto decls = parseVariableDeclarators(storage, bt, nm, true);
      auto wrap = std::make_shared<AstNode>();
      wrap->kind = "DeclStmt";
      wrap->children = decls;
      return wrap;
    }

    auto es = std::make_shared<AstNode>();
    es->kind = "ExprStmt";
    es->children.push_back(parseExpression());
    requireDelim(";", "конец оператора-выражения");
    return es;
  }

  std::shared_ptr<AstNode> parseIfStatement() {
    matchKw("if");
    auto n = std::make_shared<AstNode>();
    n->kind = "IfStmt";
    requireDelim("(", "условие if");
    n->children.push_back(parseExpression());
    requireDelim(")", "конец условия if");
    n->children.push_back(parseStatement());
    if (matchKw("else")) {
      if (peekKw("if")) {
        n->children.push_back(parseIfStatement());
      } else {
        n->children.push_back(parseStatement());
      }
    }
    return n;
  }

  std::shared_ptr<AstNode> parseForStatement() {
    matchKw("for");
    auto n = std::make_shared<AstNode>();
    n->kind = "ForStmt";
    requireDelim("(", "заголовок for");

    auto init = std::make_shared<AstNode>();
    init->kind = "ForInit";
    if (!peekDelim(";")) {
      if (isSimpleDeclarationStart(pos_)) {
        std::string storage;
        if (matchKw("static")) { storage = "static"; }
        std::string bt;
        parseBaseType(bt);
        if (requireType("IDENTIFIER", "имя переменной в for-init")) {
          const std::string nm = prev_.value;
          auto decls = parseVariableDeclarators(storage, bt, nm, false);
          init->children = decls;
        }
      } else {
        init->children.push_back(parseExpression());
      }
    }
    n->children.push_back(init);

    requireDelim(";", "разделитель секций for");
    auto cond = std::make_shared<AstNode>();
    cond->kind = "ForCond";
    if (!peekDelim(";")) { cond->children.push_back(parseExpression()); }
    n->children.push_back(cond);

    requireDelim(";", "разделитель секций for");
    auto step = std::make_shared<AstNode>();
    step->kind = "ForStep";
    if (!peekDelim(")")) { step->children.push_back(parseExpression()); }
    n->children.push_back(step);

    requireDelim(")", "конец заголовка for");
    n->children.push_back(parseStatement());
    return n;
  }

  std::shared_ptr<AstNode> parseWhileStatement() {
    matchKw("while");
    auto n = std::make_shared<AstNode>();
    n->kind = "WhileStmt";
    requireDelim("(", "условие while");
    n->children.push_back(parseExpression());
    requireDelim(")", "конец условия while");
    n->children.push_back(parseStatement());
    return n;
  }

  std::shared_ptr<AstNode> parseReturnStatement() {
    matchKw("return");
    auto n = std::make_shared<AstNode>();
    n->kind = "ReturnStmt";
    if (!peekDelim(";")) { n->children.push_back(parseExpression()); }
    requireDelim(";", "конец return");
    return n;
  }

  std::shared_ptr<AstNode> parseBreakStatement() {
    matchKw("break");
    auto n = std::make_shared<AstNode>();
    n->kind = "BreakStmt";
    requireDelim(";", "конец break");
    return n;
  }

  std::shared_ptr<AstNode> parseUsingStatement() {
    matchKw("using");
    auto n = std::make_shared<AstNode>();
    n->kind = "UsingNamespaceStmt";
    if (!matchKw("namespace")) {
      err(cur(), "UNEXPECTED_TOKEN", "После using ожидалось namespace", "namespace");
    }
    if (!requireType("IDENTIFIER", "имя пространства имён")) {
      recoverWeak();
      return n;
    }
    n->attrs.push_back({"name", prev_.value});
    requireDelim(";", "конец директивы using namespace");
    return n;
  }

  // ---------------- expressions (рекурсивный спуск по уровням приоритета) ----------------

  std::shared_ptr<AstNode> parseExpression() { return parseAssignmentExpression(); }

  std::shared_ptr<AstNode> parseAssignmentExpression() {
    std::shared_ptr<AstNode> lhs = parseConditionalExpression();
    if (!eof() && cur().type == "OPERATOR" && assignPrec(cur().value) > 0) {
      std::string op = cur().value;
      advance();
      auto n = std::make_shared<AstNode>();
      n->kind = "BinaryExpr";
      n->attrs.push_back({"op", op});
      n->children.push_back(lhs);
      n->children.push_back(parseAssignmentExpression());  // правоассоциативно
      return n;
    }
    return lhs;
  }

  std::shared_ptr<AstNode> parseConditionalExpression() {
    std::shared_ptr<AstNode> cond = parseLogicalOrExpression();
    if (matchOp("?")) {
      auto n = std::make_shared<AstNode>();
      n->kind = "ConditionalExpr";
      n->children.push_back(cond);
      n->children.push_back(parseExpression());
      requireDelim(":", "ветка else у оператора ?:");
      n->children.push_back(parseConditionalExpression());
      return n;
    }
    return cond;
  }

  std::shared_ptr<AstNode> parseLogicalOrExpression() {
    std::shared_ptr<AstNode> n = parseLogicalAndExpression();
    while (!eof() && cur().type == "OPERATOR" && cur().value == "||") {
      advance();
      auto b = std::make_shared<AstNode>();
      b->kind = "BinaryExpr";
      b->attrs.push_back({"op", "||"});
      b->children.push_back(n);
      b->children.push_back(parseLogicalAndExpression());
      n = b;
    }
    return n;
  }

  std::shared_ptr<AstNode> parseLogicalAndExpression() {
    std::shared_ptr<AstNode> n = parseBitOrExpression();
    while (!eof() && cur().type == "OPERATOR" && cur().value == "&&") {
      advance();
      auto b = std::make_shared<AstNode>();
      b->kind = "BinaryExpr";
      b->attrs.push_back({"op", "&&"});
      b->children.push_back(n);
      b->children.push_back(parseBitOrExpression());
      n = b;
    }
    return n;
  }

  std::shared_ptr<AstNode> parseBitOrExpression() {
    std::shared_ptr<AstNode> n = parseBitXorExpression();
    while (!eof() && cur().type == "OPERATOR" && cur().value == "|") {
      advance();
      auto b = std::make_shared<AstNode>();
      b->kind = "BinaryExpr";
      b->attrs.push_back({"op", "|"});
      b->children.push_back(n);
      b->children.push_back(parseBitXorExpression());
      n = b;
    }
    return n;
  }

  std::shared_ptr<AstNode> parseBitXorExpression() {
    std::shared_ptr<AstNode> n = parseBitAndExpression();
    while (!eof() && cur().type == "OPERATOR" && cur().value == "^") {
      advance();
      auto b = std::make_shared<AstNode>();
      b->kind = "BinaryExpr";
      b->attrs.push_back({"op", "^"});
      b->children.push_back(n);
      b->children.push_back(parseBitAndExpression());
      n = b;
    }
    return n;
  }

  std::shared_ptr<AstNode> parseBitAndExpression() {
    std::shared_ptr<AstNode> n = parseEqualityExpression();
    while (!eof() && cur().type == "OPERATOR" && cur().value == "&") {
      advance();
      auto b = std::make_shared<AstNode>();
      b->kind = "BinaryExpr";
      b->attrs.push_back({"op", "&"});
      b->children.push_back(n);
      b->children.push_back(parseEqualityExpression());
      n = b;
    }
    return n;
  }

  std::shared_ptr<AstNode> parseEqualityExpression() {
    std::shared_ptr<AstNode> n = parseRelationalExpression();
    while (!eof() && cur().type == "OPERATOR" && (cur().value == "==" || cur().value == "!=")) {
      std::string op = cur().value;
      advance();
      auto b = std::make_shared<AstNode>();
      b->kind = "BinaryExpr";
      b->attrs.push_back({"op", op});
      b->children.push_back(n);
      b->children.push_back(parseRelationalExpression());
      n = b;
    }
    return n;
  }

  std::shared_ptr<AstNode> parseRelationalExpression() {
    std::shared_ptr<AstNode> n = parseShiftExpression();
    while (!eof() && cur().type == "OPERATOR" &&
           (cur().value == "<" || cur().value == ">" || cur().value == "<=" || cur().value == ">=")) {
      std::string op = cur().value;
      advance();
      auto b = std::make_shared<AstNode>();
      b->kind = "BinaryExpr";
      b->attrs.push_back({"op", op});
      b->children.push_back(n);
      b->children.push_back(parseShiftExpression());
      n = b;
    }
    return n;
  }

  std::shared_ptr<AstNode> parseShiftExpression() {
    std::shared_ptr<AstNode> n = parseAdditiveExpression();
    while (!eof() && cur().type == "OPERATOR" && (cur().value == "<<" || cur().value == ">>")) {
      std::string op = cur().value;
      advance();
      auto b = std::make_shared<AstNode>();
      b->kind = "BinaryExpr";
      b->attrs.push_back({"op", op});
      b->children.push_back(n);
      b->children.push_back(parseAdditiveExpression());
      n = b;
    }
    return n;
  }

  std::shared_ptr<AstNode> parseAdditiveExpression() {
    std::shared_ptr<AstNode> n = parseMultiplicativeExpression();
    while (!eof() && cur().type == "OPERATOR" && (cur().value == "+" || cur().value == "-")) {
      std::string op = cur().value;
      advance();
      auto b = std::make_shared<AstNode>();
      b->kind = "BinaryExpr";
      b->attrs.push_back({"op", op});
      b->children.push_back(n);
      b->children.push_back(parseMultiplicativeExpression());
      n = b;
    }
    return n;
  }

  std::shared_ptr<AstNode> parseMultiplicativeExpression() {
    std::shared_ptr<AstNode> n = parseUnaryExpression();
    while (!eof() && cur().type == "OPERATOR" &&
           (cur().value == "*" || cur().value == "/" || cur().value == "%")) {
      std::string op = cur().value;
      advance();
      auto b = std::make_shared<AstNode>();
      b->kind = "BinaryExpr";
      b->attrs.push_back({"op", op});
      b->children.push_back(n);
      b->children.push_back(parseUnaryExpression());
      n = b;
    }
    return n;
  }

  bool isUnaryPrefixOp(const Token& t) const {
    if (t.type != "OPERATOR") { return false; }
    return t.value == "+" || t.value == "-" || t.value == "!" || t.value == "~" || t.value == "*" ||
           t.value == "&" || t.value == "++" || t.value == "--";
  }

  std::shared_ptr<AstNode> parseUnaryExpression() {
    if (!eof() && cur().type == "KEYWORD" && cur().value == "static_cast") {
      return parseStaticCastExpression();
    }

    // C-cast: ( type ) expr — отличать от группировки ( expr )
    if (peekDelim("(")) {
      const std::size_t save = pos_;
      advance();  // '('
      std::string ty;
      if (parseBaseType(ty)) {
        if (peekDelim(")")) {
          advance();  // ')'
          auto cast = std::make_shared<AstNode>();
          cast->kind = "CastExpr";
          cast->attrs.push_back({"style", "C"});
          cast->attrs.push_back({"type", ty});
          cast->children.push_back(parseUnaryExpression());
          return cast;
        }
      }
      pos_ = save;
    }

    if (!eof() && isUnaryPrefixOp(cur())) {
      std::string op = cur().value;
      advance();
      auto u = std::make_shared<AstNode>();
      u->kind = "UnaryExpr";
      u->attrs.push_back({"op", op});
      u->children.push_back(parseUnaryExpression());
      return u;
    }

    return parsePostfixExpression();
  }

  std::shared_ptr<AstNode> parseStaticCastExpression() {
    matchKw("static_cast");
    auto n = std::make_shared<AstNode>();
    n->kind = "CastExpr";
    n->attrs.push_back({"style", "static_cast"});
    requireOp("<", "начало аргумента типа static_cast");
    std::string ty;
    if (!parseBaseType(ty)) {
      err(cur(), "UNEXPECTED_TOKEN", "Ожидался тип внутри static_cast<…>", "тип");
    }
    n->attrs.push_back({"type", ty});
    requireOp(">", "конец аргумента типа static_cast");
    requireDelim("(", "начало операнда static_cast");
    n->children.push_back(parseExpression());
    requireDelim(")", "конец операнда static_cast");
    return n;
  }

  std::shared_ptr<AstNode> parsePostfixExpression() {
    std::shared_ptr<AstNode> n = parsePrimaryExpression();
    for (;;) {
      if (peekDelim("(")) {
        advance();
        auto call = std::make_shared<AstNode>();
        call->kind = "CallExpr";
        call->children.push_back(n);
        if (!peekDelim(")")) {
          for (;;) {
            call->children.push_back(parseExpression());
            if (matchDelim(",")) { continue; }
            break;
          }
        }
        requireDelim(")", "аргументы вызова функции");
        n = call;
        continue;
      }
      if (!eof() && cur().type == "OPERATOR" && (cur().value == "++" || cur().value == "--")) {
        std::string op = cur().value;
        advance();
        auto p = std::make_shared<AstNode>();
        p->kind = "PostfixExpr";
        p->attrs.push_back({"op", op});
        p->children.push_back(n);
        n = p;
        continue;
      }
      break;
    }
    return n;
  }

  std::shared_ptr<AstNode> parsePrimaryExpression() {
    if (peekDelim("(")) {
      advance();
      auto inner = parseExpression();
      requireDelim(")", "закрывающая скобка подвыражения");
      auto wrap = std::make_shared<AstNode>();
      wrap->kind = "ParenExpr";
      wrap->children.push_back(inner);
      return wrap;
    }

    if (!eof() && cur().type == "INTEGER_LITERAL") {
      auto lit = std::make_shared<AstNode>();
      lit->kind = "LiteralExpr";
      lit->attrs.push_back({"kind", "int"});
      lit->attrs.push_back({"value", cur().value});
      advance();
      return lit;
    }
    if (!eof() && cur().type == "FLOAT_LITERAL") {
      auto lit = std::make_shared<AstNode>();
      lit->kind = "LiteralExpr";
      lit->attrs.push_back({"kind", "float"});
      lit->attrs.push_back({"value", cur().value});
      advance();
      return lit;
    }
    if (!eof() && cur().type == "BOOL_LITERAL") {
      auto lit = std::make_shared<AstNode>();
      lit->kind = "LiteralExpr";
      lit->attrs.push_back({"kind", "bool"});
      lit->attrs.push_back({"value", cur().value});
      advance();
      return lit;
    }
    if (!eof() && cur().type == "STRING_LITERAL") {
      auto lit = std::make_shared<AstNode>();
      lit->kind = "LiteralExpr";
      lit->attrs.push_back({"kind", "string"});
      lit->attrs.push_back({"value", cur().value});
      advance();
      return lit;
    }
    if (!eof() && cur().type == "CHAR_LITERAL") {
      auto lit = std::make_shared<AstNode>();
      lit->kind = "LiteralExpr";
      lit->attrs.push_back({"kind", "char"});
      lit->attrs.push_back({"value", cur().value});
      advance();
      return lit;
    }

    if (!eof() && cur().type == "IDENTIFIER") {
      auto id = std::make_shared<AstNode>();
      id->kind = "IdExpr";
      id->attrs.push_back({"name", cur().value});
      advance();
      return id;
    }

    err(cur(), "UNEXPECTED_TOKEN", "Неожиданный первичный элемент выражения",
        "литерал, идентификатор или '('");
    auto stub = std::make_shared<AstNode>();
    stub->kind = "ErrorExpr";
    if (!eof()) { advance(); }
    return stub;
  }
};

}  // namespace

ParseResult parseTranslationUnit(const std::vector<Token>& tokens) {
  ParseResult pr;
  Parser p(tokens, pr);
  pr.root = p.parseTranslationUnit();
  if (!pr.errors.empty()) { pr.ok = false; }
  return pr;
}
