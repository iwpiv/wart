//// run unit tests or interactive interpreter

// The design of lisp seems mindful of the following:
//  keep programs as short as possible
//  as programs grow, allow them to be decomposed into functions
//  functions are defined with a recipe for work, and invoked to perform that work
//  functions are parameterized with vars (parameters) and invoked with data (arguments)
//  new functions can be defined at any time
//  functions can invoke other functions before they've been defined
//  functions are data; they can be passed to other functions
//  when an invocation contains multiple functions, make the one being invoked easy to identify
//    almost always the first symbol in the invocation
//  there have to be *some* bedrock primitives, but any primitive can be user-defined in principle
//
// These weren't the reasons lisp was created; they're the reasons I attribute
// to its power.

bool Interactive = false;

int main(int argc, unused char* argv[]) {
  if (argc > 1) {
    run_tests();
    return 0;
  }

  //// Interactive loop: parse commands from user, evaluate them, print the results
  Interactive = true;
  setup();
  load_files(".wart");
  cout << "ready! type in an expression, then hit enter twice. ctrl-d exits.\n";
  while (!cin.eof()) {
    cout << "=> " << run(cin) << '\n';
  }
}

//// read: tokenize, segment, parse, build cells
cell* read(istream& in) {
  return mkref(next_cell(in));
}

// In batch mode, evaluate all exprs in input.
// In interactive mode, evaluate all exprs until empty line.
// Return value of last expr.
cell* run(istream& in) {
  cell* result = NULL;
  do {
    cell* form = read(in);
    result = eval(form);
    rmref(form);
  } while (!eof(in) && (!Interactive || in.peek() != '\n'));
  return result;
}

bool eof(istream& in) {
  in.peek();
  return in.eof();
}



//// test harness

void run_tests() {
  Running_tests = true;
  Pretend_raise = true;  // for death tests
  time_t t; time(&t);
  cerr << "C tests: " << ctime(&t);
  for (unsigned long i=0; i < sizeof(Tests)/sizeof(Tests[0]); ++i) {
    START_TRACING_UNTIL_END_OF_SCOPE;
    setup();
    (*Tests[i])();
    verify();
  }

  Pretend_raise = false;
  setup();
  load_files(".wart");   // after GC tests
  load_files(".test");

  cerr << '\n';
  if (Num_failures > 0)
    cerr << Num_failures << " failure"
         << (Num_failures > 1 ? "s" : "")
         << '\n';
}

void verify() {
  teardown_bindings();
  if (!Passed) return;
  if (Raise_count != 0) cerr << Raise_count << " errors encountered\n";
}

void setup() {
  setup_cells();
  setup_common_syms();
  Raise_count = 0;
  Passed = true;
}



//// helpers for tests

list<cell*> read_all(string s) {
  stringstream in(s);
  list<cell*> results;
  do {
    results.push_back(read(in));
  } while (!eof(in));
  return results;
}

cell* run(string s) {
  stringstream in(s);
  return run(in);
}
