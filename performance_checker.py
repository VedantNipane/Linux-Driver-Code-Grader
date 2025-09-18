# performance_checker.py
import re

def run_performance_check(file_path, structure):
    """
    Lightweight static performance heuristics.
    Returns dict:
      {
        "score": float (0..1),
        "details": [ ... ]
      }
    """

    metrics = {"score": 1.0, "details": []}
    perf = 10.0
    details = []

    avg_len = structure.get("avg_func_len", 0.0)
    if avg_len > 100:
        penalty = min(3.0, (avg_len - 100) / 50.0)
        perf -= penalty
        details.append(f"avg_func_len {avg_len:.1f} -> -{penalty:.1f}")

    complexity_hits = 0
    try:
        code_text = open(file_path, "r").read()
        complexity_hits = len(re.findall(r"\b(if|for|while|switch|goto)\b", code_text))
        if complexity_hits > 20:
            penalty = min(3.0, (complexity_hits - 20) / 20.0)
            perf -= penalty
            details.append(f"complexity_hits {complexity_hits} -> -{penalty:.1f}")
    except Exception:
        pass

    try:
        if "kmalloc" in code_text and ("1024" in code_text or "4096" in code_text):
            perf -= 2.0
            details.append("large kmalloc detected -> -2.0")
    except Exception:
        pass

    perf = max(0.0, min(10.0, perf))
    metrics["score"] = perf / 10.0  # normalize 0..1
    metrics["details"] = details
    return metrics
