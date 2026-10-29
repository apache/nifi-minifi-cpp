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
import copy
import os
from datetime import datetime, timezone

import humanfriendly
import run_benchmark
from run_benchmark import CONTENT_REPOSITORY_CLASSES, FLOWFILE_REPOSITORY_CLASSES, InputGenerationType


def parse_combo(value: str) -> tuple[str, str]:
    parts = value.split(":")
    if len(parts) != 2:
        raise argparse.ArgumentTypeError(f"Combo must be 'flowfile:content', got '{value}'.")
    flowfile, content = parts
    if flowfile not in FLOWFILE_REPOSITORY_CLASSES:
        raise argparse.ArgumentTypeError(
            f"Unknown flowfile repository '{flowfile}', choose from {sorted(FLOWFILE_REPOSITORY_CLASSES)}."
        )
    if content not in CONTENT_REPOSITORY_CLASSES:
        raise argparse.ArgumentTypeError(
            f"Unknown content repository '{content}', choose from {sorted(CONTENT_REPOSITORY_CLASSES)}."
        )
    return flowfile, content


def run_combo(args: argparse.Namespace, flowfile: str, content: str, output_dir: str, rep: int) -> str:
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    name = f"{timestamp}_{flowfile}_{content}_{args.input_file_generation_type}_run{rep:03d}.json"
    output_path = os.path.join(output_dir, name)
    combo_args = copy.copy(args)
    combo_args.flowfile_repository = flowfile
    combo_args.content_repository = content
    combo_args.output = output_path
    return run_benchmark.run(combo_args)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run the MiNiFi C++ repository benchmark across several repository combinations sequentially and (optionally) generate a single comparison report. "
        "Runs are sequential by design so containers do not compete for CPU/IO."
    )
    parser.add_argument("--image", required=True, help="Docker image to use for the benchmark.")
    parser.add_argument(
        "--combo",
        required=True,
        action="append",
        type=parse_combo,
        dest="combos",
        metavar="FLOWFILE:CONTENT",
        help="Repository combination to benchmark, e.g. --combo lmdb:lmdb. Repeatable.",
    )
    parser.add_argument(
        "--input-file-generation-type",
        default=InputGenerationType.TIMED_GETFILE.value,
        choices=sorted([e.value for e in InputGenerationType]),
        help="Input file generation type shared by all combos (see run_benchmark.py).",
    )
    parser.add_argument(
        "--input-file-count",
        type=int,
        default=100,
        help="Number of input files for burst input generation type (default: 100).",
    )
    parser.add_argument(
        "--duration", type=int, default=120, help="Total benchmark session length in seconds (default: 120)."
    )
    parser.add_argument(
        "--input-interval", type=float, default=1.0, help="Seconds between input file generation cycles (default: 1)."
    )
    parser.add_argument(
        "--input-file-size",
        type=humanfriendly.parse_size,
        default=humanfriendly.parse_size("1M"),
        help="Size of each generated input file, e.g. 512K, 1M, 1G (default: 1M).",
    )
    parser.add_argument(
        "--attribute-count",
        type=int,
        default=0,
        help="Number of extra attributes to set on each flow file via UpdateAttribute (default: 0).",
    )
    parser.add_argument(
        "--attribute-size",
        type=humanfriendly.parse_size,
        default=0,
        help="Size of each extra attribute value, e.g. 64, 1K (default: 0).",
    )
    parser.add_argument(
        "--metrics-interval", type=float, default=1.0, help="Seconds between metric samples (default: 1)."
    )
    parser.add_argument(
        "--repeat",
        type=int,
        default=1,
        help="Number of times to run each combo; results are aggregated per combo in the report (default: 1).",
    )
    parser.add_argument(
        "--output-dir", default=run_benchmark.RESULTS_DIR, help="Directory for result JSON files (default: results/)."
    )
    parser.add_argument(
        "--report", default=None, help="If set, generate an HTML report at this path from all successful runs."
    )
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    result_paths: list[str] = []
    failures: list[tuple[str, str, str]] = []
    total_runs = len(args.combos) * args.repeat
    for index, (flowfile, content) in enumerate(args.combos, start=1):
        for rep in range(1, args.repeat + 1):
            print(
                f"\n=== [combo {index}/{len(args.combos)} rep {rep}/{args.repeat}] Benchmarking {flowfile}/{content} ==="
            )
            try:
                result_paths.append(run_combo(args, flowfile, content, args.output_dir, rep))
            except Exception as error:
                print(f"Error: combo {flowfile}/{content} (rep {rep}) failed with: {error}")
                failures.append((flowfile, content, str(error)))

    print("\n=== Batch summary ===")
    print(f"Succeeded: {len(result_paths)}/{total_runs}")
    for flowfile, content, error in failures:
        print(f"  FAILED {flowfile}/{content}: {error}")

    if args.report and result_paths:
        import generate_report

        generate_report.write_report(result_paths, args.report)

    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
