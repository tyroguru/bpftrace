#include "ast/ast.h"

#include <algorithm>

#include "ast/visitor.h"
#include "log.h"

namespace bpftrace::ast {

static constexpr std::string_view ENUM = "enum ";

Integer::Integer(int64_t n, location loc, bool is_negative)
    : Expression(loc), n(n), is_negative(is_negative)
{
  is_literal = true;
}

String::String(const std::string &str, location loc) : Expression(loc), str(str)
{
  is_literal = true;
}

StackMode::StackMode(const std::string &mode, location loc)
    : Expression(loc), mode(mode)
{
  is_literal = true;
}

Builtin::Builtin(const std::string &ident, location loc)
    : Expression(loc), ident(is_deprecated(ident))
{
}

Identifier::Identifier(const std::string &ident, location loc)
    : Expression(loc), ident(ident)
{
}

PositionalParameter::PositionalParameter(PositionalParameterType ptype,
                                         long n,
                                         location loc)
    : Expression(loc), ptype(ptype), n(n)
{
  is_literal = true;
}

Call::Call(const std::string &func, location loc)
    : Expression(loc), func(is_deprecated(func))
{
}

Call::Call(const std::string &func, ExpressionList &&vargs, location loc)
    : Expression(loc), func(is_deprecated(func)), vargs(std::move(vargs))
{
}

Sizeof::Sizeof(SizedType type, location loc) : Expression(loc), argtype(type)
{
}

Sizeof::Sizeof(Expression *expr, location loc) : Expression(loc), expr(expr)
{
}

Offsetof::Offsetof(SizedType record,
                   std::vector<std::string> &field,
                   location loc)
    : Expression(loc), record(record), field(field)
{
}

Offsetof::Offsetof(Expression *expr,
                   std::vector<std::string> &field,
                   location loc)
    : Expression(loc), expr(expr), field(field)
{
}

Map::Map(const std::string &ident, location loc) : Expression(loc), ident(ident)
{
  is_map = true;
}

Map::Map(const std::string &ident, Expression &expr, location loc)
    : Expression(loc), ident(ident), key_expr(&expr)
{
  is_map = true;
  key_expr->key_for_map = this;
}

Variable::Variable(const std::string &ident, location loc)
    : Expression(loc), ident(ident)
{
  is_variable = true;
}

Binop::Binop(Expression *left, Operator op, Expression *right, location loc)
    : Expression(loc), left(left), right(right), op(op)
{
}

Unop::Unop(Operator op, Expression *expr, bool is_post_op, location loc)
    : Expression(loc), expr(expr), op(op), is_post_op(is_post_op)
{
}

Ternary::Ternary(Expression *cond,
                 Expression *left,
                 Expression *right,
                 location loc)
    : Expression(loc), cond(cond), left(left), right(right)
{
}

FieldAccess::FieldAccess(Expression *expr,
                         const std::string &field,
                         location loc)
    : Expression(loc), expr(expr), field(field)
{
}

FieldAccess::FieldAccess(Expression *expr, ssize_t index, location loc)
    : Expression(loc), expr(expr), index(index)
{
}

ArrayAccess::ArrayAccess(Expression *expr, Expression *indexpr, location loc)
    : Expression(loc), expr(expr), indexpr(indexpr)
{
}

Cast::Cast(SizedType cast_type, Expression *expr, location loc)
    : Expression(loc), expr(expr)
{
  type = cast_type;
}

Tuple::Tuple(ExpressionList &&elems, location loc)
    : Expression(loc), elems(std::move(elems))
{
}

ExprStatement::ExprStatement(Expression *expr, location loc)
    : Statement(loc), expr(expr)
{
}

AssignMapStatement::AssignMapStatement(Map *map, Expression *expr, location loc)
    : Statement(loc), map(map), expr(expr)
{
  expr->map = map;
};

AssignVarStatement::AssignVarStatement(Variable *var,
                                       Expression *expr,
                                       location loc)
    : Statement(loc), var(var), expr(expr)
{
  expr->var = var;
}

AssignVarStatement::AssignVarStatement(VarDeclStatement *var_decl_stmt,
                                       Expression *expr,
                                       location loc)
    : Statement(loc),
      var_decl_stmt(var_decl_stmt),
      var(var_decl_stmt->var),
      expr(expr)
{
  expr->var = var;
}

AssignConfigVarStatement::AssignConfigVarStatement(
    const std::string &config_var,
    Expression *expr,
    location loc)
    : Statement(loc), config_var(config_var), expr(expr)
{
}

VarDeclStatement::VarDeclStatement(Variable *var, SizedType type, location loc)
    : Statement(loc), var(var), set_type(true)
{
  var->type = std::move(type);
}

VarDeclStatement::VarDeclStatement(Variable *var, location loc)
    : Statement(loc), var(var)
{
  var->type = CreateNone();
}

Predicate::Predicate(Expression *expr, location loc) : Node(loc), expr(expr)
{
}

AttachPoint::AttachPoint(const std::string &raw_input,
                         bool ignore_invalid,
                         location loc)
    : Node(loc), raw_input(raw_input), ignore_invalid(ignore_invalid)
{
}

Block::Block(StatementList &&stmts, location loc)
    : Statement(loc), stmts(std::move(stmts))
{
}

If::If(Expression *cond, Block *if_block, Block *else_block, location loc)
    : Statement(loc), cond(cond), if_block(if_block), else_block(else_block)
{
}

Unroll::Unroll(Expression *expr, Block *block, location loc)
    : Statement(loc), expr(expr), block(block)
{
}

Probe::Probe(AttachPointList &&attach_points,
             Predicate *pred,
             Block *block,
             location loc)
    : Node(loc),
      attach_points(std::move(attach_points)),
      pred(pred),
      block(block)
{
}

SubprogArg::SubprogArg(std::string name, SizedType type, location loc)
    : Node(loc), type(std::move(type)), name_(std::move(name))
{
}

std::string SubprogArg::name() const
{
  return name_;
}

Subprog::Subprog(std::string name,
                 SizedType return_type,
                 SubprogArgList &&args,
                 StatementList &&stmts,
                 location loc)
    : Node(loc),
      args(std::move(args)),
      return_type(std::move(return_type)),
      stmts(std::move(stmts)),
      name_(std::move(name))
{
}

Program::Program(const std::string &c_definitions,
                 Config *config,
                 SubprogList &&functions,
                 ProbeList &&probes,
                 location loc)
    : Node(loc),
      c_definitions(c_definitions),
      config(config),
      functions(std::move(functions)),
      probes(std::move(probes))
{
}

std::string opstr(const Jump &jump)
{
  switch (jump.ident) {
    case JumpType::RETURN:
      return "return";
    case JumpType::BREAK:
      return "break";
    case JumpType::CONTINUE:
      return "continue";
    default:
      return {};
  }

  return {}; // unreached
}

std::string opstr(const Binop &binop)
{
  switch (binop.op) {
    case Operator::EQ:
      return "==";
    case Operator::NE:
      return "!=";
    case Operator::LE:
      return "<=";
    case Operator::GE:
      return ">=";
    case Operator::LT:
      return "<";
    case Operator::GT:
      return ">";
    case Operator::LAND:
      return "&&";
    case Operator::LOR:
      return "||";
    case Operator::LEFT:
      return "<<";
    case Operator::RIGHT:
      return ">>";
    case Operator::PLUS:
      return "+";
    case Operator::MINUS:
      return "-";
    case Operator::MUL:
      return "*";
    case Operator::DIV:
      return "/";
    case Operator::MOD:
      return "%";
    case Operator::BAND:
      return "&";
    case Operator::BOR:
      return "|";
    case Operator::BXOR:
      return "^";
    default:
      return {};
  }

  return {}; // unreached
}

std::string opstr(const Unop &unop)
{
  switch (unop.op) {
    case Operator::LNOT:
      return "!";
    case Operator::BNOT:
      return "~";
    case Operator::MINUS:
      return "-";
    case Operator::MUL:
      return "dereference";
    case Operator::INCREMENT:
      if (unop.is_post_op)
        return "++ (post)";
      return "++ (pre)";
    case Operator::DECREMENT:
      if (unop.is_post_op)
        return "-- (post)";
      return "-- (pre)";
    default:
      return {};
  }

  return {}; // unreached
}

AttachPoint &AttachPoint::create_expansion_copy(ASTContext &ctx,
                                                const std::string &match) const
{
  // Create a new node with the same raw tracepoint. We initialize all the
  // information about the attach point, and then override/reset values
  // depending on the specific probe type.
  auto &ap = *ctx.make_node<AttachPoint>(raw_input, ignore_invalid, loc);
  ap.index_ = index_;
  ap.provider = provider;
  ap.target = target;
  ap.lang = lang;
  ap.ns = ns;
  ap.func = func;
  ap.pin = pin;
  ap.usdt = usdt;
  ap.freq = freq;
  ap.len = len;
  ap.mode = mode;
  ap.async = async;
  ap.expansion = expansion;
  ap.address = address;
  ap.func_offset = func_offset;

  switch (probetype(ap.provider)) {
    case ProbeType::kprobe:
    case ProbeType::kretprobe:
      ap.func = match;
      if (match.find(":") != std::string::npos)
        ap.target = erase_prefix(ap.func);
      break;
    case ProbeType::uprobe:
    case ProbeType::uretprobe:
    case ProbeType::fentry:
    case ProbeType::fexit:
    case ProbeType::tracepoint:
      // Tracepoint, uprobe, and fentry/fexit probes specify both a target
      // (category for tracepoints, binary for uprobes, and kernel module
      // for fentry/fexit and a function name.
      ap.func = match;
      ap.target = erase_prefix(ap.func);
      break;
    case ProbeType::usdt:
      // USDT probes specify a target binary path, a provider, and a function
      // name.
      ap.func = match;
      ap.target = erase_prefix(ap.func);
      ap.ns = erase_prefix(ap.func);
      break;
    case ProbeType::watchpoint:
    case ProbeType::asyncwatchpoint:
      // Watchpoint probes come with target prefix. Strip the target to get the
      // function
      ap.func = match;
      erase_prefix(ap.func);
      break;
    case ProbeType::rawtracepoint:
      ap.func = match;
      break;
    case ProbeType::software:
    case ProbeType::hardware:
    case ProbeType::interval:
    case ProbeType::profile:
    case ProbeType::special:
    case ProbeType::iter:
    case ProbeType::invalid:
      break;
    default:
      LOG(BUG) << "Unknown probe type";
  }
  return ap;
}

std::string AttachPoint::name() const
{
  std::string n = provider;
  if (target != "")
    n += ":" + target;
  if (lang != "")
    n += ":" + lang;
  if (ns != "")
    n += ":" + ns;
  if (func != "") {
    n += ":" + func;
    if (func_offset != 0)
      n += "+" + std::to_string(func_offset);
  }
  if (address != 0)
    n += ":" + std::to_string(address);
  if (freq != 0)
    n += ":" + std::to_string(freq);
  if (len != 0)
    n += ":" + std::to_string(len);
  if (mode.size())
    n += ":" + mode;
  return n;
}

int AttachPoint::index() const
{
  return index_;
}

void AttachPoint::set_index(int index)
{
  index_ = index;
}

std::string Probe::name() const
{
  std::vector<std::string> ap_names;
  std::transform(attach_points.begin(),
                 attach_points.end(),
                 std::back_inserter(ap_names),
                 [](const AttachPoint *ap) { return ap->name(); });
  return str_join(ap_names, ",");
}

std::string Probe::args_typename() const
{
  return "struct " + name() + "_args";
}

int Probe::index() const
{
  return index_;
}

void Probe::set_index(int index)
{
  index_ = index;
}

std::string Subprog::name() const
{
  return name_;
}

bool Probe::has_ap_of_probetype(ProbeType probe_type)
{
  for (auto *ap : attach_points) {
    if (probetype(ap->provider) == probe_type)
      return true;
  }
  return false;
}

SizedType ident_to_record(const std::string &ident, int pointer_level)
{
  SizedType result = CreateRecord(ident, std::weak_ptr<Struct>());
  for (int i = 0; i < pointer_level; i++)
    result = CreatePointer(result);
  return result;
}

SizedType ident_to_sized_type(const std::string &ident)
{
  if (ident.starts_with(ENUM)) {
    auto enum_name = ident.substr(ENUM.size());
    // This is an automatic promotion to a uint64
    // even though it's possible that highest variant value of that enum
    // fits into a smaller int. This will also affect casts from a smaller
    // int and cause an ERROR: Integer size mismatch.
    // This could potentially be revisited or the cast relaxed
    // if we check the variant values during semantic analysis.
    return CreateEnum(64, enum_name);
  }
  return ident_to_record(ident);
}

} // namespace bpftrace::ast
