def score_runtime(runtime_results):
    """
    Assign points for runtime behavior (part of Correctness).
    Max: 10 points.
    """
    if not runtime_results:
        return 0.0, "Runtime not executed"

    score = 0.0
    details = []

    if runtime_results.get("compiled"):
        score += 2.0
        details.append("compiled OK (+2)")
    if runtime_results.get("loaded"):
        score += 4.0
        details.append("loaded OK (+4)")
    else:
        details.append("failed to load")
    if runtime_results.get("unloaded"):
        score += 2.0
        details.append("unloaded OK (+2)")
    if runtime_results.get("dmesg_success"):
        score += 2.0
        details.append("dmesg captured (+2)")

    if runtime_results.get("runtime_notes"):
        details.append(f"notes: {runtime_results['runtime_notes']}")

    return min(score, 10.0), ", ".join(details)


def score_correctness(results):
    breakdown = {"awarded": 0.0, "max": 40.0, "details": []}

    # Compilation (20)
    comp = results.get("compilation", {})
    if comp.get("success"):
        breakdown["awarded"] += 20.0
        breakdown["details"].append(
            f"Compilation: success (method={comp.get('method')})"
        )
        if comp.get("note"):
            breakdown["details"].append(comp.get("note"))
    else:
        breakdown["details"].append("Compilation failed")

    # Functionality (10)
    structure = results.get("structure", {})
    func_score = structure.get("functionality_score", 0.0)
    func_awarded = round(func_score * 10.0, 2)
    breakdown["awarded"] += func_awarded
    breakdown["details"].append(
        f"Functionality score: {func_score:.2f} -> {func_awarded:.2f}/10"
    )

    # Runtime (10)
    runtime = results.get("runtime", {})
    rt_score, rt_details = score_runtime(runtime)
    rt_awarded = round(rt_score, 2)  # runtime returns 0..10 already
    breakdown["awarded"] += rt_awarded
    breakdown["details"].append(f"Runtime score: {rt_awarded}/10 ({rt_details})")

    # Ensure not exceeding max (safety check)
    if breakdown["awarded"] > breakdown["max"]:
        breakdown["awarded"] = breakdown["max"]

    return breakdown

def score_security(results):
    breakdown = {"awarded": 0.0, "max": 25.0, "details": []}
    sec = results.get("security", {})
    sec_score = sec.get("score", 1.0)
    breakdown["awarded"] = round(sec_score * 25.0, 2)
    if sec.get("sub_scores"):
        breakdown["details"].append(f"sub_scores: {sec['sub_scores']}")
    if sec.get("issues"):
        breakdown["details"].append(f"issues: {sec['issues']}")
    return breakdown


def score_code_quality(results):
    breakdown = {"awarded": 0.0, "max": 20.0, "details": []}
    style = results.get("style", {})
    style_score = style.get("style_score", 1.0)
    doc_score = style.get("documentation_score", 1.0)
    maintain_score = style.get("maintainability_score", 1.0)

    cq_normalized = (style_score * 0.4) + (doc_score * 0.3) + (maintain_score * 0.3)
    breakdown["awarded"] = round(cq_normalized * 20.0, 2)
    breakdown["details"].append({
        "style_score": round(style_score, 3),
        "documentation_score": round(doc_score, 3),
        "maintainability_score": round(maintain_score, 3)
    })
    return breakdown


def score_performance(results):
    breakdown = {"awarded": 0.0, "max": 10.0, "details": []}
    perf = results.get("performance", {})
    perf_score = perf.get("score", 1.0)
    breakdown["awarded"] = round(perf_score * 10.0, 2)
    breakdown["details"] = perf.get("details", [])
    return breakdown


def score_advanced(results):
    breakdown = {"awarded": 0.0, "max": 5.0, "details": []}
    adv_score = 0.0
    adv_present = []
    fp = results.get("meta_file")

    if fp:
        try:
            txt = open(fp).read()
            if "devm_" in txt:
                adv_score += 1.5
                adv_present.append("devm_* used")
            if "of_match_table" in txt or "of_device_id" in txt or "of_" in txt:
                adv_score += 1.5
                adv_present.append("device-tree support")
            if "suspend" in txt or "resume" in txt or "pm_ops" in txt:
                adv_score += 1.0
                adv_present.append("pm hooks")
            if "debugfs" in txt or "pr_debug" in txt:
                adv_score += 1.0
                adv_present.append("debug helpers")
        except Exception:
            pass

    breakdown["awarded"] = round(min(5.0, adv_score), 2)
    if adv_present:
        breakdown["details"].append(adv_present)
    return breakdown


def calculate_score(results):
    breakdown = {
        "Correctness": score_correctness(results),
        "Security": score_security(results),
        "Code Quality": score_code_quality(results),
        "Performance": score_performance(results),
        "Advanced": score_advanced(results),
    }

    total_awarded = sum(section["awarded"] for section in breakdown.values())
    final_score = round(total_awarded, 2)

    return final_score, breakdown
