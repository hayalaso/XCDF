/*
Copyright (c) 2014, University of Maryland
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <xcdf/utility/Expression.h>
#include <xcdf/utility/NodeDefs.h>
#include <xcdf/utility/FieldNodeDefs.h>
#include <sstream>
#include <cctype>

void
Expression::Init() {

  ParseSymbols(expString_);

  if (parsedSymbols_.size() == 0) {
    XCDFFatal("No evaluation expression");
  }

  std::list<Symbol*>::iterator start = parsedSymbols_.begin();
  std::list<Symbol*>::iterator end = parsedSymbols_.end();

  RecursiveParseExpression(start, end);
  if (distance(start, end) != 1) {
    XCDFFatal("Invalid expression: " << expString_);
  }
}

Expression::~Expression() {

  // Delete the symbols we've allocated
  for (std::vector<Symbol*>::iterator it = allocatedSymbols_.begin();
                                      it != allocatedSymbols_.end(); ++it) {
    delete *it;
  }
}

Expression&
Expression::operator=(Expression e) {

  // Copy & swap
  std::swap(f_, e.f_);
  std::swap(expString_, e.expString_);
  std::swap(allocatedSymbols_, e.allocatedSymbols_);
  std::swap(parsedSymbols_, e.parsedSymbols_);
  return *this;
}

void
Expression::ParseSymbols(const std::string& exp) {

  size_t pos = 0;
  for (Symbol* s = GetNextSymbol(exp, pos);
               s != NULL; s = GetNextSymbol(exp, pos)) {

    parsedSymbols_.push_back(s);
    allocatedSymbols_.push_back(s);
  }
}

Symbol*
Expression::GetNextSymbol(const std::string& exp, size_t& pos) {

  // Advance to the next non-whitespace character
  pos = exp.find_first_not_of(" \n\r\t", pos);

  if (pos == std::string::npos) {
    // Nothing left
    return NULL;
  }

  // Get the position of next operator character
  size_t operpos = exp.find_first_of(",/*%^)(=><&|!~", pos);

  if (pos != operpos) {

    // A value, field, function name, or operators + or -
    return ParseValue(exp, pos, operpos);

  } else {

    // An operator.
    return ParseOperator(exp, pos);
  }
}

Symbol*
Expression::ParseValue(const std::string& exp,
                       size_t& pos,
                       size_t operpos) const {

  size_t startpos = pos;
  size_t endpos = exp.find_last_not_of(" \n\r\t", operpos - 1);
  std::string valueString = exp.substr(startpos, endpos - startpos + 1);

  // If leading +/- is an operator, we have to deal with it here
  if (valueString.find_first_of("+-") == 0) {

    // Need to deal with + or - symbols.  If previous symbol is a
    // value or ')' and first character is +/-, it is an operator.
    if (parsedSymbols_.size() > 0 &&
                  (parsedSymbols_.back()->IsNode() ||
                   parsedSymbols_.back()->GetType() == CLOSE_PARAND)) {
        return ParseOperator(exp, pos);
    }
  }

  // There is at least one value at the front of this expression.  Try
  // parsing the largest value possible and iteratively reducing the
  // scope on failure
  while (endpos != std::string::npos && endpos >= startpos) {
    std::string testString = exp.substr(startpos, endpos - startpos + 1);
    Symbol* val = ParseValueImpl(testString);
    if (val) {

      // Check validity
      if (val->IsFunction()) {

        // Expect "(" next
        if (operpos == std::string::npos) {
          XCDFFatal("Missing \"(\" after " << *val);
        } else if (exp[operpos] != '(') {
          XCDFFatal("Missing \"(\" after " << *val);
        }
      }

      pos = endpos + 1;
      return val;
    }

    // Next loop: skip to before last +/-
    endpos = exp.find_last_of("+-", endpos);
    if (endpos != std::string::npos) {
      // We're at the +/-.  Jump to last valid character in front of it
      endpos = exp.find_last_not_of("+- \n\r\t", endpos);
    }
  }

  // Parsing failure
  XCDFError("Cannot parse expression \"" << exp << "\"");
  std::string ss = "";
  for (unsigned i = 1; i < pos; ++i) {
    ss += " ";
  }
  ss += "^";
  XCDFError("                         " << ss);
  XCDFFatal("");
  return NULL;
}

Symbol*
Expression::ParseOperator(const std::string& exp,
                          size_t& pos) const {

  // An operator.
  size_t startpos = pos;
  size_t endpos = startpos;

  // Parenthesis must be treated alone
  if (exp[startpos] != '(' && exp[startpos] != ')') {

    // Find the end of the operator string
    while (exp.find_first_of(",/*%^=><&|!~", endpos + 1) == endpos + 1) {
      ++endpos;
    }
  }
  pos = endpos + 1;
  Symbol* op = ParseOperatorImpl(exp.substr(startpos, pos - startpos));
  if (!op) {
    // Parsing failure
    XCDFError("Cannot parse expression \"" << exp << "\"");
    std::string ss = "";
    for (unsigned i = 1; i < pos; ++i) {
      ss += " ";
    }
    ss += "^";
    XCDFError("                         " << ss);
    XCDFFatal("");
  }
  return op;
}

template <typename T, typename M>
Symbol* DoConstNode(const std::string& numerical, M manip) {
  std::stringstream ss(numerical);
  ss << manip;
  T out;
  ss >> out;
  if (ss.fail()) {
    return NULL;
  }
  // Make sure there are no unconverted characters
  std::string s;
  ss >> s;
  if (s.size() > 0) {
    return NULL;
  }
  return new ConstNode<T>(out);
}

Symbol*
Expression::ParseNumerical(const std::string& numerical) const {

  Symbol* out = NULL;
  // Parse hex only if we have leading x or X
  char hex[2] = {'X', 'x'};
  if (numerical.find(hex) != std::string::npos) {
    out = DoConstNode<uint64_t>(numerical, std::hex);
  }
  if (!out) {
    out = DoConstNode<uint64_t>(numerical, std::dec);
  }
  if (!out) {
    out = DoConstNode<int64_t>(numerical, std::dec);
  }
  if (!out) {
    out = DoConstNode<double>(numerical, std::dec);
  }
  return out;
}

Symbol*
Expression::ParseValueImpl(std::string exp) const {

  // First try string as a field
  if (f_->HasField(exp)) {

    // An XCDF field
    if (f_->IsUnsignedIntegerField(exp)) {
      return new FieldNode<uint64_t>(f_->GetUnsignedIntegerField(exp));
    }

    if (f_->IsSignedIntegerField(exp)) {
      return new FieldNode<int64_t>(f_->GetSignedIntegerField(exp));
    }

    return new FieldNode<double>(f_->GetFloatingPointField(exp));
  }

  // Next try string as an alias
  if (f_->HasAlias(exp)) {

    // An XCDF alias
    if (f_->IsUnsignedIntegerAlias(exp)) {
      return new AliasNode<uint64_t>(f_->GetUnsignedIntegerAlias(exp));
    }

    if (f_->IsSignedIntegerAlias(exp)) {
      return new AliasNode<int64_t>(f_->GetSignedIntegerAlias(exp));
    }

    return new AliasNode<double>(f_->GetFloatingPointAlias(exp));
  }

  // "currentEventNumber" refers to the event count and is reserved
  if (!exp.compare("currentEventNumber")) {
    return new CounterNode(*f_);
  }

  // Try to parse as a numerical value
  Symbol* numerical = ParseNumerical(exp);
  if (numerical) {
    return numerical;
  }

  // Custom function to compare against a list of nodes
  if (!exp.compare("in")) {
    return new Symbol(IN);
  }

  // Custom functions for vector data
  // Return number of unique elements in a vector
  if (!exp.compare("unique")) {
    return new Symbol(UNIQUE);
  }

  // Return true if any element is true
  if (!exp.compare("any")) {
    return new Symbol(ANY);
  }

  // Return true only if all elements are true
  if (!exp.compare("all")) {
    return new Symbol(ALL);
  }

  // Return the sum of the elements
  if (!exp.compare("sum")) {
    return new Symbol(SUM);
  }

  if (!exp.compare("sin")) {
    return new Symbol(SIN);
  }

  if (!exp.compare("cos")) {
    return new Symbol(COS);
  }

  if (!exp.compare("tan")) {
    return new Symbol(TAN);
  }

  if (!exp.compare("asin")) {
    return new Symbol(ASIN);
  }

  if (!exp.compare("acos")) {
    return new Symbol(ACOS);
  }

  if (!exp.compare("atan")) {
    return new Symbol(ATAN);
  }

  if (!exp.compare("log")) {
    return new Symbol(LOG);
  }

  if (!exp.compare("log10")) {
    return new Symbol(LOG10);
  }

  if (!exp.compare("exp")) {
    return new Symbol(EXP);
  }

  if (!exp.compare("abs")) {
    return new Symbol(ABS);
  }

  if (!exp.compare("fabs")) {
    return new Symbol(ABS);
  }

  if (!exp.compare("sqrt")) {
    return new Symbol(SQRT);
  }

  if (!exp.compare("ceil")) {
    return new Symbol(CEIL);
  }

  if (!exp.compare("floor")) {
    return new Symbol(FLOOR);
  }

  if (!exp.compare("isnan")) {
    return new Symbol(ISNAN);
  }

  if (!exp.compare("isinf")) {
    return new Symbol(ISINF);
  }

  if (!exp.compare("sinh")) {
    return new Symbol(SINH);
  }

  if (!exp.compare("cosh")) {
    return new Symbol(COSH);
  }

  if (!exp.compare("tanh")) {
    return new Symbol(TANH);
  }

  if (!exp.compare("rand")) {
    return new Symbol(RAND);
  }

  if (!exp.compare("fmod")) {
    return new Symbol(FMOD);
  }

  if (!exp.compare("pow")) {
    return new Symbol(POW);
  }

  if (!exp.compare("int")) {
    return new Symbol(INT);
  }

  if (!exp.compare("unsigned")) {
    return new Symbol(UNSIGNED);
  }

  if (!exp.compare("float")) {
    return new Symbol(DOUBLE);
  }

  if (!exp.compare("double")) {
    return new Symbol(DOUBLE);
  }

  if (!exp.compare("atan2")) {
    return new Symbol(ATAN2);
  }

  if (!exp.compare("true")) {
    return new ConstNode<uint64_t>(1);
  }

  if (!exp.compare("false")) {
    return new ConstNode<uint64_t>(0);
  }

  return NULL;
}

Symbol*
Expression::ParseOperatorImpl(std::string exp) const {

  if (!exp.compare("+")) {
    return new Symbol(ADDITION);
  } else if (!exp.compare("-")) {
    return new Symbol(SUBTRACTION);
  } else if (!exp.compare("*")) {
    return new Symbol(MULTIPLICATION);
  } else if (!exp.compare("/")) {
    return new Symbol(DIVISION);
  } else if (!exp.compare("%")) {
    return new Symbol(MODULUS);
  } else if (!exp.compare("^")) {
    return new Symbol(POWER);
  } else if (!exp.compare("(")) {
    return new Symbol(OPEN_PARAND);
  } else if (!exp.compare(")")) {
    return new Symbol(CLOSE_PARAND);
  } else if (!exp.compare("==")) {
    return new Symbol(EQUALITY);
  } else if (!exp.compare("!=")) {
    return new Symbol(INEQUALITY);
  } else if (!exp.compare(">")) {
    return new Symbol(GREATER_THAN);
  } else if (!exp.compare("<")) {
    return new Symbol(LESS_THAN);
  } else if (!exp.compare(">=")) {
    return new Symbol(GREATER_THAN_EQUAL);
  } else if (!exp.compare("<=")) {
    return new Symbol(LESS_THAN_EQUAL);
  } else if (!exp.compare("||")) {
    return new Symbol(LOGICAL_OR);
  } else if (!exp.compare("&&")) {
    return new Symbol(LOGICAL_AND);
  } else if (!exp.compare("|")) {
    return new Symbol(BITWISE_OR);
  } else if (!exp.compare("&")) {
    return new Symbol(BITWISE_AND);
  } else if (!exp.compare("!")) {
    return new Symbol(LOGICAL_NOT);
  } else if (!exp.compare("~")) {
    return new Symbol(BITWISE_NOT);
  } else if (!exp.compare(",")) {
    return new Symbol(COMMA);
  } else {
    return NULL;
  }
}

void
Expression::RecursiveParseExpression(std::list<Symbol*>::iterator& start,
                                     std::list<Symbol*>::iterator& end) {

  // Is anything here?
  if (start == end) {
    return;
  }

  // Scan for parenthesis
  while (ReplaceParenthesis(start, end)) { }

  // No more parenthesis -- just apply the operators
  // left to right in order of precedence
  ReplaceFunctions(start, end);
  ReplaceUnary(start, end);
  ReplaceMultiplyDivideModulus(start, end);
  ReplaceAdditionSubtraction(start, end);
  ReplaceComparison(start, end);
  ReplaceBitwise(start, end);
  ReplaceLogical(start, end);
  ReplaceCommas(start, end);
}

bool
Expression::ReplaceParenthesis(std::list<Symbol*>::iterator& start,
                               std::list<Symbol*>::iterator& end) {

  // Scan for parenthesis
  std::list<Symbol*>::iterator firstOpenParand = end;
  std::list<Symbol*>::iterator closeParand = end;

  int nOpen = 0;
  for (std::list<Symbol*>::iterator it = start; it != end; ++it) {
    if ((*it)->GetType() == OPEN_PARAND) {
      nOpen++;
      if (firstOpenParand == end) {
        firstOpenParand = it;
      }
    } else if ((*it)->GetType() == CLOSE_PARAND) {
      nOpen--;
      if (nOpen == 0) {
        closeParand = it;
        break;

      } else if (nOpen < 0) {
        XCDFFatal("Found unpaired \")\"");
      }
    }
  }

  // Check sanity
  if (nOpen > 0) {
    XCDFFatal("Found unpaired \"(\"");
  }

  // Do we have parenthesis?
  if (firstOpenParand == end) {
    return false;
  }

  // Parse what is inside the parenthesis
  ++firstOpenParand;
  RecursiveParseExpression(firstOpenParand, closeParand);


  // Remove the parenthesis
  ReplaceSymbols(NULL, --firstOpenParand, 1, start);
  ReplaceSymbols(NULL, closeParand, 1, start);
  return true;
}

void
Expression::ReplaceFunctions(std::list<Symbol*>::iterator& start,
                             std::list<Symbol*>::iterator& end) {

  for (std::list<Symbol*>::iterator it = start; it != end; ++it) {

    if ((*it)->IsUnaryFunction()) {
      Symbol* s = GetUnarySymbol(start, end, it, (*it)->GetType(), true);
      it = ReplaceSymbols(s, it, 2, start);
    }

    if ((*it)->IsVoidFunction()) {
      Symbol* s = GetVoidSymbol(start, end, it, (*it)->GetType());
      it = ReplaceSymbols(s, it, 1, start);
    }


    if ((*it)->IsBinaryFunction()) {
      Symbol* s = GetBinarySymbol(start, end, it, (*it)->GetType(), true);
      it = ReplaceSymbols(s, it, 2, start);
    }

    if ((*it)->GetType() == POWER) {
      Symbol* s = GetBinarySymbol(start, end, it, (*it)->GetType(), false);
      it = ReplaceSymbols(s, --it, 3, start);
    }
  }
}

void
Expression::ReplaceUnary(std::list<Symbol*>::iterator& start,
                         std::list<Symbol*>::iterator& end) {

  for (std::list<Symbol*>::iterator it = start; it != end; ++it) {

    if ((*it)->GetType() == LOGICAL_NOT ||
        (*it)->GetType() == BITWISE_NOT) {
      Symbol* s = GetUnarySymbol(start, end, it, (*it)->GetType(), false);
      it = ReplaceSymbols(s, it, 2, start);
    }
  }
}

void
Expression::ReplaceMultiplyDivideModulus(std::list<Symbol*>::iterator& start,
                                         std::list<Symbol*>::iterator& end) {

  for (std::list<Symbol*>::iterator it = start; it != end; ++it) {

    if ((*it)->GetType() == MULTIPLICATION ||
        (*it)->GetType() == DIVISION ||
        (*it)->GetType() == MODULUS) {

      Symbol* s = GetBinarySymbol(start, end, it, (*it)->GetType(), false);
      it = ReplaceSymbols(s, --it, 3, start);
    }
  }
}

void
Expression::ReplaceAdditionSubtraction(std::list<Symbol*>::iterator& start,
                                       std::list<Symbol*>::iterator& end) {

  for (std::list<Symbol*>::iterator it = start; it != end; ++it) {

    if ((*it)->GetType() == ADDITION ||
        (*it)->GetType() == SUBTRACTION) {

      Symbol* s = GetBinarySymbol(start, end, it, (*it)->GetType(), false);
      it = ReplaceSymbols(s, --it, 3, start);
    }
  }
}

void
Expression::ReplaceComparison(std::list<Symbol*>::iterator& start,
                              std::list<Symbol*>::iterator& end) {

  // Start with comparison
  for (std::list<Symbol*>::iterator it = start; it != end; ++it) {

    if ((*it)->IsComparison()) {
      Symbol* s = GetBinarySymbol(start, end, it, (*it)->GetType(), false);
      it = ReplaceSymbols(s, --it, 3, start);
    }
  }

  // Now equality
  for (std::list<Symbol*>::iterator it = start; it != end; ++it) {

    if ((*it)->IsEquality()) {
      Symbol* s = GetBinarySymbol(start, end, it, (*it)->GetType(), false);
      it = ReplaceSymbols(s, --it, 3, start);
    }
  }
}

void
Expression::ReplaceBitwise(std::list<Symbol*>::iterator& start,
                           std::list<Symbol*>::iterator& end) {

  // Replace AND first
  for (std::list<Symbol*>::iterator it = start; it != end; ++it) {

    if ((*it)->GetType() == BITWISE_AND) {

      Symbol* s = GetBinarySymbol(start, end, it, (*it)->GetType(), false);
      it = ReplaceSymbols(s, --it, 3, start);
    }
  }

  // Last is OR
  for (std::list<Symbol*>::iterator it = start; it != end; ++it) {

    if ((*it)->GetType() == BITWISE_OR) {

      Symbol* s = GetBinarySymbol(start, end, it, (*it)->GetType(), false);
      it = ReplaceSymbols(s, --it, 3, start);
    }
  }
}

void
Expression::ReplaceLogical(std::list<Symbol*>::iterator& start,
                           std::list<Symbol*>::iterator& end) {

  // Replace AND first
  for (std::list<Symbol*>::iterator it = start; it != end; ++it) {

    if ((*it)->GetType() == LOGICAL_AND) {

      Symbol* s = GetBinarySymbol(start, end, it, (*it)->GetType(), false);
      it = ReplaceSymbols(s, --it, 3, start);
    }
  }

  for (std::list<Symbol*>::iterator it = start; it != end; ++it) {

    if ((*it)->GetType() == LOGICAL_OR) {

      Symbol* s = GetBinarySymbol(start, end, it, (*it)->GetType(), false);
      it = ReplaceSymbols(s, --it, 3, start);
    }
  }
}

void
Expression::ReplaceCommas(std::list<Symbol*>::iterator& start,
                          std::list<Symbol*>::iterator& end) {

  for (std::list<Symbol*>::iterator it = start; it != end; ++it) {

    if ((*it)->GetType() == COMMA) {
      // if at start or end, get rid of it.
      // Extra commas at beginning, end are OK.
      if (it == start || distance(it, end) == 1) {
        it = ReplaceSymbols(NULL, it, 1, start);
        it--;
      } else {
        // We have a list.  Parse it.
        Symbol* first = *(--it);
        ++it;
        Symbol* second = *(++it);
        --it;
        Symbol* list;
        if (first->GetType() == LIST) {
          // Add to the list
          list = first;
          static_cast<ListSymbol*>(list)->PushBack(second);
        } else {
          // Create a new list
          list = new ListSymbol(first, second);
          allocatedSymbols_.push_back(list);
        }
        it = ReplaceSymbols(list, --it, 3, start);
      }
    }
  }
}

std::list<Symbol*>::iterator
Expression::ReplaceSymbols(Symbol* s,
                           std::list<Symbol*>::iterator removeStart,
                           size_t n,
                           std::list<Symbol*>::iterator& start) {

  std::list<Symbol*>::iterator removeEnd = removeStart;
  for (unsigned i = 0; i < n; i++) {
    removeEnd++;
  }

  std::list<Symbol*>::iterator pos = removeEnd;
  if (s) {
    // Push new symbol after the symbols it is replacing
    pos = parsedSymbols_.insert(removeEnd, s);
  }

  // Need to change start if we're removing from the beginning
  if (removeStart == start) {
    start = pos;
  }

  parsedSymbols_.erase(removeStart, pos);
  return pos;
}

template <typename T, typename U>
T GetConstValue(Symbol* s) {
  ConstNode<U>* cn = dynamic_cast<ConstNode<U>* >(s);
  if (!cn) {
    XCDFFatal("Non-constant value used inside \"in\" expression");
  }
  // Get the datum value and cast as type T
  return static_cast<T>((*cn)[0]);
}

template <typename T>
T GetNodeValue(Symbol* s) {
  switch (s->GetType()) {
    case FLOATING_POINT_NODE: return GetConstValue<T, double>(s);
    case SIGNED_NODE: return GetConstValue<T, int64_t>(s);
    case UNSIGNED_NODE: return GetConstValue<T, uint64_t>(s);
    default:
      XCDFFatal("Non-constant value used inside \"in\" expression");
  }
}

template <typename T>
Symbol* GetInNode(Node<T>* n1, Symbol* n2) {
  std::vector<T> data;
  if (n2->GetType() == LIST) {
    ListSymbol* list = static_cast<ListSymbol*>(n2);
    for (std::vector<Symbol*>::const_iterator
                                it = list->SymbolsBegin();
                                it != list->SymbolsEnd(); ++it) {
      data.push_back(GetNodeValue<T>(*it));
    }
  } else {
    // Not a list.
    data.push_back(GetNodeValue<T>(n2));
  }
  return new InNode<T>(*n1, data);
}

template <typename T>
Symbol* GetNodeImpl(Node<T>* n1, SymbolType type) {
  switch (type) {

    case LOGICAL_NOT:
      return new LogicalNOTNode<T>(*n1);
    case BITWISE_NOT:
      return new BitwiseNOTNode<T>(*n1);
    case UNIQUE:
      return new UniqueNode<T>(*n1);
    case ANY:
      return new AnyNode<T>(*n1);
    case ALL:
      return new AllNode<T>(*n1);
    case SUM:
      return new SumNode<T>(*n1);
    case SIN:
      return new SinNode<T>(*n1);
    case COS:
      return new CosNode<T>(*n1);
    case TAN:
      return new TanNode<T>(*n1);
    case ASIN:
      return new AsinNode<T>(*n1);
    case ACOS:
      return new AcosNode<T>(*n1);
    case ATAN:
      return new AtanNode<T>(*n1);
    case LOG:
      return new LogNode<T>(*n1);
    case LOG10:
      return new Log10Node<T>(*n1);
    case EXP:
      return new ExpNode<T>(*n1);
    case ABS:
      return new AbsNode<T>(*n1);
    case SQRT:
      return new SqrtNode<T>(*n1);
    case CEIL:
      return new CeilNode<T>(*n1);
    case FLOOR:
      return new FloorNode<T>(*n1);
    case ISNAN:
      return new IsNaNNode<T>(*n1);
    case ISINF:
      return new IsInfNode<T>(*n1);
    case SINH:
      return new SinhNode<T>(*n1);
    case COSH:
      return new CoshNode<T>(*n1);
    case TANH:
      return new TanhNode<T>(*n1);
    case INT:
      return new IntNode<T>(*n1);
    case UNSIGNED:
      return new UnsignedNode<T>(*n1);
    case DOUBLE:
      return new DoubleNode<T>(*n1);
    default:
      return new Symbol();
  }

  assert(false);
  return new Symbol();
}

template <typename T, typename U, typename DominantType>
Symbol* GetNodeImpl(Node<T>* n1, Node<U>* n2, SymbolType type) {
  switch (type) {

    case EQUALITY:
      return new EqualityNode<T, U, DominantType>(*n1, *n2);
    case INEQUALITY:
      return new InequalityNode<T, U, DominantType>(*n1, *n2);
    case GREATER_THAN:
      return new GreaterThanNode<T, U, DominantType>(*n1, *n2);
    case LESS_THAN:
      return new LessThanNode<T, U, DominantType>(*n1, *n2);
    case GREATER_THAN_EQUAL:
      return new GreaterThanEqualNode<T, U, DominantType>(*n1, *n2);
    case LESS_THAN_EQUAL:
      return new LessThanEqualNode<T, U, DominantType>(*n1, *n2);
    case LOGICAL_OR:
      return new LogicalORNode<T, U, DominantType>(*n1, *n2);
    case LOGICAL_AND:
      return new LogicalANDNode<T, U, DominantType>(*n1, *n2);
    case BITWISE_OR:
      return new BitwiseORNode<T, U, DominantType>(*n1, *n2);
    case BITWISE_AND:
      return new BitwiseANDNode<T, U, DominantType>(*n1, *n2);
    case ADDITION:
      return new AdditionNode<T, U, DominantType>(*n1, *n2);
    case SUBTRACTION:
      return new SubtractionNode<T, U, DominantType>(*n1, *n2);
    case MULTIPLICATION:
      return new MultiplicationNode<T, U, DominantType>(*n1, *n2);
    case DIVISION:
      return new DivisionNode<T, U, DominantType>(*n1, *n2);
    case MODULUS:
      return new ModulusNode<T, U>(*n1, *n2);
    case POWER:
      return new PowerNode<T, U>(*n1, *n2);
    case FMOD:
      return new FmodNode<T, U>(*n1, *n2);
    case ATAN2:
      return new Atan2Node<T, U>(*n1, *n2);
    case POW:
      return new PowerNode<T, U>(*n1, *n2);
    default:
      return new Symbol();
  }
}


Symbol*
DoGetNode(Symbol* n1, SymbolType type) {
  switch (n1->GetType()) {

    default:
    case FLOATING_POINT_NODE:
      return GetNodeImpl(static_cast<Node<double>* >(n1), type);
    case SIGNED_NODE:
      return GetNodeImpl(static_cast<Node<int64_t>* >(n1), type);
    case UNSIGNED_NODE:
      return GetNodeImpl(static_cast<Node<uint64_t>* >(n1), type);
  }
}

Symbol*
DoGetNode(Symbol* n1, Symbol* n2, SymbolType type) {

  if (type == IN) {
    switch (n1->GetType()) {
      default:
      case FLOATING_POINT_NODE:
        return GetInNode<double>(static_cast<Node<double>* >(n1), n2);
      case SIGNED_NODE:
        return GetInNode<int64_t>(static_cast<Node<int64_t>* >(n1), n2);
      case UNSIGNED_NODE:
        return GetInNode<uint64_t>(static_cast<Node<uint64_t>* >(n1), n2);
    }
  }

  switch (n1->GetType()) {

    case FLOATING_POINT_NODE: {

      switch (n2->GetType()) {

        default:
        case FLOATING_POINT_NODE:
          return GetNodeImpl<double, double, double>(
                             static_cast<Node<double>* >(n1),
                             static_cast<Node<double>* >(n2), type);
        case SIGNED_NODE:
          return GetNodeImpl<double, int64_t, double>(
                             static_cast<Node<double>* >(n1),
                             static_cast<Node<int64_t>* >(n2), type);
        case UNSIGNED_NODE:
          return GetNodeImpl<double, uint64_t, double>(
                             static_cast<Node<double>* >(n1),
                             static_cast<Node<uint64_t>* >(n2), type);
      }
      break;
    }

    case SIGNED_NODE: {

      switch (n2->GetType()) {

        default:
        case FLOATING_POINT_NODE:
          return GetNodeImpl<int64_t, double, double>(
                             static_cast<Node<int64_t>* >(n1),
                             static_cast<Node<double>* >(n2), type);
        case SIGNED_NODE:
          return GetNodeImpl<int64_t, int64_t, int64_t>(
                             static_cast<Node<int64_t>* >(n1),
                             static_cast<Node<int64_t>* >(n2), type);
        case UNSIGNED_NODE:
          return GetNodeImpl<int64_t, uint64_t, int64_t>(
                             static_cast<Node<int64_t>* >(n1),
                             static_cast<Node<uint64_t>* >(n2), type);

      }
      break;
    }

    case UNSIGNED_NODE: {

      switch (n2->GetType()) {

        default:
        case FLOATING_POINT_NODE:
          return GetNodeImpl<uint64_t, double, double>(
                             static_cast<Node<uint64_t>* >(n1),
                             static_cast<Node<double>* >(n2), type);
        case SIGNED_NODE:
          return GetNodeImpl<uint64_t, int64_t, int64_t>(
                             static_cast<Node<uint64_t>* >(n1),
                             static_cast<Node<int64_t>* >(n2), type);
        case UNSIGNED_NODE:
          return GetNodeImpl<uint64_t, uint64_t, uint64_t>(
                             static_cast<Node<uint64_t>* >(n1),
                             static_cast<Node<uint64_t>* >(n2), type);
      }
      break;
    }

    default:

      return new Symbol();
  }
}

Symbol*
Expression::GetUnarySymbol(std::list<Symbol*>::iterator start,
                           std::list<Symbol*>::iterator end,
                           std::list<Symbol*>::iterator it,
                           SymbolType type,
                           bool isFunction) {

  Symbol* func = *it;
  if (distance(it, end) < 2) {
    XCDFFatal("Cannot evaluate expression: " <<
                        "Missing unary operand in " << *func);
  }

  ++it;
  Symbol* n1 = *it;

  if (!(n1->IsNode())) {
    XCDFFatal("Cannot evaluate expression: " <<
                        "Missing unary operand in " << *func);
  }

  ++it;
  if (it != end) {
    if ((*it)->IsNode() && isFunction) {
      XCDFFatal("Too many arguments to unary function " << *func);
    }
  }

  Symbol* symbol = DoGetNode(n1, type);
  allocatedSymbols_.push_back(symbol);
  return symbol;
}

Symbol*
Expression::GetBinarySymbol(std::list<Symbol*>::iterator start,
                            std::list<Symbol*>::iterator end,
                            std::list<Symbol*>::iterator it,
                            SymbolType type,
                            bool isFunction) {

  Symbol* func = *it;

  Symbol* n1;
  Symbol* n2;

  if (isFunction) {

    if (distance(it, end) < 2) {
      XCDFFatal("Cannot evaluate expression: " <<
                         "Missing binary operand in " << *func);
    }
    ++it;

    if ((*it)->GetType() != LIST) {
      XCDFFatal("Cannot evaluate expression: " <<
                            "Missing binary operand in " << *func);
    }

    ListSymbol* argList = static_cast<ListSymbol*>(*it);
    if (argList->GetSize() < 2) {
      XCDFFatal("Cannot evaluate expression: " <<
                            "Missing binary operand in " << *func);
    }

    if (argList->GetSize() > 2) {
      XCDFFatal("Too many arguments to binary function " << *func);
    }

    n1 = (*argList)[0];
    n2 = (*argList)[1];

  } else {

    if (it == start || distance(it, end) < 2) {
      XCDFFatal("Cannot evaluate expression: " <<
                         "Missing binary operand in " << *func);
    }

    --it;
    n1 = *it;

    ++it;
    ++it;
    n2 = *it;
  }

  if (!(n1->IsNode()) ||  !((n2->IsNode()) || type == IN)) {
    XCDFFatal("Cannot evaluate expression: " <<
                            "Missing binary operand in " << *func);
  }

  Symbol* symbol = DoGetNode(n1, n2, type);
  allocatedSymbols_.push_back(symbol);
  return symbol;
}

Symbol* GetNodeImpl(SymbolType type) {
  switch(type) {

    case RAND:
      return new RandNode();
    default:
      return new Symbol();
  }
}


Symbol*
Expression::GetVoidSymbol(std::list<Symbol*>::iterator start,
                          std::list<Symbol*>::iterator end,
                          std::list<Symbol*>::iterator it,
                          SymbolType type) {

  Symbol* func = *it;

  ++it;
  if (it != end) {
    if ((*it)->IsNode()) {
      XCDFFatal("Too many arguments to function " << *func);
    }
  }

  Symbol* symbol = GetNodeImpl(type);
  allocatedSymbols_.push_back(symbol);
  return symbol;
}
