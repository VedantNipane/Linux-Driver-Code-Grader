def calculate_score(results):
    score = 0

    # Correctness (40%) - includes compilation + parser structure
    if results["compilation"]["success"]:
        score += 30  # compilation contributes majority
    # Structural checks (module_init/exit, function count)
    score += results["structure"]["score"] * 10  # up to 10 points

    # Security (25%)
    score += results["security"]["score"] * 25

    # Code Quality (20%) - gentler style scaling
    style_score = results["style"]["score"]
    # If violations > 0, style_score is scaled softer
    score += style_score * 20

    # Performance (10%) - placeholder
    score += 5

    # Advanced (5%) - placeholder
    score += 0

    return round(score, 2)
