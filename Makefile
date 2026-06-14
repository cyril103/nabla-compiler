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
		compile_status=$$?; \
		case "$$testfile" in \
			*error*|*fail*) \
				if [ $$compile_status -ne 0 ]; then \
					echo "${GREEN}PASS (expected compilation failure):${NC} $$testfile"; \
				else \
					echo "${RED}FAIL:${NC} $$testfile (expected compilation failure but succeeded)"; \
					all_status=1; \
				fi; \
				;; \
			*) \
				expected_file=$${testfile%.nabla}.expected; \
				if [ $$compile_status -ne 0 ]; then \
					echo "${RED}FAIL:${NC} $$testfile (compilation status=$$compile_status)"; \
					all_status=1; \
				elif [ ! -f "$$expected_file" ]; then \
					echo "${RED}FAIL:${NC} $$testfile (missing $$expected_file)"; \
					all_status=1; \
				else \
					executable=$(BUILD_DIR)/$$(basename "$$testfile" .nabla); \
					"$$executable" >/dev/null 2>&1; \
					run_status=$$?; \
					expected=$$(tr -d '[:space:]' < "$$expected_file"); \
					if [ "$$run_status" = "$$expected" ]; then \
						echo "${GREEN}PASS:${NC} $$testfile (exit=$$run_status)"; \
					else \
						echo "${RED}FAIL:${NC} $$testfile (exit=$$run_status, expected=$$expected)"; \
						all_status=1; \
					fi; \
				fi; \
				;; \
		esac; \
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
