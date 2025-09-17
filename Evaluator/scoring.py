def calculate_score(results):
    score = 0

    # Correctness (40%)
    if results["compilation"]["success"]:
        score += 40
    else:
        score += 0

    # Security (25%)
    score += results["security"]["score"] * 25

    # Code Quality (20%)
    score += results["style"]["score"] * 20

    # Performance (10%) -> placeholder
    score += 5

    # Advanced (5%) -> placeholder
    score += 0

    return round(score, 2)
