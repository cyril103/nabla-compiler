CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
PYTHON ?= python
SRC ?= tests/test_import.nabla
BUILD_DIR := build
BIN := $(BUILD_DIR)/$(notdir $(basename $(SRC)))

all: nablac

nablac: src/main.cpp src/parser.cpp src/ast.cpp src/semantic_analyzer.cpp src/ir.cpp src/ir_codegen.cpp src/runtime_asm.cpp src/lexer.hpp src/ast.hpp src/parser.hpp src/semantic_analyzer.hpp src/ir.hpp src/ir_codegen.hpp src/runtime_asm.hpp src/compiler_context.hpp src/compiler_error.hpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) src/main.cpp src/parser.cpp src/ast.cpp src/semantic_analyzer.cpp src/ir.cpp src/ir_codegen.cpp src/runtime_asm.cpp -o $(BUILD_DIR)/nablac

$(BUILD_DIR)/frontend_unit_tests: tests/frontend_unit_tests.cpp src/parser.cpp src/ast.cpp src/semantic_analyzer.cpp src/ir.cpp src/lexer.hpp src/ast.hpp src/parser.hpp src/semantic_analyzer.hpp src/ir.hpp src/compiler_context.hpp src/compiler_error.hpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) tests/frontend_unit_tests.cpp src/parser.cpp src/ast.cpp src/semantic_analyzer.cpp src/ir.cpp -o $(BUILD_DIR)/frontend_unit_tests

unit-tests: $(BUILD_DIR)/frontend_unit_tests
	@$(BUILD_DIR)/frontend_unit_tests

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
					args_file=$${testfile%.nabla}.args; \
					test_args=""; \
					if [ -f "$$args_file" ]; then test_args=$$(cat "$$args_file"); fi; \
					if [ -f "$$stdout_file" ]; then \
						"$$executable" $$test_args >"$$actual_stdout" 2>/dev/null; \
					else \
						"$$executable" $$test_args >/dev/null 2>&1; \
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
								"$$executable" $$test_args >"$$actual_stdout" 2>/dev/null; \
							else \
								"$$executable" $$test_args >/dev/null 2>&1; \
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

examples-quick: nablac
	@mkdir -p $(BUILD_DIR)/examples
	@all_status=0; \
	for testfile in examples/*.nabla; do \
		echo "===== $$testfile ====="; \
		NABLA_BUILD_DIR=$(BUILD_DIR) $(BUILD_DIR)/nablac "$$testfile" >/dev/null 2>&1; \
		compile_status=$$?; \
		if [ $$compile_status -ne 0 ]; then \
			echo "FAIL: $$testfile (compilation status=$$compile_status)"; \
			all_status=1; \
		else \
			echo "PASS: $$testfile"; \
		fi; \
		echo; \
	done; \
	exit $$all_status

examples: nablac
	@mkdir -p $(BUILD_DIR)/examples
	@all_status=0; \
	for testfile in examples/*.nabla; do \
		echo "===== $$testfile ====="; \
		executable=$(BUILD_DIR)/$$(basename "$$testfile" .nabla); \
		NABLA_BUILD_DIR=$(BUILD_DIR) $(BUILD_DIR)/nablac "$$testfile" >/dev/null 2>&1; \
		compile_status=$$?; \
		if [ $$compile_status -ne 0 ]; then \
			echo "FAIL: $$testfile (compilation status=$$compile_status)"; \
			all_status=1; \
		elif [ -f "$${testfile%.nabla}.expected" ]; then \
			expected=$$(tr -d '[:space:]' < "$${testfile%.nabla}.expected"); \
			stdout_file=$${testfile%.nabla}.stdout; \
			actual_stdout=$(BUILD_DIR)/examples/$$(basename "$$testfile" .nabla).stdout; \
			if [ -f "$$stdout_file" ]; then \
				"$$executable" >"$$actual_stdout" 2>/dev/null; \
			else \
				"$$executable" >/dev/null 2>&1; \
			fi; \
			run_status=$$?; \
			if [ "$$run_status" = "$$expected" ]; then \
				echo "PASS: $$testfile (exit=$$run_status)"; \
			else \
				echo "FAIL: $$testfile (exit=$$run_status, expected=$$expected)"; \
				all_status=1; \
			fi; \
			if [ -f "$$stdout_file" ]; then \
				if cmp -s "$$actual_stdout" "$$stdout_file"; then \
					echo "PASS (stdout): $$testfile"; \
				else \
					echo "FAIL (stdout): $$testfile"; \
					diff -u "$$stdout_file" "$$actual_stdout" || true; \
					all_status=1; \
				fi; \
				rm -f "$$actual_stdout"; \
			fi; \
		else \
			echo "PASS (compiled): $$testfile (no expected files)"; \
		fi; \
		echo; \
	done; \
	exit $$all_status

examples-full: examples

format:
	$(PYTHON) tools/format_sources.py

format-check:
	$(PYTHON) tools/format_sources.py --check

tooling-tests: nablac unit-tests stdlib-docs format-check
	@tests/test_missing_external_tools.sh
	@tests/test_configurable_heap_size.sh
	@tests/test_heap_overflow_diagnostic.sh
	@tests/test_heap_stats_builtins.sh
	@tests/test_gc_active_collection.sh
	@tests/test_gc_runtime_metrics.sh
	@tests/test_gc_detailed_metrics.sh
	@tests/test_gc_heap_accounting_metrics.sh
	@tests/test_gc_candidate_scan_metrics.sh
	@tests/test_gc_interior_candidate_scan_metrics.sh
	@tests/test_gc_free_list_splitting.sh
	@tests/test_gc_memory_stress.sh
	@tests/test_gc_frame_root_metadata.sh
	@tests/test_gc_heap_layout_metadata.sh
	@tests/test_gc_alloc_call_metadata.sh
	@tests/test_gc_static_root_metadata.sh
	@$(PYTHON) tests/test_gc_runtime_helper_alloc_inventory.py
	@$(PYTHON) tests/test_gc_runtime_helper_root_maps.py
	@$(PYTHON) tests/test_gc_runtime_helper_root_spills.py
	@$(PYTHON) tests/test_gc_inventory_docs.py
	@tests/test_stdlib_docs_html.py
	@$(PYTHON) tools/test_editor_vscode.py
	@$(PYTHON) tests/test_format_sources.py

stdlib-docs:
	tools/generate_stdlib_docs.py

debug: nablac
	@mkdir -p $(BUILD_DIR)
	NABLA_BUILD_DIR=$(BUILD_DIR) $(BUILD_DIR)/nablac --keep-asm $(SRC)
	-@./$(BIN); status=$$?; echo "Program exit status: $$status"

clean:
	rm -rf $(BUILD_DIR) nablac

.PHONY: all clean test debug all-tests examples examples-quick examples-full unit-tests tooling-tests format format-check stdlib-docs
