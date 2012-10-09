//// split input into tokens separated by newlines, indent, and the following boundaries:
const string punctuationChars = "();\"";  // the skeleton of a wart program
const string quoteAndUnquoteChars = ",'`@";   // controlling eval and macros

// Design considered the following:
//  doing the minimum necessary to support macros later
//    so backquote and unquote and splice are supported
//    so infix ops are ignored
//  supporting whitespace sensitivity
//    preserve indent information because later passes can't recreate it
//  avoid modifying strings
//    so parse them here and make them easy for later passes to detect

// line contains 1 indent and zero or more regular tokens
struct Token {
  string token;
  long indentLevel;   // all tokens on a line share its indentLevel

  explicit Token(string s)
    :token(s), indentLevel(0) {}
  explicit Token(long indent)
    :token(""), indentLevel(indent) {}
  Token(const string x, const long l)
    :token(x), indentLevel(l) {}
  Token(const Token& rhs)
    :token(rhs.token), indentLevel(rhs.indentLevel) {}
  Token& operator=(const Token& rhs) {
    if (this == &rhs) return *this;
    token = rhs.token;
    indentLevel = rhs.indentLevel;
    return *this;
  }

  bool isIndent() {
    return token == "";
  }
  bool isParen() {
    return token == "(" || token == ")";
  }
  bool isQuoteOrUnquote() {
    return token == "'" || token == "`"
        || token == "," || token == ",@" || token == "@";
  }

  bool operator==(string x) {
    return token == x;
  }
  bool operator!=(string x) {
    return !(*this == x);
  }
  bool operator==(Token x) {
    return token == x.token && indentLevel == x.indentLevel;
  }
  bool operator!=(Token x) {
    return !(*this == x);
  }
};

Token nextToken(CodeStream& c) {
  if (c.currIndent == -1)   // initial
    return Token(c.currIndent=indent(c.fd));
  skipWhitespace(c.fd);
  if (c.fd.peek() == '\n' || c.fd.peek() == ';')
    return Token(c.currIndent=indent(c.fd));

  ostringstream out;
  char nextchar = c.fd.peek(); // guaranteed not to be whitespace
  if (nextchar == '"')
    slurpString(c.fd, out);
  else if (find(punctuationChars, nextchar))
    slurpChar(c.fd, out);
  else if (nextchar == ',')
    slurpUnquote(c.fd, out);
  else if (find(quoteAndUnquoteChars, nextchar))
    slurpChar(c.fd, out);
  else
    slurpWord(c.fd, out);

  return Token(out.str(), c.currIndent);
}



// internals

// slurp functions read a token when you're sure to be at it
void slurpChar(istream& in, ostream& out) {
  out << (char)in.get();
}

void slurpWord(istream& in, ostream& out) {
  char c;
  while (in >> c) {
    if (isspace(c) || find(punctuationChars, c) || find(quoteAndUnquoteChars, c)) {
      in.putback(c);
      break;
    }
    out << c;
  }
}

void slurpString(istream& in, ostream& out) {
  slurpChar(in, out);   // initial quote
  char c;
  while (in >> c) {
    out << c;
    if (c == '\\')
      slurpChar(in, out);   // blindly read next
    else if (c == '"')
      break;
  }
}

void slurpUnquote(istream& in, ostream& out) {
  slurpChar(in, out);   // comma
  if (in.peek() == '@')
    slurpChar(in, out); // ..and maybe splice
}



// also modifies cs
long indent(istream& in) {
  long indent = 0;
  char c;
  while (in >> c) {
    if (c == ';')
      skipComment(in);

    else if (!isspace(c)) {
      in.putback(c);
      break;
    }

    else if (c == ' ') ++indent;
    else if (c == '\t') indent+=2;
    else if (c == '\n') indent=0;
  }
  return indent;
}

void skipComment(istream& in) {
  char c;
  while (in >> c) {
    if (c == '\n') {
      in.putback(c);
      break;
    }
  }
}


void skipWhitespace(istream& in) {
  while (isspace(in.peek()) && in.peek() != '\n')
    skip(in);
}

void skip(istream& in) {
  char dummy;
  in >> dummy;
}



const size_t NOT_FOUND = string::npos;
bool find(string s, char c) {
  return s.find(c) != NOT_FOUND;
}

ostream& operator<<(ostream& os, Token y) {
  if (y == "") return os << ":" << y.indentLevel;
  else return os << y.token;
}