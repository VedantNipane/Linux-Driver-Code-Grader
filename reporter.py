import json
import os


def _print_compilation(results):
    comp = results.get("compilation", {})
    if comp.get("success"):
        note = comp.get("note", "")
        if note:
            print(f"Compilation: Success ({note})")
        else:
            print("Compilation: Success")
    else:
        print("Compilation: Failed")

    print(
        f"Warnings: {comp.get('warnings', 0)} "
        f"Errors: {comp.get('errors', 0)} "
        f"(method={comp.get('method')})"
    )


def _print_breakdown(breakdown):
    print("\n--- Score Breakdown ---")
    for cat in ["Correctness", "Security", "Code Quality", "Performance", "Advanced"]:
        entry = breakdown.get(cat, {})
        awarded = entry.get("awarded", 0.0)
        maxv = entry.get("max", 0.0)
        print(f"{cat}: {awarded:.1f}/{maxv:.0f}")

        details = entry.get("details", [])
        if details:
            if isinstance(details, list):
                for d in details:
                    print(f"  - {d}")
            else:
                print(f"  - {details}")


def _print_security(results):
    security = results.get("security", {})
    sub_scores = security.get("sub_scores")
    if sub_scores:
        print("\n--- Security Breakdown ---")
        for k, v in sub_scores.items():
            print(f"{k.replace('_',' ').title()}: {v:.2f}")
        if security.get("issues"):
            print(f"Issues: {security.get('issues')}")


def _print_style(results):
    style = results.get("style", {})
    print("\n--- Style ---")
    print(f"Violations: {style.get('violations', 0)}")
    print(f"Style Score: {style.get('style_score', 0.0):.2f}")
    print(f"Documentation Score: {style.get('documentation_score', 0.0):.2f}")
    print(f"Maintainability Score: {style.get('maintainability_score', 0.0):.2f}")


def generate_report(results, file_path):
    """
    Expects results to contain:
      - overall_score (float)
      - breakdown (from scoring.calculate_score)
      - compilation
      - security (with sub_scores and issues)
      - style (with violations etc.)
      - runtime (optional: compiled/loaded/unloaded/dmesg_success)
    """
    base_name = os.path.basename(file_path).replace(".c", "")
    report_path = os.path.join("outputs", f"{base_name}_results.json")
    os.makedirs("outputs", exist_ok=True)

    # Save raw results
    with open(report_path, "w") as f:
        json.dump(results, f, indent=4)

    # Pretty print
    print("\n=== Evaluation Report ===")
    print(f"File: {file_path}")

    _print_compilation(results)

    # Breakdown (includes runtime inside Correctness)
    _print_breakdown(results.get("breakdown", {}))

    # Security breakdown
    _print_security(results)

    # Style section
    _print_style(results)

    print(f"\nOverall Score: {results.get('overall_score', 0.0):.1f}/100")
    print(f"Report saved to: {report_path}")
