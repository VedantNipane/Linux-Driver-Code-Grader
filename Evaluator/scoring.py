# scoring.py
def calculate_score(results):
    """
    Returns: (final_score_float, breakdown_dict)

    breakdown_dict structure:
    {
      "Correctness": {"awarded": X, "max": 40.0, "details": [...]},
      "Security": {"awarded": Y, "max": 25.0, "details": [...]},
      "Code Quality": {"awarded": Z, "max": 20.0, "details": [...]},
      "Performance": {"awarded": P, "max": 10.0, "details": [...]},
      "Advanced": {"awarded": A, "max": 5.0, "details": [...]}
    }
    """
    breakdown = {
        "Correctness": {"awarded": 0.0, "max": 40.0, "details": []},
        "Security": {"awarded": 0.0, "max": 25.0, "details": []},
        "Code Quality": {"awarded": 0.0, "max": 20.0, "details": []},
        "Performance": {"awarded": 0.0, "max": 10.0, "details": []},
        "Advanced": {"awarded": 0.0, "max": 5.0, "details": []}
    }

    # -------- Correctness (40) --------
    # Compilation contributes up to 30
    comp = results.get("compilation", {})
    if comp.get("success"):
        breakdown["Correctness"]["awarded"] += 30.0
        breakdown["Correctness"]["details"].append(f"Compilation: success (method={comp.get('method')})")
        if comp.get("note"):
            breakdown["Correctness"]["details"].append(comp.get("note"))
    else:
        breakdown["Correctness"]["details"].append("Compilation failed")

    # Functionality / kernel integration contributes up to 10 (use parser functionality_score)
    structure = results.get("structure", {})
    func_score = structure.get("functionality_score", 0.0)
    func_awarded = round(func_score * 10.0, 2)
    breakdown["Correctness"]["awarded"] += func_awarded
    breakdown["Correctness"]["details"].append(f"Functionality score: {func_score:.2f} -> {func_awarded:.2f}/10")

    # -------- Security (25) --------
    # security module already returns an averaged score 0..1
    sec = results.get("security", {})
    sec_score = sec.get("score", 1.0)
    sec_awarded = round(sec_score * 25.0, 2)
    breakdown["Security"]["awarded"] = sec_awarded
    # include sub-scores if present
    sub = sec.get("sub_scores")
    if sub:
        breakdown["Security"]["details"].append(f"sub_scores: {sub}")
    if sec.get("issues"):
        breakdown["Security"]["details"].append(f"issues: {sec.get('issues')}")

    # -------- Code Quality (20) --------
    style = results.get("style", {})
    style_score = style.get("style_score", 1.0)
    doc_score = style.get("documentation_score", 1.0)
    maintain_score = style.get("maintainability_score", 1.0)

    # Weights within Code Quality: style 40%, doc 30%, maintain 30%
    cq_normalized = (style_score * 0.4) + (doc_score * 0.3) + (maintain_score * 0.3)
    cq_awarded = round(cq_normalized * 20.0, 2)
    breakdown["Code Quality"]["awarded"] = cq_awarded
    breakdown["Code Quality"]["details"].append({
        "style_score": round(style_score, 3),
        "documentation_score": round(doc_score, 3),
        "maintainability_score": round(maintain_score, 3)
    })

    # -------- Performance (10) --------
    perf = results.get("performance", {})
    perf_score = perf.get("score", 1.0)
    perf_awarded = round(perf_score * 10.0, 2)
    breakdown["Performance"]["awarded"] = perf_awarded
    breakdown["Performance"]["details"] = perf.get("details", [])

    # -------- Advanced (5) - feature checks (devm, of, pm hooks etc.) --------
    adv = 0.0
    adv_details = []
    try:
        # detect devm_ allocation usage
        with open(results.get("meta_file", ""), "r") as _:
            pass
    except Exception:
        pass
    # We'll inspect structure and style outputs to infer advanced features
    # Devm usage (reward 1.5), device tree (1.5), pm hooks (1.0), debug helpers (1.0)
    adv_score = 0.0
    # heuristics from results (structure and style raw outputs present)
    # Check for devm_
    try:
        # we expect the caller to have file path in structure (not mandatory)
        pass
    except Exception:
        pass

    # Simpler heuristics by reading file if possible via results (structure doesn't have file path, so skip heavy checks)
    # We'll attempt to give 0–2 points if obvious tokens present in style->output or structure
    # The evaluator can set results["meta_file"] = path to allow advanced detection; skip if not provided.
    adv_present = []
    # try to read raw file if caller attached it (optional)
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

    adv_awarded = round(min(5.0, adv_score), 2)
    breakdown["Advanced"]["awarded"] = adv_awarded
    if adv_present:
        breakdown["Advanced"]["details"].append(adv_present)

    # -------- Final score calculation --------
    total_awarded = sum(breakdown[k]["awarded"] for k in breakdown)
    final_score = round(total_awarded, 2)

    return final_score, breakdown
