void test_pointers_from_nil_are_nil() {
  checkEq(nil->car, nil);
  checkEq(nil->cdr, nil);
}

void test_newCell_has_nil_car_and_cdr() {
  Cell* x = newCell();
  checkEq(x->car, nil);
  checkEq(x->cdr, nil);
  rmref(x);
}

void test_rmref_frees_space() {
  Cell* c = newCell();
  checkEq(c->car, nil);
  checkEq(freelist, NULL);
  rmref(c);
  check(!c->car);
  checkEq(freelist, c);
}

void test_rmref_handles_nums() {
  Cell* c = newNum(34);
  rmref(c);
  check(!c->car);
  checkEq(freelist, c);
}

void test_nthCdr() {
  Cell* x = newCons(newNum(3), newCons(newNum(4), nil));
  checkEq(nthCdr(x, 0), x);
  checkEq(car(nthCdr(x, 1)), newNum(4));
  checkEq(nthCdr(x, 2), nil);
  rmref(x);
}

void test_num_types_take_up_same_space_as_Cell() {
  checkEq(sizeof(int), sizeof(Cell*));
  checkEq(sizeof(long), sizeof(Cell*));
  checkEq(sizeof(float), sizeof(Cell*));
  checkEq(sizeof(size_t), sizeof(Cell*));
}
