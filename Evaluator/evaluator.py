from compile_checker import run_compilation
from parser import analyze_code_structure
from style_checker import run_style_check
from security_checker import run_security_check
from scoring import calculate_score
from reporter import generate_report
from logger import log_score   # ✅ import logger

import sys
def main(file_path):
    results = {}

    # 1. Compile the driver
    results["compilation"] = run_compilation(file_path)

    # 2. Parse code structure
    results["structure"] = analyze_code_structure(file_path)

    # 3. Style compliance
    results["style"] = run_style_check(file_path)

    # 4. Security checks
    results["security"] = run_security_check(file_path)

    # 5. Scoring
    final_score, breakdown = calculate_score(results)
    results["overall_score"] = final_score
    results["breakdown"] = breakdown   # ✅ attach breakdown

    # 6. Reporting
    generate_report(results, file_path)

    # 7. Logging
    log_score(file_path, breakdown, final_score)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python evaluator.py <driver.c>")
        sys.exit(1)
    main(sys.argv[1])
