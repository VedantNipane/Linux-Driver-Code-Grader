import re

def analyze_code_structure(file_path):
    metrics = {"module_init": False,
                "module_exit": False,
                "function_count": 0,
                "score":0.0 # structural score contribution
                }

    with open(file_path, "r") as f:
        code = f.read()

    # Look for init/exit
    if re.search(r"module_init\s*\(", code):
        metrics["module_init"] = True
    if re.search(r"module_exit\s*\(", code):
        metrics["module_exit"] = True

    # Count functions (very naive regex)
    metrics["function_count"] = len(re.findall(r"\w+\s+\w+\s*\([^)]*\)\s*{", code))

    # Assign structural score
    score = 0
    if metrics["module_init"]:
        score += 0.5
    if metrics["module_exit"]:
        score += 0.5
    if metrics["function_count"] >= 2:
        score += 0.5
    metrics["score"] = min(score, 1.0)  # normalize [0–1]

    return metrics
