PYTHON = python3
EVALUATOR = evaluator.py
TEST_DIR = Tests
TESTS = $(wildcard $(TEST_DIR)/*)

.PHONY: all run clean

all: run

run:
	@for test in $(TESTS); do \
		echo "Running $(EVALUATOR) on $$test"; \
		$(PYTHON) $(EVALUATOR) $$test; \
	done

clean:
	@echo "Cleaning up..."
	@rm -f *.pyc
