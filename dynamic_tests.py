# Evaluator/dynamic_tests.py
import os
import time
import glob
import threading
import fcntl
import errno

def find_device_by_name(module_name):
    """Return the first /dev path that contains module_name; None if not found."""
    candidates = [p for p in glob.glob("/dev/*") if module_name in os.path.basename(p)]
    return candidates[0] if candidates else None

def smoke_test_device(device_path, write_data=b"hello\n", read_len=4096, timeout=2.0):
    """
    Try open/write/read basic smoke test. Non-blocking open used to avoid hang.
    Returns dict: {passed: bool, read: bytes|None, error: str|None}
    """
    res = {"passed": False, "read": None, "error": None}
    try:
        fd = os.open(device_path, os.O_RDWR | os.O_NONBLOCK)
    except OSError as e:
        res["error"] = f"open_failed: {e}"
        return res

    try:
        # write
        try:
            os.write(fd, write_data)
        except OSError as e:
            # write can fail; record and continue to attempt read
            res["error"] = f"write_failed: {e}"
        # try read (some drivers may not echo; presence of readable data means something)
        start = time.time()
        read_b = None
        while time.time() - start < timeout:
            try:
                read_b = os.read(fd, read_len)
                break
            except BlockingIOError:
                time.sleep(0.05)
            except OSError as e:
                res["error"] = f"read_failed: {e}"
                break
        res["read"] = read_b
        # Basic pass criteria: either wrote without throwing fatal error OR we read data back
        res["passed"] = (res["error"] is None) or (read_b not in (None, b""))
    finally:
        try:
            os.close(fd)
        except Exception:
            pass
    return res

def concurrency_test_device(device_path, threads=8, iterations=200, payload=b"X"*256):
    """
    Spawn threads concurrently writing to device_path.
    Returns counts and sample errors.
    """
    stats = {"success": 0, "fail": 0, "errors": []}
    lock = threading.Lock()

    def _worker():
        try:
            fd = os.open(device_path, os.O_WRONLY | os.O_NONBLOCK)
        except OSError as e:
            with lock:
                stats["fail"] += iterations
                stats["errors"].append(f"open_err:{e}")
            return
        for _ in range(iterations):
            try:
                os.write(fd, payload)
                with lock:
                    stats["success"] += 1
            except OSError as e:
                with lock:
                    stats["fail"] += 1
                    stats["errors"].append(str(e))
        try:
            os.close(fd)
        except Exception:
            pass

    th = []
    for _ in range(threads):
        t = threading.Thread(target=_worker)
        t.start()
        th.append(t)
    for t in th:
        t.join()
    return stats

def perf_test_device(device_path, total_bytes=2*1024*1024, chunk=65536):
    """
    Write `total_bytes` in chunks and report MB/s. Non-blocking to avoid permanent hangs.
    """
    res = {"written": 0, "elapsed": None, "MBps": None, "error": None}
    try:
        fd = os.open(device_path, os.O_WRONLY | os.O_NONBLOCK)
    except OSError as e:
        res["error"] = f"open_failed: {e}"
        return res

    buf = b"X" * chunk
    written = 0
    start = time.time()
    try:
        while written < total_bytes:
            try:
                os.write(fd, buf)
                written += len(buf)
            except BlockingIOError:
                # small wait and retry
                time.sleep(0.01)
            except OSError as e:
                res["error"] = f"write_failed: {e}"
                break
    finally:
        try:
            os.close(fd)
        except Exception:
            pass
        end = time.time()
        res["written"] = written
        res["elapsed"] = round(end - start, 3)
        res["MBps"] = round((written / (1024*1024)) / res["elapsed"], 3) if res["elapsed"] and res["elapsed"] > 0 else 0.0
    return res

def sysfs_params_test(module_name, expected_params=None):
    """
    Check /sys/module/<module_name>/parameters presence and optionally verify expected param names.
    """
    base = f"/sys/module/{module_name}/parameters"
    res = {"exists": os.path.isdir(base), "found": [], "error": None}
    if not res["exists"]:
        return res
    try:
        names = os.listdir(base)
        if expected_params:
            for p in expected_params:
                if p in names:
                    res["found"].append(p)
        else:
            res["found"] = names
    except OSError as e:
        res["error"] = str(e)
    return res

def ioctl_test(device_path, ioctl_codes=None):
    """
    Send IOCTLs from ioctl_codes (list of ints). Return results for each code.
    Many drivers will return EINVAL for unknown codes — we treat that as graceful handling.
    """
    results = {}
    if not ioctl_codes:
        ioctl_codes = []
    try:
        fd = os.open(device_path, os.O_RDWR | os.O_NONBLOCK)
    except OSError as e:
        return {"error": f"open_failed: {e}"}
    for code in ioctl_codes:
        try:
            fcntl.ioctl(fd, code, 0)
            results[code] = {"ok": True}
        except OSError as e:
            results[code] = {"ok": False, "errno": e.errno, "strerror": str(e)}
    try:
        os.close(fd)
    except Exception:
        pass
    return results

def run_dynamic_tests(module_name, device_path=None, concurrency_cfg=None, perf_cfg=None, ioctl_codes=None, expected_sysfs=None):
    """
    Orchestrator returns a dict with all dynamic test results.
    """
    out = {}
    if device_path is None:
        device_path = find_device_by_name(module_name)
    if not device_path:
        out["skipped"] = "no device node found"
        return out

    out["device"] = device_path
    out["smoke"] = smoke_test_device(device_path)
    out["concurrency"] = concurrency_test_device(device_path, **(concurrency_cfg or {}))
    out["perf"] = perf_test_device(device_path, **(perf_cfg or {}))
    out["sysfs"] = sysfs_params_test(module_name, expected_sysfs)
    if ioctl_codes:
        out["ioctl"] = ioctl_test(device_path, ioctl_codes)
    return out
