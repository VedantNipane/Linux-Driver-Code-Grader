import csv
import os
from datetime import datetime

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOG_FILE = os.path.join(REPO_ROOT,"score_logs.csv")

def log_score(file_path, results, overall_score, breakdown):
    """
    Append evaluation results to score_logs.csv with full breakdown.
    
    Args:
        file_path (str): Path to the evaluated driver file.
        results (dict): Full results dict (compilation, style, security, structure, etc.).
        overall_score (float): Final overall score.
        breakdown (dict): Detailed scoring breakdown from scoring.py
                          Format: { "Correctness": {"awarded": x, "max": y, "details": [...]}, ... }
    """

    log_file = "score_logs.csv"
    file_exists = os.path.isfile(log_file)

    row = {
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "file": os.path.basename(file_path),
        "overall_score": f"{overall_score:.1f}/100"
    }

    for category, data in breakdown.items():
        awarded = data.get("awarded", 0.0)
        max_pts = data.get("max", 0.0)
        row[category] = f"{awarded:.1f}/{max_pts:.0f}"

    with open(LOG_FILE, "a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=row.keys())
        if not file_exists:
            writer.writeheader()
        writer.writerow(row)