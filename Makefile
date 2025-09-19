PYTHON      = python3
EVALUATOR   = Evaluator/evaluator.py
TEST_DIR    = Tests
TESTS       = $(wildcard $(TEST_DIR)/*.c)

.PHONY: all run clean summary

all: run

# Run evaluator on all .c files in Tests/
run:
	@for test in $(TESTS); do \
		echo "==> Running $(EVALUATOR) on $$test"; \
		$(PYTHON) $(EVALUATOR) $$test; \
	done

# Show summary of last CSV log
summary:
	@echo "=== Latest Evaluation Summary (from score_logs.csv) ==="
	@tail -n 5 score_logs.csv || echo "No logs yet."

# Clean outputs and temporary files
clean:
	@echo "Cleaning up..."
	@rm -f *.pyc
	@rm -f */*.pyc
	@rm -rf __pycache__ */__pycache__
	@rm -f score_logs.csv
	@rm -rf outputs
