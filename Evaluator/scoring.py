def calculate_score(results):
    breakdown = {}

    # Correctness (40%)
    correctness = 0
    if results["compilation"]["success"]:
        correctness += 30
    correctness += results["structure"]["score"] * 10
    breakdown["Correctness"] = correctness

    # Security (25%)
    security = results["security"]["score"] * 25
    breakdown["Security"] = security

    # Code Quality (20%)
    quality = results["style"]["score"] * 20
    breakdown["Code Quality"] = quality

    # Performance (10%) - placeholder
    performance = 5
    breakdown["Performance"] = performance

    # Advanced (5%) - placeholder
    advanced = 0
    breakdown["Advanced"] = advanced

    # Final
    final_score = round(sum(breakdown.values()), 2)

    return final_score, breakdown

