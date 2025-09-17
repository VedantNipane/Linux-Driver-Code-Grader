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

# Quick additions to your security checker (if needed)

# def run_security_check(file_path):
#     """Enhanced security checks for Linux driver code"""
    
#     with open(file_path, "r") as f:
#         code_content = f.read()

#     security_issues = []
    
#     # Current check (you already have this)
#     unsafe_functions = ['strcpy', 'sprintf', 'gets', 'strcat']
    
#     # Additional critical checks for kernel code
#     kernel_security_patterns = [
#         # Memory safety
#         (r'sprintf\s*\(', 'sprintf - use snprintf instead'),
#         (r'strcat\s*\(', 'strcat - potential buffer overflow'),
#         (r'gets\s*\(', 'gets - extremely unsafe'),
        
#         # Kernel-specific issues
#         (r'kmalloc\s*\([^)]*\)(?![^;]*kfree)', 'potential memory leak - kmalloc without kfree'),
#         (r'copy_from_user\s*\([^)]*\)(?![^}]*return)', 'copy_from_user without error checking'),
#         (r'copy_to_user\s*\([^)]*\)(?![^}]*return)', 'copy_to_user without error checking'),
        
#         # Locking issues
#         (r'spin_lock\s*\([^)]*\)(?![^}]*spin_unlock)', 'spin_lock without unlock'),
#         (r'mutex_lock\s*\([^)]*\)(?![^}]*mutex_unlock)', 'mutex_lock without unlock'),
        
#         # Input validation
#         (r'(\w+)\s*=\s*copy_from_user.*?if\s*\(\s*\1\s*>', 'good: checking copy_from_user return'),
#     ]
    
#     for pattern, description in kernel_security_patterns:
#         if re.search(pattern, code_content, re.MULTILINE):
#             security_issues.append(description)
    
#     # Calculate score based on issues found
#     base_score = 1.0
#     penalty_per_issue = 0.1
#     final_score = max(0.0, base_score - (len(security_issues) * penalty_per_issue))
    
#     return {
#         "issues": security_issues,
#         "score": final_score,
#         "unsafe_functions": [func for func in unsafe_functions if func in code_content]
#     }
