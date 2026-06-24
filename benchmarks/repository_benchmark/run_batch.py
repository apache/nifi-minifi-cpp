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
import os
import humanfriendly
import run_benchmark
from argparse import Namespace
from datetime import datetime, timezone
from run_benchmark import CONTENT_REPOSITORY_CLASSES, FLOWFILE_REPOSITORY_CLASSES, InputGenerationType


def parse_combo(value: str) -> tuple[str, str]:
    parts = value.split(":")
    if len(parts) != 2:
        raise argparse.ArgumentTypeError(f"Combo must be 'flowfile:content', got '{value}'.")
    flowfile, content = parts
    if flowfile not in FLOWFILE_REPOSITORY_CLASSES:
        raise argparse.ArgumentTypeError(
            f"Unknown flowfile repository '{flowfile}', choose from {sorted(FLOWFILE_REPOSITORY_CLASSES)}.")
    if content not in CONTENT_REPOSITORY_CLASSES:
        raise argparse.ArgumentTypeError(
            f"Unknown content repository '{content}', choose from {sorted(CONTENT_REPOSITORY_CLASSES)}.")
    return flowfile, content


def run_combo(args: argparse.Namespace, flowfile: str, content: str, output_dir: str) -> str:
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    name = f"{timestamp}_{flowfile}_{content}_{args.input_file_generation_type}.json"
    output_path = os.path.join(output_dir, name)
    combo_args = Namespace(
        image=args.image,
        flowfile_repository=flowfile,
        content_repository=content,
        input_file_generation_type=args.input_file_generation_type,
        input_file_count=args.input_file_count,
        duration=args.duration,
        input_interval=args.input_interval,
        input_file_size=args.input_file_size,
        metrics_interval=args.metrics_interval,
        output=output_path,
    )
    return run_benchmark.run(combo_args)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run the MiNiFi C++ repository benchmark across several repository combinations sequentially and (optionally) generate a single comparison report. "
                    "Runs are sequential by design so containers do not compete for CPU/IO.")
    parser.add_argument("--image", required=True, help="Docker image to use for the benchmark.")
    parser.add_argument("--combo", required=True, action="append", type=parse_combo, dest="combos",
                        metavar="FLOWFILE:CONTENT",
                        help="Repository combination to benchmark, e.g. --combo lmdb:lmdb. Repeatable.")
    parser.add_argument("--input-file-generation-type", default=InputGenerationType.TIMED_GETFILE.value,
                        choices=sorted([e.value for e in InputGenerationType]),
                        help="Input file generation type shared by all combos (see run_benchmark.py).")
    parser.add_argument("--input-file-count", type=int, default=100,
                        help="Number of input files for burst input generation type (default: 100).")
    parser.add_argument("--duration", type=int, default=120,
                        help="Total benchmark session length in seconds (default: 120).")
    parser.add_argument("--input-interval", type=float, default=1.0,
                        help="Seconds between input file generation cycles (default: 1).")
    parser.add_argument("--input-file-size", type=humanfriendly.parse_size, default=humanfriendly.parse_size("1M"),
                        help="Size of each generated input file, e.g. 512K, 1M, 1G (default: 1M).")
    parser.add_argument("--metrics-interval", type=float, default=5.0,
                        help="Seconds between metric samples (default: 5).")
    parser.add_argument("--output-dir", default=run_benchmark.RESULTS_DIR,
                        help="Directory for result JSON files (default: results/).")
    parser.add_argument("--report", default=None,
                        help="If set, generate an HTML report at this path from all successful runs.")
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    result_paths: list[str] = []
    failures: list[tuple[str, str, str]] = []
    for index, (flowfile, content) in enumerate(args.combos, start=1):
        print(f"\n=== [{index}/{len(args.combos)}] Benchmarking {flowfile}/{content} ===")
        try:
            result_paths.append(run_combo(args, flowfile, content, args.output_dir))
        except Exception as error:
            print(f"Error: combo {flowfile}/{content} failed with: {error}")
            failures.append((flowfile, content, str(error)))

    print("\n=== Batch summary ===")
    print(f"Succeeded: {len(result_paths)}/{len(args.combos)}")
    for flowfile, content, error in failures:
        print(f"  FAILED {flowfile}/{content}: {error}")

    if args.report and result_paths:
        import generate_report
        generate_report.write_report(result_paths, args.report)


if __name__ == "__main__":
    main()
