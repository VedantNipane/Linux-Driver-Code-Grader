import subprocess

def run_compilation(file_path):
    result = {"success": False, "errors": 0, "warnings": 0, "output": ""}

    try:
        proc = subprocess.run(
            ["gcc", "-Wall", "-Wextra", "-c", file_path, "-o", "temp.o"],
            capture_output=True,
            text=True
        )
        result["output"] = proc.stderr

        if proc.returncode == 0:
            result["success"] = True
        else:
            # Count errors and warnings in output
            result["errors"] = result["output"].count("error:")
            result["warnings"] = result["output"].count("warning:")
    except Exception as e:
        result["output"] = str(e)

    return result
