def score_runtime(runtime_results):
    """
    Assign points for runtime behavior.
    Max: 10 points under Correctness.
    """
    score = 0.0
    details = []

    if not runtime_results:
        details.append("Runtime skipped")
        return score, details

    if runtime_results.get("compiled"):
        score += 2.0
        details.append("compiled OK (+2)")
    if runtime_results.get("loaded"):
        score += 4.0
        details.append("loaded OK (+4)")
    if runtime_results.get("unloaded"):
        score += 2.0
        details.append("unloaded OK (+2)")
    if runtime_results.get("dmesg_success"):
        score += 2.0
        details.append("dmesg output OK (+2)")

    if not details:
        details.append(
            f"failed to load, notes: {runtime_results.get('runtime_notes','')}"
        )

    return score, details


def score_correctness(results):
    breakdown = {"awarded": 0.0, "max": 40.0, "details": []}

    # Compilation
    comp = results.get("compilation", {})
    if comp.get("success"):
        breakdown["awarded"] += 30.0
        breakdown["details"].append(
            f"Compilation: success (method={comp.get('method')})"
        )
        if comp.get("note"):
            breakdown["details"].append(comp.get("note"))
    else:
        breakdown["details"].append("Compilation failed")

    # Functionality
    structure = results.get("structure", {})
    func_score = structure.get("functionality_score", 0.0)
    func_awarded = round(func_score * 10.0, 2)
    breakdown["awarded"] += func_awarded
    breakdown["details"].append(
        f"Functionality score: {func_score:.2f} -> {func_awarded:.2f}/10"
    )

    # Runtime
    runtime_results = results.get("runtime", {})
    rt_awarded, rt_details = score_runtime(runtime_results)
    breakdown["awarded"] += rt_awarded
    breakdown["details"].append(f"Runtime score: {rt_awarded}/10 ({', '.join(rt_details)})")

    # Cap at max
    breakdown["awarded"] = min(breakdown["awarded"], breakdown["max"])
    return breakdown


def score_security(results):
    sec = results.get("security", {})
    sec_score = sec.get("score", 1.0)
    sec_awarded = round(sec_score * 25.0, 2)
    breakdown = {"awarded": sec_awarded, "max": 25.0, "details": []}
    if sec.get("sub_scores"):
        breakdown["details"].append(f"sub_scores: {sec.get('sub_scores')}")
    if sec.get("issues"):
        breakdown["details"].append(f"issues: {sec.get('issues')}")
    return breakdown


def score_code_quality(results):
    style = results.get("style", {})
    style_score = style.get("style_score", 1.0)
    doc_score = style.get("documentation_score", 1.0)
    maintain_score = style.get("maintainability_score", 1.0)
    cq_normalized = (style_score * 0.4) + (doc_score * 0.3) + (maintain_score * 0.3)
    cq_awarded = round(cq_normalized * 20.0, 2)
    breakdown = {"awarded": cq_awarded, "max": 20.0, "details": []}
    breakdown["details"].append(
        {
            "style_score": round(style_score, 3),
            "documentation_score": round(doc_score, 3),
            "maintainability_score": round(maintain_score, 3),
        }
    )
    return breakdown


def score_performance(results):
    perf = results.get("performance", {})
    perf_score = perf.get("score", 1.0)
    perf_awarded = round(perf_score * 10.0, 2)
    return {
        "awarded": perf_awarded,
        "max": 10.0,
        "details": perf.get("details", []),
    }


def score_advanced(results):
    adv_score = 0.0
    adv_details = []
    fp = results.get("meta_file")
    if fp:
        try:
            txt = open(fp).read()
            if "devm_" in txt:
                adv_score += 1.5
                adv_details.append("devm_* used")
            if "of_match_table" in txt or "of_device_id" in txt or "of_" in txt:
                adv_score += 1.5
                adv_details.append("device-tree support")
            if "suspend" in txt or "resume" in txt or "pm_ops" in txt:
                adv_score += 1.0
                adv_details.append("pm hooks")
            if "debugfs" in txt or "pr_debug" in txt:
                adv_score += 1.0
                adv_details.append("debug helpers")
        except Exception:
            pass
    adv_awarded = round(min(5.0, adv_score), 2)
    return {"awarded": adv_awarded, "max": 5.0, "details": adv_details}


def calculate_score(results):
    breakdown = {
        "Correctness": score_correctness(results),
        "Security": score_security(results),
        "Code Quality": score_code_quality(results),
        "Performance": score_performance(results),
        "Advanced": score_advanced(results),
    }

    total_awarded = sum(bd["awarded"] for bd in breakdown.values())
    final_score = round(total_awarded, 2)
    return final_score, breakdown
