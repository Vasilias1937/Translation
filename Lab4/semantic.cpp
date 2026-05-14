#include "semantic.hpp"

#include <sstream>
#include <unordered_map>

namespace {

std::string attr(const AstNode& n, const std::string& k) {
  for (const auto& p : n.attrs) {
    if (p.first == k) { return p.second; }
  }
  return {};
}

enum class SemType { Unknown, Void, Int, Double, Bool, String, Char };

std::string semTypeName(SemType t) {
  switch (t) {
    case SemType::Void:
      return "void";
    case SemType::Int:
      return "int";
    case SemType::Double:
      return "double";
    case SemType::Bool:
      return "bool";
    case SemType::String:
      return "string";
    case SemType::Char:
      return "char";
    default:
      return "unknown";
  }
}

SemType parseDataType(const std::string& s) {
  if (s == "int" || s == "int32_t") { return SemType::Int; }
  if (s == "double") { return SemType::Double; }
  if (s == "bool") { return SemType::Bool; }
  if (s == "void") { return SemType::Void; }
  // подмножество C++ из тестов: неизвестное имя типа трактуем как int (осторожное допущение)
  if (!s.empty()) { return SemType::Int; }
  return SemType::Unknown;
}

bool isNumeric(SemType t) { return t == SemType::Int || t == SemType::Double; }

SemType promote(SemType a, SemType b) {
  if (a == SemType::Double || b == SemType::Double) { return SemType::Double; }
  if (isNumeric(a) && isNumeric(b)) { return SemType::Int; }
  return SemType::Unknown;
}

bool assignCompatible(SemType lhs, SemType rhs) {
  if (lhs == SemType::Unknown || rhs == SemType::Unknown) { return true; }
  if (lhs == rhs) { return true; }
  if (lhs == SemType::Double && rhs == SemType::Int) { return true; }
  return false;
}

struct VarInfo {
  SemType type{SemType::Unknown};
  bool initialized{false};
  std::size_t declLine{1};
  std::string scopeTag;
};

struct FuncInfo {
  SemType ret{SemType::Void};
  std::vector<SemType> params;
};

class Analyzer {
public:
  explicit Analyzer(SemanticResult& out) : out_(out) {
    FuncInfo printfFn;
    printfFn.ret = SemType::Void;
    printfFn.params.clear();  // встроенная: без строгой проверки числа параметров
    funcs_["printf"] = std::move(printfFn);
  }

  void process(const std::shared_ptr<AstNode>& root) {
    if (!root || root->kind != "TranslationUnit") {
      error("STRUCTURE", "Ожидался корень AST вида TranslationUnit", 1, 1);
      return;
    }
    scopeLabels_.push_back("global");
    scopes_.emplace_back();
    for (const auto& ch : root->children) {
      if (!ch) { continue; }
      if (ch->kind == "VarDecl") { processGlobalVarDecl(ch); }
      else if (ch->kind == "FuncDef") { collectFuncSignature(ch); }
    }
    for (const auto& ch : root->children) {
      if (!ch) { continue; }
      if (ch->kind == "FuncDef") { processFuncDef(ch); }
    }
    scopeLabels_.pop_back();
    scopes_.pop_back();
  }

private:
  SemanticResult& out_;
  std::vector<std::unordered_map<std::string, VarInfo>> scopes_;
  std::vector<std::string> scopeLabels_;
  std::unordered_map<std::string, FuncInfo> funcs_;

  std::vector<Triad>& triads() { return out_.triads; }

  int temp_{0};
  int label_{0};

  std::string newTemp() { return std::string("#t") + std::to_string(++temp_); }
  std::string newLabel() { return std::string("@L") + std::to_string(++label_); }

  int emit(const std::string& op, const std::string& a, const std::string& b) {
    triads().push_back({op, a, b});
    return static_cast<int>(triads().size());
  }

  std::string triadRef(int idx) { return "^" + std::to_string(idx); }

  void error(const std::string& cat, const std::string& msg, std::size_t line, std::size_t col) {
    SemanticError e;
    e.category = cat;
    e.message = msg;
    e.line = line;
    e.col = col;
    out_.errors.push_back(e);
    out_.ok = false;
  }

  void recordSymbol(const std::string& name, SemType ty, bool initialized, std::size_t line) {
    SymbolEntry se;
    se.name = name;
    se.type = semTypeName(ty);
    se.declared = true;
    se.initialized = initialized;
    se.scope = scopeLabels_.empty() ? std::string("global") : scopeLabels_.back();
    se.declLine = line;
    out_.symbols.push_back(se);
  }

  VarInfo* lookupVarMutable(const std::string& name) {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
      auto it = scopes_[static_cast<std::size_t>(i)].find(name);
      if (it != scopes_[static_cast<std::size_t>(i)].end()) { return &it->second; }
    }
    return nullptr;
  }

  bool declareVar(const std::string& name, SemType ty, bool initialized, std::size_t line) {
    if (scopes_.empty()) {
      error("INTERNAL", "Внутренняя ошибка области видимости", line, 1);
      return false;
    }
    auto& top = scopes_.back();
    if (top.find(name) != top.end()) {
      error("REDEFINITION", "Повторное объявление переменной «" + name + "» в текущей области видимости", line, 1);
      return false;
    }
    VarInfo vi;
    vi.type = ty;
    vi.initialized = initialized;
    vi.declLine = line;
    vi.scopeTag = scopeLabels_.empty() ? std::string("global") : scopeLabels_.back();
    top[name] = vi;
    recordSymbol(name, ty, initialized, line);
    return true;
  }

  void markVarInitialized(const std::string& name, std::size_t line, std::size_t col) {
    VarInfo* v = lookupVarMutable(name);
    if (!v) {
      error("UNDECLARED", "Использование необъявленной переменной «" + name + "»", line, col);
      return;
    }
    v->initialized = true;
    for (auto& se : out_.symbols) {
      if (se.name == name && se.scope == v->scopeTag) {
        se.initialized = true;
        break;
      }
    }
  }

  SemType lookupVarType(const std::string& name, std::size_t line, std::size_t col) {
    VarInfo* v = lookupVarMutable(name);
    if (!v) {
      error("UNDECLARED", "Использование необъявленной переменной «" + name + "»", line, col);
      return SemType::Unknown;
    }
    return v->type;
  }

  void pushScope(const std::string& label) {
    scopes_.emplace_back();
    scopeLabels_.push_back(label);
  }

  void popScope() {
    if (!scopes_.empty()) { scopes_.pop_back(); }
    if (!scopeLabels_.empty()) { scopeLabels_.pop_back(); }
  }

  void processGlobalVarDecl(const std::shared_ptr<AstNode>& n) {
    const std::string name = attr(*n, "name");
    const SemType ty = parseDataType(attr(*n, "type"));
    bool hasInit = !n->children.empty();
    if (!declareVar(name, ty, hasInit, n->attrs.empty() ? 1 : 1)) {
      if (hasInit) {
        SemType rt;
        genExpr(n->children[0], rt, 1, 1);
      }
      return;
    }
    if (hasInit) {
      SemType rt;
      std::string rhs = genExpr(n->children[0], rt, 1, 1);
      if (!assignCompatible(ty, rt)) {
        error("TYPE_MISMATCH", "Тип инициализатора не совместим с типом переменной «" + name + "»", 1, 1);
      }
      emit(":=", name, rhs);
    }
  }

  void collectFuncSignature(const std::shared_ptr<AstNode>& fn) {
    const std::string fname = attr(*fn, "name");
    FuncInfo fi;
    fi.ret = parseDataType(attr(*fn, "returnType"));
    if (fn->children.empty()) { return; }
    const auto& plist = fn->children[0];
    if (plist && plist->kind == "ParamList") {
      for (const auto& p : plist->children) {
        if (!p || p->kind != "ParamDecl") { continue; }
        fi.params.push_back(parseDataType(attr(*p, "type")));
      }
    }
    funcs_[fname] = std::move(fi);
  }

  void processFuncDef(const std::shared_ptr<AstNode>& fn) {
    const std::string fname = attr(*fn, "name");
    pushScope(std::string("func:") + fname);
    const FuncInfo* meta = nullptr;
    auto it = funcs_.find(fname);
    if (it != funcs_.end()) { meta = &it->second; }

    if (fn->children.size() >= 1) {
      const auto& plist = fn->children[0];
      if (plist && plist->kind == "ParamList") {
        std::size_t pi = 0;
        for (const auto& p : plist->children) {
          if (!p || p->kind != "ParamDecl") { continue; }
          const std::string pname = attr(*p, "name");
          SemType pt = parseDataType(attr(*p, "type"));
          if (meta && pi < meta->params.size() && meta->params[pi] != pt) {
            /* тип совпадает с сигнатурой по AST */
          }
          declareVar(pname, pt, true, 1);
          ++pi;
        }
      }
    }

    if (fn->children.size() >= 2 && fn->children[1]) { visitStmt(fn->children[1]); }

    popScope();
  }

  void visitStmt(const std::shared_ptr<AstNode>& n) {
    if (!n) { return; }
    if (n->kind == "BlockStmt") {
      pushScope("block");
      for (const auto& ch : n->children) { visitStmt(ch); }
      popScope();
      return;
    }
    if (n->kind == "DeclStmt") {
      for (const auto& d : n->children) { processLocalVarDecl(d); }
      return;
    }
    if (n->kind == "ExprStmt") {
      if (!n->children.empty()) {
        SemType t;
        genExpr(n->children[0], t, 1, 1);
      }
      return;
    }
    if (n->kind == "ReturnStmt") {
      SemType t = SemType::Void;
      std::string rv = "-";
      if (!n->children.empty()) { rv = genExpr(n->children[0], t, 1, 1); }
      emit("return", rv, "-");
      return;
    }
    if (n->kind == "BreakStmt") {
      if (breakLabels_.empty()) {
        error("BREAK_OUTSIDE_LOOP", "Оператор break вне цикла", 1, 1);
      } else {
        emit("goto", breakLabels_.back(), "-");
      }
      return;
    }
    if (n->kind == "UsingNamespaceStmt") { return; }
    if (n->kind == "IfStmt") {
      visitIf(n);
      return;
    }
    if (n->kind == "WhileStmt") {
      visitWhile(n);
      return;
    }
    if (n->kind == "ForStmt") {
      visitFor(n);
      return;
    }
    if (n->kind == "ErrorStmt") { return; }
  }

  void processLocalVarDecl(const std::shared_ptr<AstNode>& n) {
    if (!n || n->kind != "VarDecl") { return; }
    const std::string name = attr(*n, "name");
    const SemType ty = parseDataType(attr(*n, "type"));
    bool hasInit = !n->children.empty();
    if (!declareVar(name, ty, hasInit, 1)) {
      if (hasInit) {
        SemType rt;
        genExpr(n->children[0], rt, 1, 1);
      }
      return;
    }
    if (hasInit) {
      SemType rt;
      std::string rhs = genExpr(n->children[0], rt, 1, 1);
      if (!assignCompatible(ty, rt)) {
        error("TYPE_MISMATCH", "Тип выражения справа от «" + name + "» не совместим с типом объявления", 1, 1);
      }
      emit(":=", name, rhs);
    }
  }

  void visitIf(const std::shared_ptr<AstNode>& n) {
    SemType ct;
    std::string cond = genExpr(n->children[0], ct, 1, 1);
    if (ct != SemType::Bool && ct != SemType::Int && ct != SemType::Unknown) {
      error("TYPE_MISMATCH", "Условие оператора if должно быть приводимо к bool", 1, 1);
    }
    const std::string Lelse = newLabel();
    const std::string Lend = newLabel();
    emit("if_false", cond, Lelse);
    visitStmt(n->children[1]);
    if (n->children.size() > 2) {
      emit("goto", Lend, "-");
      emit("label", Lelse, "-");
      visitStmt(n->children[2]);
      emit("label", Lend, "-");
    } else {
      emit("label", Lelse, "-");
    }
  }

  void visitWhile(const std::shared_ptr<AstNode>& n) {
    const std::string Ls = newLabel();
    const std::string Le = newLabel();
    emit("label", Ls, "-");
    SemType ct;
    std::string cond = genExpr(n->children[0], ct, 1, 1);
    emit("if_false", cond, Le);
    breakLabels_.push_back(Le);
    visitStmt(n->children[1]);
    breakLabels_.pop_back();
    emit("goto", Ls, "-");
    emit("label", Le, "-");
  }

  void visitFor(const std::shared_ptr<AstNode>& n) {
    pushScope("for");
    const auto& init = n->children[0];
    if (init && init->kind == "ForInit") {
      for (const auto& ch : init->children) {
        if (!ch) { continue; }
        if (ch->kind == "DeclStmt") {
          for (const auto& d : ch->children) { processLocalVarDecl(d); }
        } else if (ch->kind == "VarDecl") {
          processLocalVarDecl(ch);
        } else {
          SemType t;
          genExpr(ch, t, 1, 1);
        }
      }
    }
    const std::string Ls = newLabel();
    const std::string Le = newLabel();
    emit("label", Ls, "-");
    if (n->children.size() > 1 && n->children[1] && n->children[1]->kind == "ForCond") {
      const auto& cwrap = n->children[1];
      if (!cwrap->children.empty()) {
        SemType ct;
        std::string cond = genExpr(cwrap->children[0], ct, 1, 1);
        emit("if_false", cond, Le);
      }
    }
    breakLabels_.push_back(Le);
    if (n->children.size() > 3) { visitStmt(n->children[3]); }
    breakLabels_.pop_back();
    if (n->children.size() > 2 && n->children[2] && n->children[2]->kind == "ForStep") {
      const auto& sw = n->children[2];
      for (const auto& s : sw->children) {
        SemType st;
        genExpr(s, st, 1, 1);
      }
    }
    emit("goto", Ls, "-");
    emit("label", Le, "-");
    popScope();
  }

  std::vector<std::string> breakLabels_;

  std::string genExpr(const std::shared_ptr<AstNode>& n, SemType& ty, std::size_t line, std::size_t col) {
    ty = SemType::Unknown;
    if (!n) { return "-"; }
    if (n->kind == "LiteralExpr") {
      const std::string k = attr(*n, "kind");
      const std::string v = attr(*n, "value");
      if (k == "int") {
        ty = SemType::Int;
      } else if (k == "float") {
        ty = SemType::Double;
      } else if (k == "bool") {
        ty = SemType::Bool;
      } else if (k == "string") {
        ty = SemType::String;
      } else if (k == "char") {
        ty = SemType::Char;
      }
      return v;
    }
    if (n->kind == "IdExpr") {
      const std::string name = attr(*n, "name");
      ty = lookupVarType(name, line, col);
      return name;
    }
    if (n->kind == "ParenExpr") {
      if (!n->children.empty()) { return genExpr(n->children[0], ty, line, col); }
      return "-";
    }
    if (n->kind == "UnaryExpr") {
      const std::string op = attr(*n, "op");
      SemType ct;
      std::string x = genExpr(n->children[0], ct, line, col);
      int k = emit(op, x, "-");
      ty = ct;
      return triadRef(k);
    }
    if (n->kind == "PostfixExpr") {
      const std::string op = attr(*n, "op");
      SemType ct;
      std::string x = genExpr(n->children[0], ct, line, col);
      int k = emit(std::string("post") + op, x, "-");
      ty = ct == SemType::Unknown ? SemType::Int : ct;
      return triadRef(k);
    }
    if (n->kind == "BinaryExpr") {
      const std::string op = attr(*n, "op");
      SemType lt;
      SemType rt;
      std::string L = genExpr(n->children[0], lt, line, col);
      std::string R = genExpr(n->children[1], rt, line, col);

      if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "^=" ||
          op == "|=" || op == "&=") {
        std::string lhsName = extractAssignableName(n->children[0]);
        if (lhsName.empty()) {
          error("LVALUE", "Левая часть присваивания должна быть изменяемым идентификатором", line, col);
          ty = lt;
          return L;
        }
        SemType ltype = lookupVarType(lhsName, line, col);
        if (op == "=") {
          if (!assignCompatible(ltype, rt)) {
            error("TYPE_MISMATCH", "Тип правой части присваивания не совместим с типом «" + lhsName + "»", line, col);
          }
          int k = emit(":=", lhsName, R);
          markVarInitialized(lhsName, line, col);
          ty = ltype;
          return triadRef(k);
        }
        SemType pr = promote(ltype, rt);
        if (pr == SemType::Unknown || (!isNumeric(ltype) && op != "=")) {
          error("TYPE_MISMATCH", "Недопустимые типы для составного присваивания «" + op + "»", line, col);
        }
        int k = emit(op, lhsName, R);
        markVarInitialized(lhsName, line, col);
        ty = ltype;
        return triadRef(k);
      }

      if (op == "&&" || op == "||") {
        if ((lt != SemType::Bool && lt != SemType::Int && lt != SemType::Unknown) ||
            (rt != SemType::Bool && rt != SemType::Int && rt != SemType::Unknown)) {
          error("TYPE_MISMATCH", "Операнды логических операций должны быть bool или int", line, col);
        }
        ty = SemType::Bool;
        int k = emit(op, L, R);
        return triadRef(k);
      }
      if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
        ty = SemType::Bool;
        int k = emit(op, L, R);
        return triadRef(k);
      }
      if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%" || op == "&" || op == "|" || op == "^" ||
          op == "<<" || op == ">>") {
        if (!isNumeric(lt) || !isNumeric(rt)) {
          error("TYPE_MISMATCH", "Арифметические/побитовые операции допустимы только для числовых типов", line, col);
        }
        ty = promote(lt, rt);
        int k = emit(op, L, R);
        return triadRef(k);
      }
      int k = emit(op, L, R);
      ty = promote(lt, rt);
      return triadRef(k);
    }
    if (n->kind == "ConditionalExpr") {
      SemType t1, t2, t3;
      std::string c = genExpr(n->children[0], t1, line, col);
      const std::string Lf = newLabel();
      const std::string Le = newLabel();
      const std::string tmp = newTemp();
      emit("if_false", c, Lf);
      std::string a = genExpr(n->children[1], t2, line, col);
      emit(":=", tmp, a);
      emit("goto", Le, "-");
      emit("label", Lf, "-");
      std::string b = genExpr(n->children[2], t3, line, col);
      emit(":=", tmp, b);
      emit("label", Le, "-");
      ty = (t2 == SemType::Double || t3 == SemType::Double) ? SemType::Double : SemType::Int;
      if (t2 != t3 && !(assignCompatible(t2, t3) || assignCompatible(t3, t2))) {
        error("TYPE_MISMATCH", "Несовместимые типы веток условного оператора ?: ", line, col);
      }
      return tmp;
    }
    if (n->kind == "CastExpr") {
      const std::string style = attr(*n, "style");
      const SemType castTy = parseDataType(attr(*n, "type"));
      SemType innerT;
      std::string inner = genExpr(n->children[0], innerT, line, col);
      int k = emit(std::string("cast_") + style, semTypeName(castTy), inner);
      ty = castTy;
      return triadRef(k);
    }
    if (n->kind == "CallExpr") {
      if (n->children.empty()) {
        error("CALL", "Некорректный вызов функции", line, col);
        return "-";
      }
      std::string callee;
      if (n->children[0] && n->children[0]->kind == "IdExpr") {
        callee = attr(*n->children[0], "name");
      } else {
        SemType ct;
        callee = genExpr(n->children[0], ct, line, col);
      }
      std::vector<std::string> args;
      for (std::size_t i = 1; i < n->children.size(); ++i) {
        SemType at;
        args.push_back(genExpr(n->children[i], at, line, col));
      }
      auto fit = funcs_.find(callee);
      if (fit == funcs_.end()) {
        error("UNDEFINED_FUNC", "Вызов необъявленной функции «" + callee + "»", line, col);
      } else if (callee != "printf") {
        if (fit->second.params.size() != args.size()) {
          error("ARGCOUNT", "Неверное число аргументов при вызове «" + callee + "»", line, col);
        }
      }
      std::ostringstream oss;
      for (std::size_t i = 0; i < args.size(); ++i) {
        if (i) { oss << '|'; }
        oss << args[i];
      }
      int k = emit("call", callee, oss.str());
      ty = SemType::Void;
      if (fit != funcs_.end()) { ty = fit->second.ret; }
      return triadRef(k);
    }

    ty = SemType::Unknown;
    return "-";
  }

  static std::string extractAssignableName(const std::shared_ptr<AstNode>& n) {
    if (!n) { return {}; }
    if (n->kind == "IdExpr") { return attr(*n, "name"); }
    if (n->kind == "ParenExpr" && !n->children.empty()) { return extractAssignableName(n->children[0]); }
    return {};
  }
};

}  // namespace

SemanticResult analyzeSemantics(const std::shared_ptr<AstNode>& root) {
  SemanticResult r;
  Analyzer a(r);
  a.process(root);
  return r;
}

static std::string yn(bool v) { return v ? "+" : "-"; }

static std::string friendlyType(const std::string& t) {
  if (t == "int") { return "integer"; }
  if (t == "double") { return "real"; }
  if (t == "bool") { return "boolean"; }
  if (t == "void") { return "void"; }
  if (t == "string") { return "string"; }
  if (t == "char") { return "char"; }
  return t;
}

void printSymbolTable(std::ostream& os, const SemanticResult& r) {
  os << "Таблица символов\n";
  os << "Имя\tТип\tОбъявлена\tИнициализирована\n";
  for (const auto& s : r.symbols) {
    os << s.name << '\t' << friendlyType(s.type) << '\t' << yn(s.declared) << '\t' << yn(s.initialized) << '\n';
  }
}

void printTriads(std::ostream& os, const SemanticResult& r) {
  for (std::size_t i = 0; i < r.triads.size(); ++i) {
    const Triad& t = r.triads[i];
    os << (i + 1) << ") (" << t.op << ", " << t.arg1 << ", " << t.arg2 << ")\n";
  }
}
