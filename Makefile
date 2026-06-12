CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

all: nablac

nablac: src/main.cpp src/lexer.hpp src/ast.hpp src/parser.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o nablac

clean:
	rm -f nablac tests/test_import

.PHONY: all clean
