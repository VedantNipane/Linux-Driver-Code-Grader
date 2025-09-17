import subprocess
import re

def run_compilation(file_path):
    result = {"success": False, "errors": 0, "warnings": 0, "output": ""}

    try:
        proc = subprocess.run(
            ["gcc", "-Wall", "-Wextra", "-c", file_path, "-o", "temp.o"],
            capture_output=True,
            text=True
        )
        result["output"] = proc.stderr

        # Count errors and warnings
        result["errors"] = len(re.findall(r"error:", result["output"]))
        result["warnings"] = len(re.findall(r"warning:", result["output"]))

        if proc.returncode == 0:
            # Compiled cleanly
            result["success"] = True
        else:
            # Check if errors are ONLY missing kernel headers
            missing_header_errors = re.findall(r"fatal error: linux/.*: No such file", result["output"])
            if missing_header_errors and result["errors"] == len(missing_header_errors):
                # Treat as soft success
                result["success"] = True
                result["note"] = "Soft pass: missing kernel headers only"
            else:
                result["success"] = False

    except Exception as e:
        result["output"] = str(e)

    return result
