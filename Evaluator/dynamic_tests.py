import os
import threading
import subprocess
import time


def _find_device_node(driver_name):
    """
    Try to locate a device node for the driver.
    Checks common /dev patterns, then scans /proc/devices.
    """
    candidates = [
        f"/dev/{driver_name}",
        f"/dev/{driver_name}0",
        f"/dev/{driver_name}_dev",
    ]
    for path in candidates:
        if os.path.exists(path):
            return path

    # Scan /proc/devices for registered char devices
    try:
        with open("/proc/devices") as f:
            for line in f:
                if driver_name in line:
                    return f"/dev/{driver_name}"
    except Exception:
        pass

    return None


def _basic_io_test(dev_node):
    notes = []
    success = True
    try:
        with open(dev_node, "w") as f:
            f.write("hello\n")
        notes.append("write ok")
    except Exception as e:
        notes.append(f"write failed: {e}")
        success = False

    try:
        with open(dev_node, "r") as f:
            data = f.read(64)
        notes.append(f"read ok (got: {data.strip()})")
    except Exception as e:
        notes.append(f"read failed: {e}")
        success = False

    return success, notes


def _concurrency_test(dev_node, threads=5):
    notes = []
    success = True

    def writer(idx):
        try:
            with open(dev_node, "w") as f:
                f.write(f"thread-{idx}\n")
        except Exception as e:
            notes.append(f"writer {idx} failed: {e}")

    ths = [threading.Thread(target=writer, args=(i,)) for i in range(threads)]
    for t in ths:
        t.start()
    for t in ths:
        t.join()

    if notes:
        success = False
    return success, notes or ["concurrent writes ok"]


def _stress_test(dev_node, duration=1.0):
    """
    Write repeatedly for `duration` seconds to simulate stress.
    """
    notes = []
    success = True
    start = time.perf_counter()
    ops = 0
    try:
        with open(dev_node, "w") as f:
            while time.perf_counter() - start < duration:
                f.write("spam\n")
                ops += 1
        notes.append(f"stress test ok ({ops} writes in {duration:.1f}s)")
    except Exception as e:
        notes.append(f"stress test failed: {e}")
        success = False
    return success, notes


def _dmesg_diff(before, after):
    """
    Compare dmesg logs before and after runtime tests.
    """
    new_lines = [line for line in after if line not in before]
    return new_lines if new_lines else ["no new dmesg output"]


def run_dynamic_tests(driver_name):
    results = {
        "device_found": False,
        "io_success": False,
        "concurrency_success": False,
        "stress_success": False,
        "notes": [],
    }

    # Capture dmesg before tests
    try:
        before = subprocess.run(
            ["dmesg", "--kernel", "--color=never"],
            capture_output=True,
            text=True,
        ).stdout.splitlines()
    except Exception:
        before = []

    dev_node = _find_device_node(driver_name)
    if not dev_node:
        results["notes"].append("No device node found under /dev/ or /proc/devices")
        return results

    results["device_found"] = True
    results["notes"].append(f"device node detected: {dev_node}")

    # Basic I/O
    io_ok, io_notes = _basic_io_test(dev_node)
    results["io_success"] = io_ok
    results["notes"].extend(io_notes)

    # Concurrency
    conc_ok, conc_notes = _concurrency_test(dev_node)
    results["concurrency_success"] = conc_ok
    results["notes"].extend(conc_notes)

    # Stress test
    stress_ok, stress_notes = _stress_test(dev_node)
    results["stress_success"] = stress_ok
    results["notes"].extend(stress_notes)

    # Capture dmesg after tests
    try:
        after = subprocess.run(
            ["dmesg", "--kernel", "--color=never"],
            capture_output=True,
            text=True,
        ).stdout.splitlines()
        diff = _dmesg_diff(before, after)
        results["notes"].append("dmesg diff:")
        results["notes"].extend(diff)
    except Exception:
        results["notes"].append("dmesg not accessible")

    return results
