import csv
import os
from datetime import datetime

LOG_FILE = "score_logs.csv"

def log_score(file_name, breakdown, final_score):
    """
    Append evaluation results (breakdown + total) to CSV.
    """
    # Ensure CSV exists with header
    if not os.path.exists(LOG_FILE):
        with open(LOG_FILE, mode="w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "timestamp", "file",
                "Correctness", "Security", "Code Quality",
                "Performance", "Advanced",
                "Final Score"
            ])

    # Append new row
    with open(LOG_FILE, mode="a", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            file_name,
            f"{breakdown['Correctness']:.1f}/40",
            f"{breakdown['Security']:.1f}/25",
            f"{breakdown['Code Quality']:.1f}/20",
            f"{breakdown['Performance']:.1f}/10",
            f"{breakdown['Advanced']:.1f}/5",
            f"{final_score:.1f}/100"
        ])
