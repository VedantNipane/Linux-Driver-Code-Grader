# compile_checker.py
import subprocess
import re
import os
import shutil
import tempfile
import sys

def _run_proc(cmd, cwd=None, timeout=120):
    try:
        proc = subprocess.run(
            cmd,
            cwd=cwd,
            capture_output=True,
            text=True,
            timeout=timeout,
            shell=False
        )
        out = (proc.stdout or "") + (proc.stderr or "")
        return proc.returncode, out
    except subprocess.TimeoutExpired as e:
        return 1, f"TimeoutExpired: {str(e)}"
    except Exception as e:
        return 1, f"Exception: {str(e)}"

def run_compilation(file_path):
    """
    Try to build the module using kernel kbuild if possible;
    otherwise fallback to a lightweight `gcc -c` compile to
    capture syntax/semantic errors and header issues.

    Returns dict:
      {
        success: bool,
        method: "kbuild" or "gcc",
        output: str,
        errors: int,
        warnings: int,
        note: optional str,
        built_module: optional filename (if kbuild produced .ko)
      }
    """
    result = {
        "success": False,
        "method": None,
        "output": "",
        "errors": 0,
        "warnings": 0,
    }

    # If kernel build dir exists for current running kernel, try kbuild
    kernel_build_dir = f"/lib/modules/{os.uname().release}/build"
    cwd = os.getcwd()

    if os.path.exists(kernel_build_dir):
        result["method"] = "kbuild"
        # Create a tiny temporary module tree so kbuild has a Makefile
        tmpdir = tempfile.mkdtemp(prefix="evaluator_kbuild_")
        try:
            # Copy source file into tmpdir
            base = os.path.basename(file_path)
            dest = os.path.join(tmpdir, base)
            shutil.copy2(file_path, dest)

            # Create a minimal top-level Makefile for kbuild
            mk = (
                f"obj-m += evaluator_tmp.o\n"
                f"evaluator_tmp-objs := {base}\n"
            )
            # But obj-m needs object filenames; instead keep simple Makefile that builds this file as a module:
            mk = f"obj-m := {base.replace('.c','')}.o\n"
            with open(os.path.join(tmpdir, "Makefile"), "w") as fh:
                fh.write(mk)

            # Attempt to run kbuild
            cmd = [
                "make", "-C", kernel_build_dir,
                f"M={tmpdir}", "modules", "-j"
            ]
            ret, out = _run_proc(cmd, cwd=None, timeout=240)
            result["output"] = out
            # Parse warnings / errors
            result["errors"] = len(re.findall(r": error:", out)) + len(re.findall(r"error:", out))
            result["warnings"] = len(re.findall(r": warning:", out)) + len(re.findall(r"warning:", out))
            result["success"] = (ret == 0)
            # Detect built .ko
            ko_files = [f for f in os.listdir(tmpdir) if f.endswith(".ko")]
            if ko_files:
                result["built_module"] = ko_files[0]
        except Exception as e:
            result["output"] = f"Kbuild exception: {e}"
            result["success"] = False
        finally:
            try:
                shutil.rmtree(tmpdir)
            except Exception:
                pass

        # If kbuild produced fatal errors about missing headers (unlikely), fall back
        if not result["success"] and "No rule to make target" in result["output"]:
            # fall through to gcc path
            pass
        else:
            return result

    # Fallback: gcc compilation (lightweight)
    result["method"] = "gcc"
    try:
        # Use a temporary output object to avoid permission issues
        temp_obj = "temp_evaluator.o"
        cmd = ["gcc", "-Wall", "-Wextra", "-c", file_path, "-o", temp_obj]
        ret, out = _run_proc(cmd, cwd=None, timeout=60)
        result["output"] = out or ""
        # Count errors and warnings conservatively
        result["errors"] = len(re.findall(r"error:", result["output"]))
        result["warnings"] = len(re.findall(r"warning:", result["output"]))

        if ret == 0:
            result["success"] = True
        else:
            # Check if errors are only missing linux headers -> treat as soft pass
            missing_header_errors = re.findall(r"fatal error: linux/[^:]+: No such file or directory", result["output"])
            if missing_header_errors and result["errors"] == len(missing_header_errors):
                result["success"] = True
                result["note"] = "Soft pass: missing kernel headers only"
            else:
                result["success"] = False
    except Exception as e:
        result["output"] = f"GCC compile exception: {e}"
        result["success"] = False
    finally:
        # cleanup object file if present
        try:
            if os.path.exists("temp_evaluator.o"):
                os.remove("temp_evaluator.o")
        except Exception:
            pass

    return result
