# evaluator.py
from compile_checker import run_compilation
from parser import analyze_code_structure
from style_checker import run_style_check
from security_checker import run_security_check
from performance_checker import run_performance_check
from scoring import calculate_score
from reporter import generate_report
from logger import log_score

import sys
import os

def main(file_path):
    results = {}

    # Ensure file exists
    if not os.path.exists(file_path):
        print(f"File not found: {file_path}")
        return

    # 1. Compile the driver
    results["compilation"] = run_compilation(file_path)

    # 2. Parse code structure
    results["structure"] = analyze_code_structure(file_path)

    # 3. Style compliance
    results["style"] = run_style_check(file_path)

    # 4. Security checks
    results["security"] = run_security_check(file_path)

    # Attach meta file path for advanced heuristics
    results["meta_file"] = file_path

    # 5. Performance checks
    results["performance"] = run_performance_check(file_path, results["structure"])

    # 6. Scoring
    final_score, breakdown = calculate_score(results)

    results["overall_score"] = final_score
    results["breakdown"] = breakdown

    # Reporting
    # generate_report(results, file_path)

    # Logging
    log_score(file_path, results, final_score,breakdown)



if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python evaluator.py <driver.c>")
        sys.exit(1)
    main(sys.argv[1])
