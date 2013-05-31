//// creating functions and macros; calling them with args

// Design considered the following:
//  implicit eval: by default (f arg1 arg2 arg3) evals all the elems, then passes the args to f
//  functions are just data: car => (object function {params: l, body: #compiled})
//  user-defined functions are easy to reflect on: (fn (x) 34) => (object function {params: (x), body: (34)})
//  functions create a private scope, can continue to access outer bindings (lexical scope)
//  user-defined functions can't see caller environment
//  quote to suppress eval in expressions: 'abc => abc
//  quoted params suppress arg eval: ((fn '(x) x) abc) => abc
//  varargs functions: ((fn params params) 1 2 3) => (1 2 3)
//  varargs functions with some named params: ((fn (fmt ... rest) (printf fmt rest)) "%d%d" 34 35) => "3435"
//  passing in lists to functions: ((fn ((x y)) (+ x y)) '(3 4)) => 7
//
//  list templates: backquote to suppress eval, unquote to reenable eval inside backquote. `(+ ,a ,b)
//  ability to splice multiple elements into lists: ,@vars inside backquote
//  macros need to access caller environment

cell* eval(cell* expr) {
  return eval(expr, Curr_lexical_scope);
}

cell* eval(cell* expr, cell* scope) {
  new_trace_frame("eval");
  if (!expr)
    RAISE << "eval: cell should never be NULL\n" << die();

  trace("eval") << expr;
  if (expr == nil) {
    trace("eval") << "nil branch";
    trace("eval") << "=> nil";
    return nil;
  }

  if (is_keyword_sym(expr)) {
    trace("eval") << "keyword sym";
    trace("eval") << "=> " << expr;
    return mkref(expr);
  }

  if (is_sym(expr)) {
    trace("eval") << "sym";
    cell* result = lookup(expr);
    trace("eval") << "=> " << result;
    return mkref(result);
  }

  if (is_atom(expr)) {
    trace("eval") << "literal";
    trace("eval") << "=> " << expr;
    return mkref(expr);
  }

  if (is_object(expr)) {
    trace("eval") << "object";
    trace("eval") << "=> " << expr;
    return mkref(expr);
  }

  if (is_quoted(expr)) {
    trace("eval") << "quote";
    trace("eval") << "=> " << cdr(expr);
    return mkref(cdr(expr));
  }

  if (is_backquoted(expr)) {
    cell* result = process_unquotes(cdr(expr), 1, scope);
    trace("eval") << "backquote";
    trace("eval") << "=> " << result;
    return result;  // already mkref'd
  }

  // expr is a call
  TEMP(fn, to_fn(eval(car(expr), scope)));
  if (!is_fn(fn))
    RAISE << "Not a call: " << expr << '\n'
        << "Perhaps you need to split the line in two.\n";

  // eval its args in the caller's lexical environment
  cell* new_scope = new_table();
  eval_bind_all(sig(fn), cdr(expr), scope, new_scope);

  // swap in the function's lexical environment
  new_dynamic_scope(CURR_LEXICAL_SCOPE, is_compiledfn(body(fn)) ? scope : env(fn));
  add_lexical_scope(new_scope);
  add_lexical_binding(sym_caller_scope, scope);

  cell* result = nil;
  if (is_compiledfn(body(fn))) {
    trace("eval") << "compiled fn";
    result = to_compiledfn(body(fn))();   // all compiledfns mkref their result
  }
  else {
    trace("eval") << "fn";
    // eval all forms in body, save result of final form
    for (cell* form = impl(fn); form != nil; form=cdr(form))
      update(result, eval(car(form)));
  }

  end_lexical_scope();  // implicitly rmrefs new_scope
  end_dynamic_scope(CURR_LEXICAL_SCOPE);

  trace("eval") << "=> " << result;
  return result;  // already mkref'd
}

// bind params to args in new_scope, taking into account:
//  quoted params (eval'ing args as necessary; args is never quoted, though)
//  destructured params
void eval_bind_all(cell* params, cell* args, cell* scope, cell* new_scope) {
  trace("eval/bind/all") << params << " <-> " << args;
  if (params == nil)
    return;

  TEMP(args2, nil);
  if (is_quoted(params)) {
    params = strip_quote(params);
    args2 = quote_all(args);   // already mkref'd
    trace("eval/bind/all") << "stripping quote: " << params << " <-> " << args2;
  }
  else {
    args2 = mkref(args);
    trace("eval/bind/all") << " args nrefs: " << args2->nrefs;
  }

  if (is_sym(params)) {
    TEMP(val, eval_all(args2, scope));
    bind_params(params, val, new_scope);
  }

  else if (!is_cons(params))
    ;

  else {
    eval_bind_param(car(params), car(args2), scope, new_scope);
    eval_bind_all(cdr(params), cdr(args2), scope, new_scope);
  }
}

void eval_bind_param(cell* param, cell* arg, cell* scope, cell* new_scope) {
  TEMP(arg2, nil);
  if (is_quoted(param)) {
    param = strip_quote(param);
    arg2 = mkref(new_cons(sym_quote, arg));
  }
  else
    arg2 = mkref(arg);

  TEMP(val, eval(arg2, scope));
  bind_params(param, val, new_scope);
}

void bind_params(cell* params, cell* args, cell* new_scope) {
  trace("eval/bind/one") << params << " <-> " << args;
  if (is_quoted(params))
    bind_params(strip_quote(params), args, new_scope);

  else if (params == nil)
    ;

  else if (is_sym(params)) {
    trace("eval/bind/one") << "binding " << params << " to " << args;
    add_lexical_binding(params, args, new_scope);
  }

  else if (!is_cons(params))
    ;

  else if (args != nil && !is_cons(args))
    bind_params(params, nil, new_scope);

  else {
    bind_params(car(params), car(args), new_scope);
    bind_params(cdr(params), cdr(args), new_scope);
  }
}

cell* eval_all(cell* args, cell* scope) {
  if (!is_cons(args))
    return eval(args, scope);
  cell* p_result = new_cell(), *curr = p_result;
  for (; args != nil; args=cdr(args), curr=cdr(curr)) {
    TEMP(val, eval(car(args), scope));
    add_cons(curr, val);
  }
  return drop_ptr(p_result);
}



//// backquoted exprs

cell* process_unquotes(cell* x, long depth, cell* scope) {
  new_trace_frame("backquote");
  trace("backquote") << x << " " << depth;
  if (!is_cons(x)) {
    trace("backquote") << "atom: " << x;
    return mkref(x);
  }

  if (unquote_depth(x) == depth) {
    cell* result = eval(strip_unquote(x), scope);
    trace("backquote") << "eval: " << result;
    return result;  // already mkref'd
  }
  else if (unquote_splice_depth(car(x)) == depth) {
    TEMP(splice, eval(strip_unquote_splice(car(x)), scope));
    trace("backquote") << "splice: " << splice;
    TEMP(rest, process_unquotes(cdr(x), depth, scope));
    if (splice == nil) return mkref(rest);

    // always splice in a copy
    cell* result = copy_list(splice);
    append(result, rest);
    return mkref(result);
  }
  else if (unquote_depth(x) > 0) {
    trace("backquote") << "not deep enough: " << x;
    return mkref(x);
  }

  if (is_backquoted(x)) {
    TEMP(rest, process_unquotes(cdr(x), depth+1, scope));
    cell* result = new_cons(car(x), rest);
    trace("backquote") << "backquote: " << result;
    return mkref(result);
  }

  TEMP(head, process_unquotes(car(x), depth, scope));
  TEMP(tail, process_unquotes(cdr(x), depth, scope));
  cell* result = new_cons(head, tail);
  trace("backquote") << "=> " << result;
  return mkref(result);
}

long unquote_depth(cell* x) {
  if (is_unquoted(x))
    return unquote_depth(cdr(x))+1;
  return 0;
}

cell* strip_unquote(cell* x) {
  if (is_unquoted(x))
    return strip_unquote(cdr(x));
  return x;
}

long unquote_splice_depth(cell* x) {
  if (is_unquote_spliced(x))
    return 1;
  if (is_unquoted(x))
    return unquote_splice_depth(cdr(x))+1;
  return 1000;  // never try to splice
}

cell* strip_unquote_splice(cell* x) {
  return cdr(strip_unquote(x));
}



//// helpers

bool is_quoted(cell* cell) {
  return is_cons(cell) && car(cell) == sym_quote;
}

bool is_backquoted(cell* cell) {
  return is_cons(cell) && car(cell) == sym_backquote;
}

cell* strip_quote(cell* cell) {
  return is_quoted(cell) ? cdr(cell) : cell;
}

bool is_unquoted(cell* arg) {
  return is_cons(arg) && car(arg) == sym_unquote;
}

bool is_spliced(cell* arg) {
  return is_cons(arg) && car(arg) == sym_splice;
}

bool is_unquote_spliced(cell* arg) {
  return is_cons(arg) && car(arg) == sym_unquote_splice;
}

bool is_keyword_sym(cell* x) {
  if (!is_sym(x)) return false;
  string name = to_string(x);
  if (name == ":") return false;
  return name[0] == ':';
}

// fn = (object function {sig => .., body => .., env => ..})
bool is_fn(cell* x) {
  return is_cons(x) && type(x) == sym_function;
}

cell* to_fn(cell* x) {
  if (x == nil || is_fn(x)) return x;
  lease_cell lease(x);  // we assume x is already mkref'd
  if (!lookup_dynamic_binding(sym_Coercions))
    RAISE << "tried to call " << x << '\n' << die();
  cell* result = coerce_quoted(x, sym_function, lookup(sym_Coercions));
  return result;
}

cell* sig(cell* fn) {
  return get(rep(fn), sym_sig);
}

cell* body(cell* fn) {
  return get(rep(fn), sym_body);
}

cell* impl(cell* fn) {
  cell* impl = get(rep(fn), sym_optimized_body);
  return (impl != nil) ? impl : body(fn);
}

cell* env(cell* fn) {
  return get(rep(fn), sym_env);
}

cell* quote(cell* x) {
  return new_cons(sym_quote, x);
}

cell* quote_all(cell* x) {
  cell* result = new_cell(), *curr = result;
  for (cell* iter = x; iter != nil; iter=cdr(iter), curr=cdr(curr))
    add_cons(curr, quote(car(iter)));
  return drop_ptr(result);
}
