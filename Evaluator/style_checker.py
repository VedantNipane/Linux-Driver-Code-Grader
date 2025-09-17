import subprocess
import re
# def run_style_check(file_path):
    # result = {"violations": 0, "score": 1.0, "output": ""}

    # try:
    #     proc = subprocess.run(
    #         ["perl", "checkpatch.pl", "--no-tree", "--file", file_path],
    #         capture_output=True,
    #         text=True
    #     )
    #     result["output"] = proc.stdout
    #     result["violations"] = result["output"].count("WARNING") + result["output"].count("ERROR")
    #     result["score"] = max(0, 1 - (result["violations"] / 100))  # crude formula, can be tweaked
    # except Exception as e:
    #     result["output"] = str(e)
    #     result["score"] = 0.0

    # return result

def run_style_check(file_path):
    """Comprehensive structure validation for Linux device drivers"""
    with open(file_path, "r") as f:
        code_content = f.read()
    checks = {}
    score_components = []
    
    # Essential includes (weight: 20%)
    required_includes = [
        'linux/module.h',
        'linux/kernel.h', 
        'linux/init.h'
    ]
    
    include_score = 0
    for include in required_includes:
        if f'#include <{include}>' in code_content:
            include_score += 1
    checks['includes'] = include_score / len(required_includes)
    score_components.append(('includes', 0.2))
    
    # Module metadata (weight: 15%)
    metadata_items = ['MODULE_LICENSE', 'MODULE_AUTHOR', 'MODULE_DESCRIPTION']
    metadata_score = sum(1 for item in metadata_items if item in code_content) / len(metadata_items)
    checks['metadata'] = metadata_score
    score_components.append(('metadata', 0.15))
    
    # Core driver functions (weight: 35%)
    core_functions = ['module_init', 'module_exit']
    core_score = sum(1 for func in core_functions if func in code_content) / len(core_functions)
    checks['core_functions'] = core_score
    score_components.append(('core_functions', 0.35))
    
    # Device operations (weight: 20%) - for character devices
    device_ops = ['open', 'release', 'read', 'write']
    ops_found = sum(1 for op in device_ops if f'device_{op}' in code_content or f'.{op}' in code_content)
    checks['device_operations'] = min(1.0, ops_found / 2)  # At least 2 operations expected
    score_components.append(('device_operations', 0.2))
    
    # Error handling patterns (weight: 10%)
    error_patterns = ['return -E', 'if (', 'goto err', 'cleanup']
    error_handling = sum(1 for pattern in error_patterns if pattern in code_content) / len(error_patterns)
    checks['error_handling'] = error_handling
    score_components.append(('error_handling', 0.1))
    
    # Calculate weighted score
    total_score = sum(checks[component] * weight for component, weight in score_components)
    
    return {
        "checks": checks,
        "score": total_score,
        "function_count": len(re.findall(r'^\s*\w+\s+\w+\s*\([^)]*\)\s*{', code_content, re.MULTILINE)),
        "details": {
            "includes_found": include_score,
            "metadata_found": metadata_score * len(metadata_items),
            "core_functions_found": core_score * len(core_functions),
            "device_ops_found": ops_found,
            "error_handling_indicators": error_handling * len(error_patterns)
        }
    }