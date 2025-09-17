import re

def run_security_check(file_path):
    metrics = {"unsafe_functions": [], "score": 1.0}

    unsafe_patterns = ["strcpy", "sprintf", "gets"]
    with open(file_path, "r") as f:
        code = f.read()

    for func in unsafe_patterns:
        if func in code:
            metrics["unsafe_functions"].append(func)

    # kernel-specific risky usage
    if "copy_to_user" in code and not re.search(r"copy_to_user\s*\([^,]+,[^,]+,.*len", code):
        metrics["unsafe_functions"].append("copy_to_user_unchecked")

    if "copy_from_user" in code and not re.search(r"copy_from_user\s*\([^,]+,[^,]+,.*len", code):
        metrics["unsafe_functions"].append("copy_from_user_unchecked")

    # Deduct score for each unsafe function
    penalty = 0.2*len(metrics["unsafe_functions"])
    metrics["score"] = max(0, 1 - penalty)

    return metrics
