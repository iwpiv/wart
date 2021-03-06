//// compiled primitives for I/O

long Max_depth = 12;

ostream& operator<<(ostream& os, cell* c) {
  if (c == NULL) return os << "NULLNULLNULL";
  if (c == nil) return os << "nil";
  switch(c->type) {
  case CONS:
    if (car(c) == sym_quote || car(c) == sym_backquote || car(c) == sym_unquote || car(c) == sym_splice || car(c) == sym_unquote_splice || car(c) == sym_already_evald)
      return os << car(c) << cdr(c);
    os << "(" << car(c);
    for (cell* curr = cdr(c); curr != nil; curr = cdr(curr)) {
      if (is_cons(curr))
        os << " " << car(curr);
      else
        os << " ... " << curr;
    }
    return os << ")";
  case INTEGER:
    return os << to_int(c);
  case FLOAT:
    return os << to_float(c);
  case SYMBOL:
    return os << to_string(c);
  case STRING:
    return os << "\"" << to_string(c) << "\"";
  case TABLE:
    os << (table*)c->car;
    if (cdr(c) != nil)
      os << "->" << cdr(c);
    return os;
  case COMPILED_FN:
    return os << "#compiled";
  default:
    cerr << "Can't print type " << c->type << '\n' << die();
    return os;
  }
}

ostream& operator<<(ostream& os, table* t) {
  os << "{";
  // order common keys deterministically for tracing tests
  if (t->value[sym_name]) os << t->value[sym_name] << ": ";
  if (t->value[sym_sig]) os << sym_sig << ':' << t->value[sym_sig] << ", ";
  if (t->value[sym_body]) os << sym_body << ':' << t->value[sym_body] << ", ";
  if (t->value[sym_env]) os << sym_env << ':' << t->value[sym_env] << ", ";
  for (cell_map::iterator p = t->value.begin(); p != t->value.end(); ++p) {
    if (!p->second) continue;
    if (p->first == sym_name || p->first == sym_sig || p->first == sym_body || p->first == sym_env)
      continue;
    os << (cell*)p->first << ':' << (cell*)p->second << ", ";
  }
  return os << "}";
}



COMPILE_FN(pr, compiledfn_pr, "($x)",
  cell* x = lookup("$x");
  ostream& out = to_ostream(STDOUT);
  print(x, out);
  out.flush();
  return mkref(x);
)

COMPILE_FN(write, compiledfn_write, "($x)",
  cell* x = lookup("$x");
  ostream& out = to_ostream(STDOUT);
  out << x;
  out.flush();
  return mkref(x);
)

COMPILE_FN(err, compiledfn_err, "($x)",
  cell* x = lookup("$x");
  ostream& out = to_ostream(STDERR);
  print(x, out);
  out.flush();
  return mkref(x);
)

void print(cell* x, ostream& out) {
  if (is_string(x)) out << to_string(x);
  else out << x;
}



COMPILE_FN(read, compiledfn_read, "('$eof)",
  if (to_istream(STDIN).eof())
    return mkref(lookup("$eof"));
  return mkref(read(to_istream(STDIN)));
)

cell* read(istream& in) {
  indent_sensitive_stream in2(in);
  return read(in2);
}

COMPILE_FN(read_byte, compiledfn_read_byte, "('$eof)",
  istream& f = to_istream(STDIN);
  char c;
  if (!f.read(&c, 1))
    return mkref(lookup("$eof"));
  return mkref(new_num((long)c));
)

COMPILE_FN(read_line, compiledfn_read_line, "('$eof)",
  istream& f = to_istream(STDIN);
  string result;
  if (!getline(f, result))
    return mkref(lookup("$eof"));
  return mkref(new_string(result));
)
