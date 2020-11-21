CXXFLAGS := -O2 -std=c++17
LDFLAGS := -Wl,--strip-all

all: test.out test2.out test3.out test4.out

%.out: %.cc ValOrErr.hpp
	$(CXX) $< $(CXXFLAGS) $(LDFLAGS) -o $@
	./$@

test2.out: test2.cc ValOrErr.hpp
	$(CXX) test2.cc $(CXXFLAGS) -fno-exceptions $(LDFLAGS) -o $@
	(./$@ && exit 1) || exit 0

clean:
	rm -f *.out

gendoc:
	doxygen Doxyfile
	rsync -av doc/html/ docs

cleandoc:
	rm -rf docs/*

.PHONY: clean all
