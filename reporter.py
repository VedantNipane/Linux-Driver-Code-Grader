# reporter.py
import json
import os

def generate_report(results, file_path):
    """
    Expects results to contain:
      - overall_score (float)
      - breakdown (from scoring.calculate_score)
      - security (with sub_scores and issues)
      - style (with violations etc.)
      - compilation
    """
    base_name = os.path.basename(file_path).replace(".c", "")
    report_path = os.path.join("outputs", f"{base_name}_results.json")

    os.makedirs("outputs", exist_ok=True)
    # Save raw results
    to_save = results.copy()
    with open(report_path, "w") as f:
        json.dump(to_save, f, indent=4)

    # Pretty print
    print("\n=== Evaluation Report ===")
    print(f"File: {file_path}")

    comp = results.get("compilation", {})
    if comp.get("success"):
        note = comp.get("note", "")
        if note:
            print(f"Compilation: Success ({note})")
        else:
            print("Compilation: Success")
    else:
        print("Compilation: Failed")
    print(f"Warnings: {comp.get('warnings', 0)} Errors: {comp.get('errors', 0)} (method={comp.get('method')})")

    # Breakdown
    print("\n--- Score Breakdown ---")
    bd = results.get("breakdown", {})
    for cat in ["Correctness", "Security", "Code Quality", "Performance", "Advanced"]:
        entry = bd.get(cat, {})
        awarded = entry.get("awarded", 0.0)
        maxv = entry.get("max", 0.0)
        print(f"{cat}: {awarded:.1f}/{maxv:.0f}")
        details = entry.get("details", [])
        if details:
            # print a compact summary of details
            if isinstance(details, list):
                for d in details:
                    print(f"  - {d}")
            else:
                print(f"  - {details}")

    # Security sub-score details if present
    security = results.get("security", {})
    sub_scores = security.get("sub_scores")
    if sub_scores:
        print("\n--- Security Breakdown ---")
        for k, v in sub_scores.items():
            print(f"{k.replace('_',' ').title()}: {v:.2f}")
        if security.get("issues"):
            print(f"Issues: {security.get('issues')}")

    # Style / violations
    style = results.get("style", {})
    print("\n--- Style ---")
    print(f"Violations: {style.get('violations', 0)}")
    print(f"Style Score: {style.get('style_score', 0.0):.2f}")
    print(f"Documentation Score: {style.get('documentation_score', 0.0):.2f}")
    print(f"Maintainability Score: {style.get('maintainability_score', 0.0):.2f}")

    print(f"\nOverall Score: {results.get('overall_score', 0.0):.1f}/100")
    print(f"Report saved to: {report_path}")
