//// eval: lookup symbols, respect quotes, rewrite lambda calls

                                  bool isQuoted(Cell* cell) {
                                    return isCons(cell) && car(cell) == newSym(L"'");
                                  }

                                  bool isBackQuoted(Cell* cell) {
                                    return isCons(cell) && car(cell) == newSym(L"`");
                                  }

                                  bool isColonSym(Cell* x) {
                                    return isSym(x) && toString(x)[0] == L':';
                                  }

                                  Cell* sig(Cell* lambda) {
                                    return car(cdr(lambda));
                                  }

                                  Cell* body(Cell* lambda) {
                                    return cdr(cdr(lambda));
                                  }

                                  Cell* calleeBody(Cell* callee) {
                                    return car(cdr(cdr(callee)));
                                  }

                                  Cell* calleeEnv(Cell* callee) {
                                    return cdr(cdr(cdr(callee)));
                                  }

                                  Cell* callArgs(Cell* call) {
                                    return cdr(call);
                                  }



                                  // doesn't look inside destructured params
                                  Cell* keywordArg(Cell* params, Cell* arg) {
                                    if (!isColonSym(arg)) return nil;
                                    Cell* realArg = newSym(toString(arg).substr(1));
                                    for (; params != nil; params=cdr(params))
                                      if (realArg == (isCons(params) ? car(params) : params))
                                        return realArg;
                                    return nil;
                                  }

                                  // extract keyword args into the hash_map provided; return non-keyword args
                                  Cell* extractKeywordArgs(Cell* params, Cell* args, CellMap& keywordArgs) {
                                    Cell *pNonKeywordArgs = newCell(), *curr = pNonKeywordArgs;
                                    for (; args != nil; args=cdr(args)) {
                                      Cell* currArg = keywordArg(params, car(args));
                                      if (currArg != nil) {
                                        args = cdr(args); // skip keyword arg
                                        keywordArgs[currArg] = car(args);
                                      }
                                      else {
                                        addCons(curr, car(args));
                                        curr=cdr(curr);
                                      }
                                    }
                                    return dropPtr(pNonKeywordArgs);
                                  }

                                  Cell* argsInParamOrder(Cell* params, Cell* nonKeywordArgs, CellMap& keywordArgs) {
                                    Cell* pReconstitutedArgs = newCell();
                                    for (Cell* curr = pReconstitutedArgs; params != nil; curr=cdr(curr), params=cdr(params)) {
                                      if (!isCons(params)) {
                                        setCdr(curr, keywordArgs[params] ? keywordArgs[params] : nonKeywordArgs);
                                        break;
                                      }

                                      if (keywordArgs[car(params)]) {
                                        addCons(curr, keywordArgs[car(params)]);
                                      } else {
                                        addCons(curr, car(nonKeywordArgs));
                                        nonKeywordArgs = cdr(nonKeywordArgs);
                                      }
                                    }
                                    return dropPtr(pReconstitutedArgs);
                                  }

Cell* reorderKeywordArgs(Cell* params, Cell* args) {
  CellMap keywordArgs;
  Cell* nonKeywordArgs = extractKeywordArgs(params, args, keywordArgs);
  Cell* result = argsInParamOrder(params, nonKeywordArgs, keywordArgs);
  rmref(nonKeywordArgs);
  return result;
}



                                  bool isSplice(Cell* arg) {
                                    return isCons(arg) && car(arg) == newSym(L"@");
                                  }

                                  Cell* unsplice(Cell* arg) {
                                    return eval(cdr(arg));
                                  }

                                  Cell* unspliceAll(Cell* args) {
                                    Cell* pResult = newCell();
                                    for (Cell *curr=pResult, *arg=car(args); args != nil; args=cdr(args), arg=car(args), curr=last(curr)) {
                                      if (!isSplice(arg)) {
                                        addCons(curr, arg);
                                        continue;
                                      }
                                      Cell* newLimb = unsplice(arg);
                                      setCdr(curr, newLimb);
                                      rmref(newLimb);
                                    }
                                    return dropPtr(pResult); // mkref's
                                  }

                                  Cell* evalArgs(Cell* params, Cell* args);
                                  Cell* spliceFirst(Cell* params, Cell* args) {
                                    Cell* result = unsplice(car(args));
                                    if (result == nil)
                                      return evalArgs(params, cdr(args));
                                    Cell* curr = result;
                                    for (; cdr(curr) != nil; curr=cdr(curr))
                                      params=cdr(params); // don't eval spliced args again, even if param is unquoted
                                    setCdr(curr, evalArgs(cdr(params), cdr(args)));
                                    rmref(cdr(curr));
                                    return result; // already mkref'd
                                  }

Cell* evalArgs(Cell* params, Cell* args) {
  if (args == nil) return nil;
  if (isQuoted(params))
    return unspliceAll(args); // already mkref'd

  if (isSplice(car(args)))
    return spliceFirst(params, args); // already mkref'd

  Cell* result = newCell();
  setCdr(result, evalArgs(cdr(params), cdr(args)));
  rmref(cdr(result));
  if (!isCons(params) || !isQuoted(car(params))) {
    setCar(result, eval(car(args)));
    rmref(car(result));
  }
  else {
    setCar(result, car(args));
  }
  return mkref(result);
}

void bindArgs(Cell* params, Cell* args) {
  if (params == nil) return;

  if (isQuoted(params)) {
    bindArgs(cdr(params), args);
    return;
  }

  if (isSym(params))
    addLexicalBinding(params, args);
  else
    bindArgs(car(params), car(args));

  bindArgs(cdr(params), cdr(args));
}



                                  int unquoteDepth(Cell* x) {
                                    if (!isCons(x) || car(x) != newSym(L","))
                                      return 0;
                                    return unquoteDepth(cdr(x))+1;
                                  }

                                  Cell* stripUnquote(Cell* x) {
                                    if (!isCons(x) || car(x) != newSym(L","))
                                      return x;
                                    return stripUnquote(cdr(x));
                                  }

Cell* processUnquotes(Cell* x, int depth) {
  if (!isCons(x)) return mkref(x);

  if (unquoteDepth(x) == depth)
    return eval(stripUnquote(x));
  else if (car(x) == newSym(L","))
    return mkref(x);

  if (isBackQuoted(x)) {
    Cell* result = newCons(car(x), processUnquotes(cdr(x), depth+1));
    rmref(cdr(result));
    return mkref(result);
  }

  if (depth == 1 && isCons(car(x)) && car(car(x)) == newSym(L",@")) {
    Cell* result = eval(cdr(car(x)));
    Cell* splice = processUnquotes(cdr(x), depth);
    if (result == nil) return splice;
    append(result, splice);
    rmref(splice);
    return result; // already mkref'd
  }

  Cell* result = newCons(processUnquotes(car(x), depth), processUnquotes(cdr(x), depth));
  rmref(car(result));
  rmref(cdr(result));
  return mkref(result);
}



                                  bool isFunc(Cell* x) {
                                    return isCons(x)
                                      && (isPrimFunc(car(x)) || car(x) == newSym(L"evald-lambda"));
                                  }

                                  Cell* evalLambda(Cell* expr) {
                                    return newCons(newSym(L"evald-lambda"),
                                        newCons(sig(expr),
                                            newCons(body(expr), currLexicalScopes.top())));
                                  }

Cell* eval(Cell* expr) {
  if (!expr)
    err << "eval: cell should never be NULL" << endl << DIE;

  if (expr == nil)
    return nil;

  if (isColonSym(expr))
    return mkref(expr);

  if (isSym(expr))
    return mkref(lookup(expr));

  if (isAtom(expr))
    return mkref(expr);

  if (isQuoted(expr))
    return mkref(cdr(expr));

  if (isBackQuoted(expr))
    return processUnquotes(cdr(expr), 1); // already mkref'd

  if (car(expr) == newSym(L"lambda"))
    // attach current lexical scope
    return mkref(evalLambda(expr));
  else if (isFunc(expr))
    // lexical scope is already attached
    return mkref(expr);

  // expr is a function call
  Cell* lambda = eval(car(expr));
  if (!isFunc(lambda))
    err << "not a function call: " << expr << endl << DIE;

  // eval all its args in the current lexical scope
  Cell* realArgs = reorderKeywordArgs(sig(lambda), callArgs(expr));
  Cell* evaldArgs = evalArgs(sig(lambda), realArgs);

  // swap in the function's lexical environment
  if (!isPrimFunc(car(lambda)))
    newDynamicScope(L"currLexicalScope",
        newCons(calleeEnv(lambda), currLexicalScopes.top()));
  // now bind its params to args in the new environment
  newLexicalScope();
  bindArgs(sig(lambda), evaldArgs);

  // eval all forms in body, save result of final form
  Cell* result = nil;
  if (isPrimFunc(car(lambda)))
    result = toPrimFunc(car(lambda))(); // all primFuncs must mkref result
  else
    for (Cell* form = calleeBody(lambda); form != nil; form = cdr(form)) {
      rmref(result);
      result = eval(car(form));
    }

  endLexicalScope();
  if (!isPrimFunc(car(lambda)))
    endDynamicScope(L"currLexicalScope");
  rmref(evaldArgs);
  rmref(realArgs);
  rmref(lambda);
  return result; // already mkref'd
}
