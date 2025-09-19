# parser.py
import re

def _count_functions_and_lengths(code):
    """
    Naive function parser: locate occurrences of function headers and measure
    approximate lengths. Returns list of function lengths (in lines).
    """
    # This is deliberately simple and regex-based.
    func_header_re = re.compile(r"^\s*([a-zA-Z_][\w\s\*]+)\s+([a-zA-Z_]\w*)\s*\(([^)]*)\)\s*\{", re.MULTILINE)
    funcs = []
    for m in func_header_re.finditer(code):
        start = m.start()
        # Find matching closing brace naively by counting braces from start
        brace_count = 0
        lines = code[start:].splitlines()
        line_count = 0
        for line in lines:
            line_count += 1
            brace_count += line.count("{")
            brace_count -= line.count("}")
            if brace_count <= 0:
                break
        funcs.append(max(1, line_count))
    return funcs

def analyze_code_structure(file_path):
    """
    Extracts metadata:
      - module_init/module_exit presence
      - function_count
      - average function length
      - driver_type (char/platform/block/net/unknown)
      - functionality_score [0..1] based on presence of expected symbols for driver type
    """
    metrics = {
        "module_init": False,
        "module_exit": False,
        "function_count": 0,
        "avg_func_len": 0.0,
        "driver_type": "unknown",
        "functionality_score": 0.0,
        "fops_present": [],
    }

    with open(file_path, "r") as f:
        code = f.read()

    # module init/exit
    if re.search(r"\bmodule_init\s*\(", code):
        metrics["module_init"] = True
    if re.search(r"\bmodule_exit\s*\(", code):
        metrics["module_exit"] = True

    # function lengths and count
    func_lengths = _count_functions_and_lengths(code)
    metrics["function_count"] = len(func_lengths)
    if func_lengths:
        metrics["avg_func_len"] = sum(func_lengths) / len(func_lengths)

    # Detect driver type heuristically and compute functionality_score
    functionality_hits = 0
    functionality_needed = 0

    # Character driver heuristics
    if ("struct file_operations" in code) or ("register_chrdev" in code) or ("register_chrdev_region" in code):
        metrics["driver_type"] = "char"
        # desired fields
        needed = {"read", "write", "open", "release"}
        found = set(re.findall(r"\.([a-z_]+)\s*=", code))
        present = list(needed & found)
        metrics["fops_present"] = present
        functionality_needed = len(needed)
        functionality_hits = len(present)

    # Platform driver heuristics
    elif ("platform_driver_register" in code) or ("platform_driver" in code) or ("ioremap" in code):
        metrics["driver_type"] = "platform"
        checks = {
            "ioremap": "ioremap" in code,
            "request_irq": "request_irq" in code,
            "platform_register": "platform_driver_register" in code,
            "device_tree": ("of_match_table" in code or "of_device_id" in code or "of_find_compatible_node" in code)
        }
        functionality_needed = len(checks)
        functionality_hits = sum(1 for v in checks.values() if v)

    # Block driver heuristics
    elif ("register_blkdev" in code) or ("request_queue" in code) or ("gendisk" in code):
        metrics["driver_type"] = "block"
        checks = {
            "register_blkdev": "register_blkdev" in code,
            "request_queue": "request_queue" in code or "blk_alloc_queue" in code,
            "disk_ops": "gendisk" in code or "submit_bio" in code
        }
        functionality_needed = len(checks)
        functionality_hits = sum(1 for v in checks.values() if v)

    # Network driver heuristics
    elif ("register_netdev" in code) or ("alloc_netdev" in code) or ("ndo_start_xmit" in code) or ("net_device_ops" in code):
        metrics["driver_type"] = "net"
        checks = {
            "register_netdev": "register_netdev" in code or "alloc_netdev" in code,
            "ndo_start_xmit": "ndo_start_xmit" in code or "ndo_start_xmit" in code,
            "net_ops": "net_device_ops" in code or "net_device" in code
        }
        functionality_needed = len(checks)
        functionality_hits = sum(1 for v in checks.values() if v)

    # Generic fallback: reward module_init/module_exit and having >=1 functions
    if metrics["driver_type"] == "unknown":
        functionality_needed = 2  # module init/exit
        functionality_hits = (1 if metrics["module_init"] else 0) + (1 if metrics["module_exit"] else 0)

    # compute normalized functionality_score
    if functionality_needed > 0:
        metrics["functionality_score"] = min(1.0, functionality_hits / float(functionality_needed))
    else:
        metrics["functionality_score"] = 0.0

    return metrics
