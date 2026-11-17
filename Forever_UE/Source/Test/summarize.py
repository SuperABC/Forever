"""汇总 CoreTest 报告目录：python summarize.py <reports_dir> [<baseline_dir>]
输出 summary.md：每套件通过/失败、失败项(含复现命令)、关键指标，以及和基线目录的指标差异(>25%标记)。
"""
import json
import os
import sys


def load(directory):
    suites = {}
    for name in sorted(os.listdir(directory)):
        if name.endswith(".json"):
            with open(os.path.join(directory, name), encoding="utf-8") as f:
                data = json.load(f)
            suites[data.get("suite", name[:-5])] = data
    return suites


def metrics_of(data):
    return {m["name"]: m["value"] for m in data.get("metrics", [])}


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    current = load(sys.argv[1])
    baseline = load(sys.argv[2]) if len(sys.argv) > 2 else {}
    lines = ["# Test summary", "", "| suite | passed | failed |", "|---|---|---|"]
    failed_total = 0
    for name, data in current.items():
        lines.append("| %s | %d | %d |" % (name, data.get("passed", 0), data.get("failed", 0)))
        failed_total += data.get("failed", 0)
    lines.append("")
    if failed_total:
        lines += ["## Failures", ""]
        for name, data in current.items():
            for check in data.get("checks", []):
                if not check.get("passed", True):
                    lines.append("- **%s** %s — %s  \n  repro: `%s`" % (name, check["name"], check.get("detail", ""), data.get("repro", "")))
        lines.append("")
    drift = []
    for name, data in current.items():
        old = metrics_of(baseline.get(name, {}))
        for key, value in metrics_of(data).items():
            if key in old and old[key] > 0 and abs(value - old[key]) / old[key] > 0.25 and not key.startswith("stage."):
                drift.append("- %s / %s: %.3f -> %.3f" % (name, key, old[key], value))
    if drift:
        lines += ["## Metric drift vs baseline (>25%)", ""] + drift + [""]
    path = os.path.join(sys.argv[1], "summary.md")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("\n".join(lines))
    return 1 if failed_total else 0


if __name__ == "__main__":
    sys.exit(main())
