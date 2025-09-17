import subprocess

def run_style_check(file_path):
    result = {"violations": 0, "score": 1.0, "output": ""}

    try:
        proc = subprocess.run(
            ["perl", "checkpatch.pl", "--no-tree", "--file", file_path],
            capture_output=True,
            text=True
        )
        result["output"] = proc.stdout
        result["violations"] = result["output"].count("WARNING") + result["output"].count("ERROR")
        result["score"] = max(0, 1 - (result["violations"] / 20))  # crude formula
    except Exception as e:
        result["output"] = str(e)
        result["score"] = 0.0

    return result
