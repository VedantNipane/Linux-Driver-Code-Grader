import re

def run_security_check(file_path):
    metrics = {"unsafe_functions": [], "score": 1.0}

    unsafe_patterns = ["strcpy", "sprintf", "gets"]
    with open(file_path, "r") as f:
        code = f.read()

    for func in unsafe_patterns:
        if func in code:
            metrics["unsafe_functions"].append(func)

    # Deduct score for each unsafe function
    metrics["score"] = max(0, 1 - (0.3 * len(metrics["unsafe_functions"])))

    return metrics
