import json
import os

def generate_report(results, file_path):
    base_name = os.path.basename(file_path).replace(".c", "")
    report_path = os.path.join("outputs", f"{base_name}_results.json")

    os.makedirs("outputs", exist_ok=True)
    with open(report_path, "w") as f:
        json.dump(results, f, indent=4)

    print("\n=== Evaluation Report ===")
    print(f"File: {file_path}")

    # Compilation
    if results["compilation"]["success"]:
        note = results["compilation"].get("note", "")
        if note:
            print(f"Compilation: Success ({note})")
        else:
            print("Compilation: Success")
    else:
        print("Compilation: Failed")

    print(f"Warnings: {results['compilation']['warnings']} Errors: {results['compilation']['errors']}")

    # Detailed scoring breakdown
    print("\n--- Score Breakdown ---")
    print(f"Correctness: {(30 if results['compilation']['success'] else 0) + results['structure']['score']*10:.1f}/40")
    print(f"Security: {results['security']['score']*25:.1f}/25")
    print(f"Code Quality: {results['style']['score']*20:.1f}/20")
    print(f"Performance: 5/10 (placeholder)")
    print(f"Advanced: 0/5 (placeholder)")

    print("\n--- Issues ---")
    # print(f"Style Violations: {results['style']['violations']}")
    print(f"Security Issues: {results['security']['unsafe_functions']}")

    print(f"\nOverall Score: {results['overall_score']}/100")
    print(f"Report saved to: {report_path}")
