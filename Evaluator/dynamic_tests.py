import os
import threading


def _find_device_node(driver_name):
    candidates = [
        f"/dev/{driver_name}",
        f"/dev/{driver_name}0",
        f"/dev/{driver_name}_dev",
    ]
    for path in candidates:
        if os.path.exists(path):
            return path
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


def _concurrency_test(dev_node):
    notes = []
    success = True

    def writer(idx):
        try:
            with open(dev_node, "w") as f:
                f.write(f"thread-{idx}\n")
        except Exception as e:
            notes.append(f"writer {idx} failed: {e}")

    threads = [threading.Thread(target=writer, args=(i,)) for i in range(5)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    if notes:
        success = False
    return success, notes or ["concurrent writes ok"]


def run_dynamic_tests(driver_name):
    results = {
        "device_found": False,
        "io_success": False,
        "concurrency_success": False,
        "notes": [],
    }

    dev_node = _find_device_node(driver_name)
    if not dev_node:
        results["notes"].append("No device node found under /dev/")
        return results

    results["device_found"] = True
    results["notes"].append(f"device node detected: {dev_node}")

    io_ok, io_notes = _basic_io_test(dev_node)
    results["io_success"] = io_ok
    results["notes"].extend(io_notes)

    conc_ok, conc_notes = _concurrency_test(dev_node)
    results["concurrency_success"] = conc_ok
    results["notes"].extend(conc_notes)

    return results
