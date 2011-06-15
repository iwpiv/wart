all: x.cc test_list
	g++ x.cc

test_list: x.cc
	grep "void test" *.cc |perl -pwe 's/^\s*void (.*)\(\) {$$/$$1,/' > test_list
