from compile_checker import run_compilation
from parser import analyze_code_structure
from style_checker import run_style_check
# from security_checker import run_security_check
# from scoring import calculate_score
# from reporter import generate_report

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
    results["overall_score"] = calculate_score(results)

    # 6. Reporting
    generate_report(results, file_path)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python evaluator.py <driver.c>")
        sys.exit(1)
    main(sys.argv[1])
