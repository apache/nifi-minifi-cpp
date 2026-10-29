#!/usr/bin/env python3
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import json
import os
import statistics
from collections import Counter

CHART_JS_CDN = "https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"

MIB = 1024 * 1024

HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>MiNiFi C++ Repository Benchmark Report</title>
<script src="{chart_js_cdn}"></script>
<style>
  body {{ font-family: sans-serif; margin: 2rem; color: #222; }}
  h1 {{ font-size: 1.5rem; }}
  .chart-container {{ max-width: 900px; margin-bottom: 3rem; }}
  table {{ border-collapse: collapse; margin-bottom: 2rem; font-size: 0.9rem; }}
  th, td {{ border: 1px solid #ccc; padding: 4px 8px; text-align: left; }}
  th {{ background: #f0f0f0; }}
  .range {{ color: #888; font-size: 0.85em; }}
</style>
</head>
<body>
<h1>MiNiFi C++ Repository Benchmark Report</h1>
<h2>Summary</h2>
{summary_table}
<h2>Runs</h2>
{config_table}
<div class="chart-container"><canvas id="flowfileChart"></canvas></div>
<div class="chart-container"><canvas id="contentChart"></canvas></div>
<div class="chart-container"><canvas id="memoryChart"></canvas></div>
<div class="chart-container"><canvas id="cpuChart"></canvas></div>
<div class="chart-container"><canvas id="throughputChart"></canvas></div>
<script>
// One entry per repository combo. Samples of the combo's repeated runs are
// aggregated per sample index into a mean line with a min/max band.
const COMBOS = {runs_json};

function megabytes(bytes) {{ return bytes / (1024 * 1024); }}

function comboColor(index, alpha) {{
  const hue = (index * 137.508) % 360;
  return `hsla(${{hue}}, 65%, 45%, ${{alpha}})`;
}}

function makeChart(canvasId, title, seriesKey, transform, yAxisLabel) {{
  const datasets = [];
  COMBOS.forEach((combo, i) => {{
    const points = combo.series[seriesKey];
    const line = comboColor(i, 1);
    const band = comboColor(i, 0.15);
    // Lower bound (min), drawn invisibly; the next dataset fills down to it.
    datasets.push({{
      label: combo.label + ' (min)', showInLegend: false,
      data: points.map(p => ({{ x: p.x, y: transform(p.min) }})),
      showLine: true, pointRadius: 0, borderWidth: 0, fill: false,
    }});
    // Upper bound (max); fill to the previous dataset (min) shades the band.
    datasets.push({{
      label: combo.label + ' (max)', showInLegend: false,
      data: points.map(p => ({{ x: p.x, y: transform(p.max) }})),
      showLine: true, pointRadius: 0, borderWidth: 0, backgroundColor: band, fill: '-1',
    }});
    // Mean line on top.
    datasets.push({{
      label: combo.label,
      data: points.map(p => ({{ x: p.x, y: transform(p.mean) }})),
      showLine: true, pointRadius: 0, borderColor: line, borderWidth: 2, fill: false, tension: 0.1,
    }});
  }});
  new Chart(document.getElementById(canvasId), {{
    type: 'scatter',
    data: {{ datasets }},
    options: {{
      plugins: {{
        title: {{ display: true, text: title }},
        legend: {{ labels: {{ filter: (item, data) => data.datasets[item.datasetIndex].showInLegend !== false }} }},
      }},
      scales: {{
        x: {{ title: {{ display: true, text: 'Elapsed time (s)' }} }},
        y: {{ title: {{ display: true, text: yAxisLabel }}, beginAtZero: true }},
      }},
    }},
  }});
}}

const identity = v => v;

// Throughput is a single value per run, shown as the mean across runs per combo.
function makeBarChart(canvasId, title, valueKey, yAxisLabel) {{
  new Chart(document.getElementById(canvasId), {{
    type: 'bar',
    data: {{
      labels: COMBOS.map(combo => combo.label),
      datasets: [{{
        label: title,
        data: COMBOS.map(combo => combo[valueKey]),
        backgroundColor: COMBOS.map((combo, i) => comboColor(i, 0.7)),
      }}],
    }},
    options: {{
      plugins: {{ title: {{ display: true, text: title }}, legend: {{ display: false }} }},
      scales: {{
        y: {{ title: {{ display: true, text: yAxisLabel }}, beginAtZero: true }},
      }},
    }},
  }});
}}

makeChart('flowfileChart', 'FlowFile repository size', 'flowfile_repo_bytes', megabytes, 'Megabytes (MiB)');
makeChart('contentChart', 'Content repository size', 'content_repo_bytes', megabytes, 'Megabytes (MiB)');
makeChart('memoryChart', 'Process memory usage', 'memory_bytes', megabytes, 'Megabytes (MiB)');
makeChart('cpuChart', 'Process CPU usage', 'cpu_percent', identity, 'CPU usage (%, 100 = 1 core)');
makeBarChart('throughputChart', 'Throughput (mean)', 'throughput', 'Flow files / sec');
</script>
</body>
</html>
"""


def build_config_table(runs: list[dict]) -> str:
    columns = [
        ("Label", lambda r: r["label"]),
        ("FlowFile repo", lambda r: r["config"].get("flowfile_repository", "")),
        ("Content repo", lambda r: r["config"].get("content_repository", "")),
        ("File size (B)", lambda r: r["config"].get("input_file_size_bytes", "")),
        ("Input interval (s)", lambda r: r["config"].get("input_interval_s", "")),
        ("Metrics interval (s)", lambda r: r["config"].get("metrics_interval_s", "")),
        ("Duration (s)", lambda r: r["config"].get("duration_s", "")),
        ("Samples", lambda r: len(r["samples"])),
        ("Input generation type", lambda r: r["config"].get("input_file_generation_type", "")),
        ("Input file count", lambda r: r["config"].get("input_file_count", "")),
    ]
    header = "".join(f"<th>{name}</th>" for name, _ in columns)
    rows = ""
    for run in runs:
        cells = "".join(f"<td>{getter(run)}</td>" for _, getter in columns)
        rows += f"<tr>{cells}</tr>"
    return f"<table><thead><tr>{header}</tr></thead><tbody>{rows}</tbody></table>"


def load_run(path: str) -> dict:
    with open(path) as result_file:
        data = json.load(result_file)
    config = data.get("config", {})
    combo = "{}/{}".format(
        config.get("flowfile_repository", "?"),
        config.get("content_repository", "?"),
    )
    return {
        "combo": combo,
        "path": path,
        "config": config,
        "samples": data.get("samples", []),
        "throughput": data.get("throughput", 0),
        "flow_files_processed": data.get("flow_files_processed"),
    }


def assign_labels(runs: list[dict]) -> None:
    # Use the repo combination as the label, only disambiguating with the file
    # name when the same combination appears more than once.
    combo_counts = Counter(run["combo"] for run in runs)
    for run in runs:
        if combo_counts[run["combo"]] > 1:
            run["label"] = f"{run['combo']} ({os.path.basename(run['path'])})"
        else:
            run["label"] = run["combo"]


def compute_summary(run: dict) -> dict:
    samples = run["samples"]
    memory = [s.get("memory_bytes", 0) for s in samples]
    cpu = [s.get("cpu_percent", 0) for s in samples]
    flowfile_sizes = [s.get("flowfile_repo_bytes", 0) for s in samples]
    content_sizes = [s.get("content_repo_bytes", 0) for s in samples]

    def peak(values: list) -> float:
        return max(values) if values else 0

    def mean(values: list) -> float:
        return statistics.fmean(values) if values else 0

    def p95(values: list) -> float:
        if not values:
            return 0
        ordered = sorted(values)
        index = min(len(ordered) - 1, round(0.95 * (len(ordered) - 1)))
        return ordered[index]

    processed = run.get("flow_files_processed")
    final_content = content_sizes[-1] if content_sizes else 0

    return {
        "peak_memory_mib": peak(memory) / MIB,
        "mean_memory_mib": mean(memory) / MIB,
        "mean_cpu": mean(cpu),
        "p95_cpu": p95(cpu),
        "final_flowfile_mib": (flowfile_sizes[-1] if flowfile_sizes else 0) / MIB,
        "final_content_mib": final_content / MIB,
        "peak_content_mib": peak(content_sizes) / MIB,
        "throughput": run["throughput"],
        "flow_files_processed": processed if processed is not None else "n/a",
    }


# Scalar summary metrics aggregated across a combo's repeated runs.
# (label, compute_summary key, decimal places)
AGGREGATE_METRICS = [
    ("Throughput (ff/s)", "throughput", 2),
    ("Files processed", "flow_files_processed", 0),
    ("Peak mem (MiB)", "peak_memory_mib", 1),
    ("Mean mem (MiB)", "mean_memory_mib", 1),
    ("Mean CPU (%)", "mean_cpu", 1),
    ("p95 CPU (%)", "p95_cpu", 1),
    ("Final FF repo (MiB)", "final_flowfile_mib", 1),
    ("Final content repo (MiB)", "final_content_mib", 1),
    ("Peak content repo (MiB)", "peak_content_mib", 1),
]

# Time-series metrics collapsed into a per-combo mean/min/max band.
SERIES_KEYS = ["flowfile_repo_bytes", "content_repo_bytes", "memory_bytes", "cpu_percent"]


def group_runs(runs: list[dict]) -> dict[str, list[dict]]:
    groups: dict[str, list[dict]] = {}
    for run in runs:
        groups.setdefault(run["combo"], []).append(run)
    return groups


def aggregate_stats(values: list) -> dict | None:
    numeric = [v for v in values if isinstance(v, (int, float))]
    if not numeric:
        return None
    return {
        "mean": statistics.fmean(numeric),
        "std": statistics.stdev(numeric) if len(numeric) > 1 else 0.0,
        "min": min(numeric),
        "max": max(numeric),
    }


def format_cell(stats: dict | None, precision: int) -> str:
    if stats is None:
        return "n/a"
    mean = f"{stats['mean']:.{precision}f}"
    std = f"{stats['std']:.{precision}f}"
    low = f"{stats['min']:.{precision}f}"
    high = f"{stats['max']:.{precision}f}"
    return f'{mean} &plusmn; {std}<br><span class="range">{low}&ndash;{high}</span>'


def build_summary_table(groups: dict[str, list[dict]]) -> str:
    header = "<th>Combo</th><th>Runs</th>" + "".join(f"<th>{name}</th>" for name, _, _ in AGGREGATE_METRICS)
    rows = ""
    for combo, runs in groups.items():
        summaries = [compute_summary(run) for run in runs]
        cells = f"<td>{combo}</td><td>{len(runs)}</td>"
        for _, key, precision in AGGREGATE_METRICS:
            stats = aggregate_stats([summary[key] for summary in summaries])
            cells += f"<td>{format_cell(stats, precision)}</td>"
        rows += f"<tr>{cells}</tr>"
    return f"<table><thead><tr>{header}</tr></thead><tbody>{rows}</tbody></table>"


def build_band_series(runs: list[dict]) -> dict[str, list[dict]]:
    step = (
        next((run["config"].get("metrics_interval_s") for run in runs if run["config"].get("metrics_interval_s")), 1)
        or 1
    )
    buckets: dict[float, dict[str, list]] = {}
    for run in runs:
        for sample in run["samples"]:
            bucket = round(sample.get("elapsed_s", 0) / step) * step
            slot = buckets.setdefault(bucket, {key: [] for key in SERIES_KEYS})
            for key in SERIES_KEYS:
                slot[key].append(sample.get(key, 0))

    series: dict[str, list[dict]] = {key: [] for key in SERIES_KEYS}
    for bucket in sorted(buckets):
        slot = buckets[bucket]
        for key in SERIES_KEYS:
            values = slot[key]
            series[key].append(
                {
                    "x": round(bucket, 3),
                    "mean": statistics.fmean(values),
                    "min": min(values),
                    "max": max(values),
                }
            )
    return series


def build_aggregated_combos(groups: dict[str, list[dict]]) -> list[dict]:
    combos = []
    for combo, runs in groups.items():
        throughput = aggregate_stats([run["throughput"] for run in runs])
        combos.append(
            {
                "label": f"{combo} (n={len(runs)})",
                "series": build_band_series(runs),
                "throughput": throughput["mean"] if throughput else 0,
            }
        )
    return combos


def write_report(result_paths: list[str], output_path: str) -> None:
    runs = [load_run(path) for path in result_paths]
    assign_labels(runs)
    groups = group_runs(runs)

    html = HTML_TEMPLATE.format(
        chart_js_cdn=CHART_JS_CDN,
        summary_table=build_summary_table(groups),
        config_table=build_config_table(runs),
        runs_json=json.dumps(build_aggregated_combos(groups)),
    )

    with open(output_path, "w") as output_file:
        output_file.write(html)

    print(f"Report with {len(runs)} run(s) written to {output_path}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate an HTML report from benchmark results.")
    parser.add_argument("results", nargs="+", help="Benchmark result JSON files.")
    parser.add_argument("-o", "--output", default="report.html", help="Output HTML file path.")
    args = parser.parse_args()

    write_report(args.results, args.output)


if __name__ == "__main__":
    main()
