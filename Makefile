CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
SRC ?= tests/test_import.nabla
BUILD_DIR := build
BIN := $(BUILD_DIR)/$(notdir $(basename $(SRC)))

all: nablac

nablac: src/main.cpp src/parser.cpp src/ast.cpp src/semantic_analyzer.cpp src/ir.cpp src/ir_codegen.cpp src/runtime_asm.cpp src/lexer.hpp src/ast.hpp src/parser.hpp src/semantic_analyzer.hpp src/ir.hpp src/ir_codegen.hpp src/runtime_asm.hpp src/compiler_context.hpp src/compiler_error.hpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) src/main.cpp src/parser.cpp src/ast.cpp src/semantic_analyzer.cpp src/ir.cpp src/ir_codegen.cpp src/runtime_asm.cpp -o $(BUILD_DIR)/nablac

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
		diagnostic_file=$(BUILD_DIR)/$$(basename "$$testfile" .nabla).diagnostic; \
		NABLA_BUILD_DIR=$(BUILD_DIR) $(BUILD_DIR)/nablac --keep-temp "$$testfile" >"$$diagnostic_file" 2>&1; \
		compile_status=$$?; \
		case "$$testfile" in \
			*error*|*fail*) \
				if [ $$compile_status -ne 0 ]; then \
					expected_diagnostic=$${testfile%.nabla}.diagnostic; \
					if [ -f "$$expected_diagnostic" ]; then \
						sed "s|$(CURDIR)/||g" "$$diagnostic_file" > "$$diagnostic_file.normalized"; \
						if cmp -s "$$diagnostic_file.normalized" "$$expected_diagnostic"; then \
							echo "${GREEN}PASS (expected diagnostic):${NC} $$testfile"; \
						else \
							echo "${RED}FAIL:${NC} $$testfile (diagnostic mismatch)"; \
							diff -u "$$expected_diagnostic" "$$diagnostic_file.normalized" || true; \
							all_status=1; \
						fi; \
						rm -f "$$diagnostic_file.normalized"; \
					else \
						echo "${GREEN}PASS (expected compilation failure):${NC} $$testfile"; \
					fi; \
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
					stdout_file=$${testfile%.nabla}.stdout; \
					actual_stdout=$(BUILD_DIR)/$$(basename "$$testfile" .nabla).stdout; \
					if [ -f "$$stdout_file" ]; then \
						"$$executable" >"$$actual_stdout" 2>/dev/null; \
					else \
						"$$executable" >/dev/null 2>&1; \
					fi; \
					run_status=$$?; \
					expected=$$(tr -d '[:space:]' < "$$expected_file"); \
					if [ "$$run_status" = "$$expected" ]; then \
						echo "${GREEN}PASS:${NC} $$testfile (exit=$$run_status)"; \
					else \
						echo "${RED}FAIL:${NC} $$testfile (exit=$$run_status, expected=$$expected)"; \
						all_status=1; \
					fi; \
					if [ -f "$$stdout_file" ]; then \
						if cmp -s "$$actual_stdout" "$$stdout_file"; then \
							echo "${GREEN}PASS (stdout):${NC} $$testfile"; \
						else \
							echo "${RED}FAIL:${NC} $$testfile (stdout mismatch)"; \
							diff -u "$$stdout_file" "$$actual_stdout" || true; \
							all_status=1; \
						fi; \
						rm -f "$$actual_stdout"; \
					fi; \
					expected_ir=$${testfile%.nabla}.ir; \
					if [ -f "$$expected_ir" ]; then \
						actual_ir=$(BUILD_DIR)/$$(basename "$$testfile" .nabla).ir; \
						NABLA_BUILD_DIR=$(BUILD_DIR) $(BUILD_DIR)/nablac --emit-ir "$$testfile" >"$$actual_ir" 2>&1; \
						ir_status=$$?; \
						if [ $$ir_status -eq 0 ] && cmp -s "$$actual_ir" "$$expected_ir"; then \
							echo "${GREEN}PASS (IR snapshot):${NC} $$testfile"; \
						else \
							echo "${RED}FAIL:${NC} $$testfile (IR snapshot mismatch)"; \
							diff -u "$$expected_ir" "$$actual_ir" || true; \
							all_status=1; \
						fi; \
						rm -f "$$actual_ir"; \
					fi; \
					expected_ir_backend=$${testfile%.nabla}.ir-backend.expected; \
					if [ -f "$$expected_ir_backend" ]; then \
						NABLA_BUILD_DIR=$(BUILD_DIR) $(BUILD_DIR)/nablac --backend-ir "$$testfile" >/dev/null 2>&1; \
						ir_backend_status=$$?; \
						if [ $$ir_backend_status -eq 0 ]; then \
							if [ -f "$$stdout_file" ]; then \
								"$$executable" >"$$actual_stdout" 2>/dev/null; \
							else \
								"$$executable" >/dev/null 2>&1; \
							fi; \
							ir_backend_run_status=$$?; \
							expected_ir_backend_status=$$(tr -d '[:space:]' < "$$expected_ir_backend"); \
							if [ "$$ir_backend_run_status" = "$$expected_ir_backend_status" ]; then \
								echo "${GREEN}PASS (IR backend):${NC} $$testfile (exit=$$ir_backend_run_status)"; \
							else \
								echo "${RED}FAIL:${NC} $$testfile (IR backend exit=$$ir_backend_run_status, expected=$$expected_ir_backend_status)"; \
								all_status=1; \
							fi; \
							if [ -f "$$stdout_file" ]; then \
								if cmp -s "$$actual_stdout" "$$stdout_file"; then \
									echo "${GREEN}PASS (IR backend stdout):${NC} $$testfile"; \
								else \
									echo "${RED}FAIL:${NC} $$testfile (IR backend stdout mismatch)"; \
									diff -u "$$stdout_file" "$$actual_stdout" || true; \
									all_status=1; \
								fi; \
								rm -f "$$actual_stdout"; \
							fi; \
						else \
							echo "${RED}FAIL:${NC} $$testfile (IR backend compilation status=$$ir_backend_status)"; \
							all_status=1; \
						fi; \
					fi; \
				fi; \
				;; \
		esac; \
		rm -f "$$diagnostic_file"; \
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
