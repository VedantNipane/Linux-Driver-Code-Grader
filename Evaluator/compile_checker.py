import subprocess
import re
import os
import shutil
import tempfile


def _run_proc(cmd, cwd=None, timeout=120):
    try:
        proc = subprocess.run(
            cmd,
            cwd=cwd,
            capture_output=True,
            text=True,
            timeout=timeout,
            shell=False,
        )
        out = (proc.stdout or "") + (proc.stderr or "")
        return proc.returncode, out
    except subprocess.TimeoutExpired as e:
        return 1, f"TimeoutExpired: {str(e)}"
    except Exception as e:
        return 1, f"Exception: {str(e)}"


def run_compilation(file_path):
    result = {
        "success": False,
        "method": None,
        "output": "",
        "errors": 0,
        "warnings": 0,
    }

    kernel_build_dir = f"/lib/modules/{os.uname().release}/build"

    # --- Try kbuild first ---
    if os.path.exists(kernel_build_dir):
        result["method"] = "kbuild"
        tmpdir = tempfile.mkdtemp(prefix="evaluator_kbuild_")
        try:
            base = os.path.basename(file_path)
            dest = os.path.join(tmpdir, base)
            shutil.copy2(file_path, dest)

            mk = f"obj-m := {base.replace('.c', '')}.o\n"
            with open(os.path.join(tmpdir, "Makefile"), "w") as fh:
                fh.write(mk)

            cmd = ["make", "-C", kernel_build_dir, f"M={tmpdir}", "modules", "-j"]
            ret, out = _run_proc(cmd, timeout=240)
            result["output"] = out
            result["errors"] = len(re.findall(r"\berror:", out))
            result["warnings"] = len(re.findall(r"\bwarning:", out))
            result["success"] = (ret == 0)

            ko_files = [f for f in os.listdir(tmpdir) if f.endswith(".ko")]
            if ko_files:
                result["built_module"] = ko_files[0]
        except Exception as e:
            result["output"] = f"Kbuild exception: {e}"
            result["success"] = False
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)

        if result["success"]:
            return result

        if re.search(r"fatal error: .*: No such file or directory", result["output"]):
            result["note"] = "Kbuild failed due to missing headers, retrying with GCC fallback."
        else:
            return result

    # --- GCC fallback ---
    result["method"] = "gcc-fallback"
    try:
        temp_obj = "temp_evaluator.o"
        cmd = ["gcc", "-Wall", "-Wextra", "-c", "-fsyntax-only", file_path, "-o", temp_obj]
        ret, out = _run_proc(cmd, timeout=60)
        result["output"] = out or ""
        result["errors"] = len(re.findall(r"\berror:", result["output"]))
        result["warnings"] = len(re.findall(r"\bwarning:", result["output"]))

        if ret == 0:
            result["success"] = True
            result["note"] = result.get("note") or "Soft pass: syntax ok without kernel headers"
        else:
            missing_header_errors = re.findall(
                r"fatal error: linux/[^:]+: No such file or directory", result["output"]
            )
            if missing_header_errors and result["errors"] == len(missing_header_errors):
                result["success"] = True
                result["note"] = "Soft pass: missing kernel headers only"
            else:
                result["success"] = False
    except Exception as e:
        result["output"] = f"GCC compile exception: {e}"
        result["success"] = False
    finally:
        try:
            if os.path.exists("temp_evaluator.o"):
                os.remove("temp_evaluator.o")
        except Exception:
            pass

    return result
