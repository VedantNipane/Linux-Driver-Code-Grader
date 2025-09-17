import re

# def run_security_check(file_path):
#     metrics = {"unsafe_functions": [], "score": 1.0}

#     unsafe_patterns = ["strcpy", "sprintf", "gets"]
#     with open(file_path, "r") as f:
#         code = f.read()

#     for func in unsafe_patterns:
#         if func in code:
#             metrics["unsafe_functions"].append(func)

#     # kernel-specific risky usage
#     if "copy_to_user" in code and not re.search(r"copy_to_user\s*\([^,]+,[^,]+,.*len", code):
#         metrics["unsafe_functions"].append("copy_to_user_unchecked")

#     if "copy_from_user" in code and not re.search(r"copy_from_user\s*\([^,]+,[^,]+,.*len", code):
#         metrics["unsafe_functions"].append("copy_from_user_unchecked")

#     # Deduct score for each unsafe function
#     penalty = 0.2*len(metrics["unsafe_functions"])
#     metrics["score"] = max(0, 1 - penalty)

#     return metrics

def run_security_check(file_path):
    metrics = {
        "issues": [],
        "sub_scores": {
            "memory_safety": 1.0,
            "resource_mgmt": 1.0,
            "race_conditions": 1.0,
            "input_validation": 1.0,
        },
        "score": 1.0
    }

    with open(file_path, "r") as f:
        code = f.read()

    # -------------------------
    # 1. Memory Safety
    # -------------------------
    unsafe_patterns = ["strcpy", "sprintf", "gets"]
    for func in unsafe_patterns:
        if func in code:
            metrics["issues"].append(f"unsafe_function:{func}")
            metrics["sub_scores"]["memory_safety"] -= 0.3

    # Look for fixed-size buffers (possible overflow risk)
    if re.search(r"char\s+\w+\s*\[\d+\];", code):
        metrics["issues"].append("fixed_buffer_array")
        metrics["sub_scores"]["memory_safety"] -= 0.2

    # -------------------------
    # 2. Resource Management
    # -------------------------
    allocs = len(re.findall(r"k[mz]alloc", code)) + len(re.findall(r"vmalloc", code))
    frees = len(re.findall(r"kfree", code)) + len(re.findall(r"vfree", code))
    if allocs > frees:
        metrics["issues"].append("possible_memory_leak")
        metrics["sub_scores"]["resource_mgmt"] -= 0.3

    # -------------------------
    # 3. Race Conditions
    # -------------------------
    if "mutex_lock" in code and "mutex_unlock" not in code:
        metrics["issues"].append("mutex_not_unlocked")
        metrics["sub_scores"]["race_conditions"] -= 0.4

    if "spin_lock" in code and "spin_unlock" not in code:
        metrics["issues"].append("spinlock_not_unlocked")
        metrics["sub_scores"]["race_conditions"] -= 0.4

    # -------------------------
    # 4. Input Validation
    # -------------------------
    # copy_to_user / copy_from_user without explicit length arg
    if "copy_to_user" in code and not re.search(r"copy_to_user\s*\([^,]+,[^,]+,.*len", code):
        metrics["issues"].append("copy_to_user_unchecked")
        metrics["sub_scores"]["input_validation"] -= 0.3

    if "copy_from_user" in code and not re.search(r"copy_from_user\s*\([^,]+,[^,]+,.*len", code):
        metrics["issues"].append("copy_from_user_unchecked")
        metrics["sub_scores"]["input_validation"] -= 0.3

    # user pointer dereference without validation
    if re.search(r"\*\s*__user", code):
        metrics["issues"].append("unchecked_user_pointer")
        metrics["sub_scores"]["input_validation"] -= 0.3

    # -------------------------
    # Normalize sub-scores
    # -------------------------
    for k in metrics["sub_scores"]:
        if metrics["sub_scores"][k] < 0:
            metrics["sub_scores"][k] = 0.0

    # Overall score = average of subs
    metrics["score"] = sum(metrics["sub_scores"].values()) / len(metrics["sub_scores"])

    return metrics
