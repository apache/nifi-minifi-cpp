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
const RUNS = {runs_json};

function megabytes(bytes) {{ return bytes / (1024 * 1024); }}

function makeChart(canvasId, title, valueKey, transform, yAxisLabel) {{
  const datasets = RUNS.map(run => ({{
    label: run.label,
    data: run.samples.map(s => ({{ x: s.elapsed_s, y: transform(s[valueKey]) }})),
    showLine: true,
    fill: false,
    tension: 0.1,
  }}));
  new Chart(document.getElementById(canvasId), {{
    type: 'scatter',
    data: {{ datasets }},
    options: {{
      plugins: {{ title: {{ display: true, text: title }} }},
      scales: {{
        x: {{ title: {{ display: true, text: 'Elapsed time (s)' }} }},
        y: {{ title: {{ display: true, text: yAxisLabel }}, beginAtZero: true }},
      }},
    }},
  }});
}}

const identity = v => v;

// Throughput is a single value per run, so it is shown as one bar per run.
function makeBarChart(canvasId, title, valueKey, yAxisLabel) {{
  new Chart(document.getElementById(canvasId), {{
    type: 'bar',
    data: {{
      labels: RUNS.map(run => run.label),
      datasets: [{{
        label: title,
        data: RUNS.map(run => run[valueKey]),
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
makeBarChart('throughputChart', 'Throughput', 'throughput', 'Flow files / sec');
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
        index = min(len(ordered) - 1, int(round(0.95 * (len(ordered) - 1))))
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


def build_summary_table(runs: list[dict]) -> str:
    columns = [
        ("Run", lambda r, s: r["label"]),
        ("Throughput (ff/s)", lambda r, s: f"{s['throughput']:.2f}"),
        ("Files processed", lambda r, s: s["flow_files_processed"]),
        ("Peak mem (MiB)", lambda r, s: f"{s['peak_memory_mib']:.1f}"),
        ("Mean mem (MiB)", lambda r, s: f"{s['mean_memory_mib']:.1f}"),
        ("Mean CPU (%)", lambda r, s: f"{s['mean_cpu']:.1f}"),
        ("p95 CPU (%)", lambda r, s: f"{s['p95_cpu']:.1f}"),
        ("Final FF repo (MiB)", lambda r, s: f"{s['final_flowfile_mib']:.1f}"),
        ("Final content repo (MiB)", lambda r, s: f"{s['final_content_mib']:.1f}"),
        ("Peak content repo (MiB)", lambda r, s: f"{s['peak_content_mib']:.1f}"),
    ]
    header = "".join(f"<th>{name}</th>" for name, _ in columns)
    rows = ""
    for run in runs:
        summary = compute_summary(run)
        cells = "".join(f"<td>{getter(run, summary)}</td>" for _, getter in columns)
        rows += f"<tr>{cells}</tr>"
    return f"<table><thead><tr>{header}</tr></thead><tbody>{rows}</tbody></table>"


def write_report(result_paths: list[str], output_path: str) -> None:
    runs = [load_run(path) for path in result_paths]
    assign_labels(runs)

    html = HTML_TEMPLATE.format(
        chart_js_cdn=CHART_JS_CDN,
        summary_table=build_summary_table(runs),
        config_table=build_config_table(runs),
        runs_json=json.dumps(runs),
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
