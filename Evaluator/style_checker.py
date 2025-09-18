# style_checker.py
import subprocess
import re
import os

CHECKPATCH = "./checkpatch.pl"  # prefer local copy in repo root

def _run_checkpatch(file_path):
    """Run checkpatch.pl if available and return raw output."""
    if os.path.exists(CHECKPATCH):
        try:
            proc = subprocess.run(
                ["perl", CHECKPATCH, "--no-tree", "--file", file_path],
                capture_output=True, text=True, timeout=60
            )
            return proc.stdout or proc.stderr or ""
        except Exception as e:
            return f"checkpatch exception: {e}"
    else:
        # fallback: simple heuristics if checkpatch not available
        try:
            with open(file_path, "r") as f:
                return f.read()
        except Exception as e:
            return f"fallback read exception: {e}"

def run_style_check(file_path):
    """
    Returns dict:
      {
        violations: int,
        style_score: float,        # 0..1
        documentation_score: float,# 0..1
        maintainability_score:float,#0..1
        output: raw_checkpatch_output
      }
    """
    result = {
        "violations": 0,
        "style_score": 1.0,
        "documentation_score": 1.0,
        "maintainability_score": 1.0,
        "output": ""
    }

    raw = _run_checkpatch(file_path)
    result["output"] = raw

    # If checkpatch output contains WARNING/ERROR, count them
    w = len(re.findall(r"WARNING:", raw)) + len(re.findall(r"WARNING:", raw.lower()))
    e = len(re.findall(r"ERROR:", raw)) + len(re.findall(r"ERROR:", raw.lower()))
    # also count "WARNING" or "warning"
    generic_w = len(re.findall(r"\bwarning:", raw)) + len(re.findall(r"\bNOTE:", raw))
    violations = w + e + generic_w

    # fallback heuristic: look for tabs, line length > 80
    if violations == 0:
        # Lines too long
        try:
            with open(file_path, "r") as fh:
                lines = fh.readlines()
            long_lines = sum(1 for L in lines if len(L.rstrip("\n")) > 80)
            tab_lines = sum(1 for L in lines if "\t" in L)
            violations += long_lines + tab_lines
        except Exception:
            pass

    result["violations"] = violations

    # style_score: gentle scaling
    result["style_score"] = max(0.0, 1.0 - (violations / 100.0))

    # Documentation heuristics:
    # + Check for MODULE_* macros
    doc_points = 0
    try:
        with open(file_path, "r") as fh:
            code = fh.read()
        if "MODULE_LICENSE" in code and "MODULE_AUTHOR" in code and "MODULE_DESCRIPTION" in code:
            doc_points += 0.5
        # count function-level comment occurrences: '/*' before a function header
        func_comment_hits = len(re.findall(r"/\*[^*]*\*/\s*[a-zA-Z_].*\(", code, flags=re.DOTALL))
        # crude: map hits to 0..0.5
        doc_points += min(0.5, 0.1 * func_comment_hits)
    except Exception:
        pass
    result["documentation_score"] = min(1.0, doc_points)

    # Maintainability heuristics: average function length penalty
    try:
        func_lengths = []
        # find approximate function bodies using braces pair heuristic
        func_headers = re.finditer(r"^\s*[a-zA-Z_][\w\s\*]+\s+[a-zA-Z_]\w*\s*\([^)]*\)\s*\{", open(file_path).read(), re.MULTILINE)
        code_text = open(file_path).read()
        for h in func_headers:
            start = h.start()
            brace_count = 0
            lines = code_text[start:].splitlines()
            count = 0
            for line in lines:
                count += 1
                brace_count += line.count("{")
                brace_count -= line.count("}")
                if brace_count <= 0:
                    break
            func_lengths.append(max(1, count))
        if func_lengths:
            avg_len = sum(func_lengths) / len(func_lengths)
            # normalize: shorter functions -> higher score
            result["maintainability_score"] = max(0.0, 1.0 - (avg_len / 500.0))
        else:
            result["maintainability_score"] = 1.0
    except Exception:
        result["maintainability_score"] = 1.0

    return result
