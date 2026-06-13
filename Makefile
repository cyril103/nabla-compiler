CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
SRC ?= tests/test_import.nabla
BUILD_DIR := build
BIN := $(BUILD_DIR)/$(notdir $(basename $(SRC)))

all: nablac

nablac: src/main.cpp src/parser.cpp src/ast.cpp src/lexer.hpp src/ast.hpp src/parser.hpp src/compiler_context.hpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) src/main.cpp src/parser.cpp src/ast.cpp -o $(BUILD_DIR)/nablac

test: nablac
	@mkdir -p $(BUILD_DIR)
	NABLA_BUILD_DIR=$(BUILD_DIR) $(BUILD_DIR)/nablac $(SRC)
	@asmfile=$(BUILD_DIR)/$$(basename $(SRC) .nabla)_tmp.asm; \
	if [ -f "$$asmfile" ]; then \
		echo "\n--- Contenu de $$asmfile ---"; \
		cat "$$asmfile"; \
	else \
		echo "Aucun fichier ASM trouvé: $$asmfile"; \
	fi
	-@./$(BIN); status=$$?; echo "Program exit status: $$status"

all-tests: nablac
	@mkdir -p $(BUILD_DIR)
	@all_status=0; \
	GREEN='\033[1;32m'; RED='\033[1;31m'; NC='\033[0m'; \
	for testfile in tests/*.nabla; do \
		echo "===== $$testfile ====="; \
		NABLA_BUILD_DIR=$(BUILD_DIR) $(BUILD_DIR)/nablac --keep-temp "$$testfile" >/dev/null 2>&1; \
		status=$$?; \
		case "$$testfile" in \
			*error*|*fail*) \
				expected=1; \
				;; \
			*) \
				expected=0; \
				;; \
		esac; \
		if [ $$status -eq $$expected ]; then \
			if [ $$expected -eq 0 ]; then \
				echo "${GREEN}PASS:${NC} $$testfile"; \
			else \
				echo "${GREEN}PASS (expected failure):${NC} $$testfile"; \
			fi; \
		else \
			if [ $$expected -eq 0 ]; then \
				echo "${RED}FAIL:${NC} $$testfile (status=$$status)"; \
			else \
				echo "${RED}FAIL:${NC} $$testfile (expected failure but succeeded)"; \
			fi; \
			all_status=1; \
		fi; \
		echo; \
	done; \
	exit $$all_status

debug: nablac
	@mkdir -p $(BUILD_DIR)
	NABLA_BUILD_DIR=$(BUILD_DIR) $(BUILD_DIR)/nablac --keep-asm $(SRC)
	-@./$(BIN); status=$$?; echo "Program exit status: $$status"

clean:
	rm -rf $(BUILD_DIR) nablac

.PHONY: all clean test debug all-tests
