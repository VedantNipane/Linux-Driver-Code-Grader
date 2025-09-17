import json
import os

def generate_report(results, file_path):
    report_path = os.path.join("outputs", "results.json")

    os.makedirs("outputs", exist_ok=True)
    with open(report_path, "w") as f:
        json.dump(results, f, indent=4)

    print("\n=== Evaluation Report ===")
    print(f"File: {file_path}")

    # Handle soft pass notes in compilation
    if results["compilation"]["success"]:
        note = results["compilation"].get("note", "")
        if note:
            print(f"Compilation: Success ({note})")
        else:
            print("Compilation: Success")
    else:
        print("Compilation: Failed")

    print(f"Warnings: {results['compilation']['warnings']} Errors: {results['compilation']['errors']}")
    print(f"Style Violations: {results['style']['violations']} (Score: {results['style']['score']:.2f})")
    print(f"Security Issues: {results['security']['unsafe_functions']} (Score: {results['security']['score']:.2f})")

    print(f"Overall Score: {results['overall_score']}/100")
