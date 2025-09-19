import subprocess
import os
import platform


def run_runtime_checks(driver_path):
    """
    Attempt to build, load, and unload a kernel module.
    Auto-generates a Makefile if missing.
    Cleans up build artifacts afterward.
    Returns metrics usable by evaluator + scoring.
    """

    metrics = {
        "compiled": False,
        "loaded": False,
        "unloaded": False,
        "dmesg_success": False,
        "runtime_notes": ""
    }

    driver_dir = os.path.dirname(os.path.abspath(driver_path))
    driver_file = os.path.basename(driver_path)
    driver_base, _ = os.path.splitext(driver_file)
    ko_file = os.path.join(driver_dir, f"{driver_base}.ko")
    makefile_path = os.path.join(driver_dir, "Makefile")

    kernel_release = platform.uname().release
    auto_makefile = False

    try:
        # --- Step 1: Ensure Makefile exists ---
        if not os.path.exists(makefile_path):
            auto_makefile = True
            makefile_content = (
                f"obj-m += {driver_base}.o\n\n"
                "all:\n"
                f"\tmake -C /lib/modules/{kernel_release}/build M=$(PWD) modules\n\n"
                "clean:\n"
                f"\tmake -C /lib/modules/{kernel_release}/build M=$(PWD) clean\n"
            )
            with open(makefile_path, "w") as f:
                f.write(makefile_content)

        # --- Step 2: Build driver ---
        build_cmd = [
            "make",
            "-C", f"/lib/modules/{kernel_release}/build",
            f"M={driver_dir}",
            "modules"
        ]
        try:
            subprocess.run(build_cmd, check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as e:
            metrics["runtime_notes"] = f"Build failed: {e.stderr.strip()}"
            return metrics

        if not os.path.exists(ko_file):
            metrics["runtime_notes"] = "No .ko file produced after build."
            return metrics

        metrics["compiled"] = True

        # --- Step 3: Try inserting module ---
        try:
            subprocess.run(["sudo", "insmod", ko_file], check=True, capture_output=True, text=True)
            metrics["loaded"] = True
        except subprocess.CalledProcessError as e:
            if "File exists" in e.stderr:
                # Module already loaded -> remove & retry
                try:
                    subprocess.run(["sudo", "rmmod", driver_base], check=True, capture_output=True, text=True)
                    subprocess.run(["sudo", "insmod", ko_file], check=True, capture_output=True, text=True)
                    metrics["loaded"] = True
                    metrics["runtime_notes"] = "Module was already loaded; reloaded successfully."
                except subprocess.CalledProcessError as e2:
                    metrics["runtime_notes"] = f"insmod retry failed: {e2.stderr.strip()}"
                    return metrics
            else:
                metrics["runtime_notes"] = f"insmod failed: {e.stderr.strip()}"
                return metrics
        except PermissionError:
            metrics["runtime_notes"] = "No permission for insmod."
            return metrics

        # --- Step 4: Check dmesg ---
        try:
            result = subprocess.run(
                ["dmesg", "--kernel", "--ctime", "--color=never"],
                capture_output=True, text=True
            )
            if result.returncode == 0 and result.stdout:
                metrics["dmesg_success"] = True
        except Exception:
            metrics["runtime_notes"] += " dmesg not accessible."

        # --- Step 5: Remove module ---
        try:
            subprocess.run(["sudo", "rmmod", driver_base], check=True, capture_output=True, text=True)
            metrics["unloaded"] = True
        except subprocess.CalledProcessError as e:
            metrics["runtime_notes"] += f" rmmod failed: {e.stderr.strip()}"
        except PermissionError:
            metrics["runtime_notes"] += " no permission for rmmod."

    finally:
        # --- Step 6: Cleanup build artifacts ---
        try:
            subprocess.run(["make", "clean"], cwd=driver_dir, capture_output=True, text=True)
        except Exception:
            pass

        # Remove auto-generated Makefile if we created one
        if auto_makefile and os.path.exists(makefile_path):
            try:
                os.remove(makefile_path)
            except Exception:
                pass

    return metrics
