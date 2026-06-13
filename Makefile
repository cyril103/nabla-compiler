CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
SRC ?= tests/test_import.nabla
BIN := $(notdir $(basename $(SRC)))

all: nablac

nablac: src/main.cpp src/parser.cpp src/ast.cpp src/lexer.hpp src/ast.hpp src/parser.hpp src/compiler_context.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp src/parser.cpp src/ast.cpp -o nablac

test: nablac
	./nablac $(SRC)
	@asmfile=$$(basename $(SRC) .nabla)_tmp.asm; \
	if [ -f "$$asmfile" ]; then \
		echo "\n--- Contenu de $$asmfile ---"; \
		cat "$$asmfile"; \
	else \
		echo "Aucun fichier ASM trouvé: $$asmfile"; \
	fi
	-@./$(BIN); status=$$?; echo "Program exit status: $$status"

debug: nablac
	./nablac --keep-asm $(SRC)
	-@./$(BIN); status=$$?; echo "Program exit status: $$status"

clean:
	rm -f nablac tests/test_import

.PHONY: all clean test debug
