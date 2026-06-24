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
import re
import docker
import humanfriendly
import json
import os
import shutil
import tempfile
import threading
import time
import jinja2
from datetime import datetime, timezone
from enum import Enum

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RESOURCES_DIR = os.path.join(SCRIPT_DIR, "resources")
GET_CONFIG_FILE_TEMPLATE = "get_config.json"
GENERATE_CONFIG_FILE_TEMPLATE = "generate_config.json"
RESULTS_DIR = os.path.join(SCRIPT_DIR, "results")

MINIFI_HOME = "/opt/minifi/minifi-current"
FLOWFILE_REPO_DIR = f"{MINIFI_HOME}/flowfile_repository"
CONTENT_REPO_DIR = f"{MINIFI_HOME}/content_repository"
INPUT_DIR = "/tmp/input"

FLOWFILE_REPOSITORY_CLASSES = {
    "rocksdb": "FlowFileRepository",
    "lmdb": "LmdbFlowFileRepository",
    "volatile": "VolatileFlowFileRepository",
}

CONTENT_REPOSITORY_CLASSES = {
    "rocksdb": "DatabaseContentRepository",
    "lmdb": "LmdbContentRepository",
    "filesystem": "FileSystemRepository",
    "volatile": "VolatileContentRepository",
}

LOG_TIMESTAMP_RE = re.compile(r'\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d+)\]')


class InputGenerationType(str, Enum):
    TIMED_GETFILE = "timed_getfile"
    TIMED_GENERATEFLOWFILE = "timed_generateflowfile"
    BURST = "burst"


def build_properties(flowfile_repository: str, content_repository: str) -> dict[str, str]:
    properties = {
        "nifi.flow.configuration.file": f"{MINIFI_HOME}/conf/config.yml",
        "nifi.extension.path": "../extensions/*",
        "nifi.administrative.yield.duration": "1 sec",
        "nifi.bored.yield.duration": "100 millis",
        "nifi.openssl.fips.support.enable": "false",
        "nifi.provenance.repository.class.name": "NoOpRepository",
        "nifi.flowfile.repository.directory.default": FLOWFILE_REPO_DIR,
        "nifi.database.content.repository.directory.default": CONTENT_REPO_DIR,
        "nifi.flowfile.repository.class.name": FLOWFILE_REPOSITORY_CLASSES[flowfile_repository],
        "nifi.content.repository.class.name": CONTENT_REPOSITORY_CLASSES[content_repository],
    }
    return properties


def write_properties_file(properties: dict[str, str], path: str) -> None:
    with open(path, "w") as properties_file:
        for key, value in properties.items():
            properties_file.write(f"{key}={value}\n")


def repo_size(container, path: str) -> int:
    exit_code, output = container.exec_run(["du", "-sk", path])
    if exit_code != 0:
        return 0
    try:
        return int(output.decode().split()[0]) * 1024
    except (ValueError, IndexError):
        return 0


def read_container_stats(container) -> tuple[int, int, int, int]:
    stats = container.stats(stream=False, one_shot=True)
    memory_stats = stats.get("memory_stats", {})
    usage = memory_stats.get("usage")
    if usage is None:
        mem = 0
    else:
        inactive_file = memory_stats.get("stats", {}).get("inactive_file", 0)
        mem = max(usage - inactive_file, 0)

    cpu_stats = stats.get("cpu_stats", {})
    cpu_total = cpu_stats.get("cpu_usage", {}).get("total_usage", 0)
    system_cpu = cpu_stats.get("system_cpu_usage", 0)
    num_cpus = cpu_stats.get("online_cpus") or len(cpu_stats.get("cpu_usage", {}).get("percpu_usage") or [1])
    return mem, cpu_total, system_cpu, num_cpus


def generate_single_input(input_dir: str, file_size: int, index: int) -> None:
    data = os.urandom(file_size)
    # Write to a temp name then rename so GetFile never reads a partial file.
    tmp_path = os.path.join(input_dir, f".{index}.tmp")
    final_path = os.path.join(input_dir, f"input_{index}.bin")
    with open(tmp_path, "wb") as input_file:
        input_file.write(data)
    os.rename(tmp_path, final_path)


def generate_input(input_dir: str, input_count: int, file_size: int) -> None:
    for i in range(1, input_count + 1):
        generate_single_input(input_dir, file_size, i)


def input_generator_loop(stop_event: threading.Event, input_dir: str, interval: float, file_size: int) -> None:
    counter = 0
    while not stop_event.is_set():
        counter += 1
        generate_single_input(input_dir, file_size, counter)
        stop_event.wait(interval)


def metrics_collector_loop(stop_event: threading.Event, container, samples: list, interval: float, start: float) -> None:
    next_sample = time.monotonic()
    prev_cpu_total = None
    prev_system_cpu = None
    while not stop_event.is_set():
        memory_bytes, cpu_total, system_cpu, num_cpus = read_container_stats(container)
        if prev_cpu_total is not None and system_cpu > prev_system_cpu:
            cpu_percent = (cpu_total - prev_cpu_total) / (system_cpu - prev_system_cpu) * num_cpus * 100.0
        else:
            cpu_percent = 0.0
        prev_cpu_total, prev_system_cpu = cpu_total, system_cpu
        sample = {
            "elapsed_s": round(time.monotonic() - start, 3),
            "flowfile_repo_bytes": repo_size(container, FLOWFILE_REPO_DIR),
            "content_repo_bytes": repo_size(container, CONTENT_REPO_DIR),
            "memory_bytes": memory_bytes,
            "cpu_percent": round(cpu_percent, 2),
        }
        samples.append(sample)
        next_sample += interval
        stop_event.wait(max(0.0, next_sample - time.monotonic()))


def wait_for_minifi_to_start(container, timeout: float = 30.0) -> None:
    deadline = time.monotonic() + timeout
    since = datetime.fromisoformat(container.attrs["Created"])
    while time.monotonic() < deadline:
        container.reload()
        if container.status != "running":
            time.sleep(0.5)
            continue
        now = datetime.now(timezone.utc)
        logs = container.logs(since=since).decode(errors="replace")
        since = now
        if "MiNiFi started" in logs:
            return
        time.sleep(0.5)
    raise RuntimeError(f"Container did not reach running state (status: {container.status})")


def write_config_yml(args: argparse.Namespace, work_dir: str) -> None:
    jinja_env = jinja2.Environment(loader=jinja2.FileSystemLoader(RESOURCES_DIR))
    if args.input_file_generation_type != InputGenerationType.TIMED_GENERATEFLOWFILE:
        flow_config_template = jinja_env.get_template(GET_CONFIG_FILE_TEMPLATE)
        flow_config = flow_config_template.render(get_file_interval=args.input_interval)
        with open(os.path.join(work_dir, "config.yml"), "w") as config_file:
            config_file.write(flow_config)
    else:
        flow_config_template = jinja_env.get_template(GENERATE_CONFIG_FILE_TEMPLATE)
        flow_config = flow_config_template.render(generate_file_interval=args.input_interval,
                                                  generate_file_size=args.input_file_size)
        with open(os.path.join(work_dir, "config.yml"), "w") as config_file:
            config_file.write(flow_config)


def create_minifi_container(args: argparse.Namespace, work_dir: str, input_dir: str) -> docker.models.containers.Container:
    properties_path = os.path.join(work_dir, "minifi.properties")
    properties = build_properties(args.flowfile_repository, args.content_repository)
    write_properties_file(properties, properties_path)

    client = docker.from_env()

    container = client.containers.run(
        args.image,
        detach=True,
        volumes={
            properties_path: {"bind": f"{MINIFI_HOME}/conf/minifi.properties", "mode": "ro"},
            os.path.join(work_dir, "config.yml"): {"bind": f"{MINIFI_HOME}/conf/config.yml", "mode": "ro"},
            input_dir: {"bind": INPUT_DIR, "mode": "rw"},
        },
    )
    return container


def wait_for_flow_files_to_be_processed(container, expected_count: int, timeout: float = 300.0) -> None:
    deadline = time.monotonic() + timeout
    since = datetime.fromisoformat(container.attrs["Created"])
    while True:
        now = datetime.now(timezone.utc)
        logs = container.logs(since=since).decode(errors="replace")
        since = now
        if f"key:flow_file_count value:{expected_count - 1}" in logs:
            break
        container.reload()
        if container.status != "running":
            raise RuntimeError(f"Container exited before processing {expected_count} flow files (status: {container.status})")
        if time.monotonic() > deadline:
            raise RuntimeError(f"Timed out after {timeout}s waiting for {expected_count} flow files to be processed")
        time.sleep(0.1)

    # Wait a bit more to see how the repositories behave after all flow files have been processed.
    time.sleep(2)


def run_threads(samples: list[dict], args: argparse.Namespace) -> tuple[float, int]:
    stop_event = threading.Event()
    container = None
    work_dir = tempfile.mkdtemp(prefix="repo_benchmark_")
    input_dir = os.path.join(work_dir, "input")
    os.makedirs(input_dir)
    write_config_yml(args, work_dir)
    try:
        if args.input_file_generation_type == InputGenerationType.TIMED_GETFILE:
            start = time.monotonic()
            container = create_minifi_container(args, work_dir, input_dir)
            wait_for_minifi_to_start(container)
            threads = [
                threading.Thread(
                    target=input_generator_loop,
                    args=(stop_event, input_dir, args.input_interval, args.input_file_size),
                    daemon=True,
                ),
                threading.Thread(
                    target=metrics_collector_loop,
                    args=(stop_event, container, samples, args.metrics_interval, start),
                    daemon=True,
                ),
            ]
        elif args.input_file_generation_type == InputGenerationType.BURST:
            generate_input(input_dir, args.input_file_count, args.input_file_size)
            start = time.monotonic()
            container = create_minifi_container(args, work_dir, input_dir)
            threads = [
                threading.Thread(
                    target=metrics_collector_loop,
                    args=(stop_event, container, samples, args.metrics_interval, start),
                    daemon=True,
                ),
            ]
        else:
            start = time.monotonic()
            container = create_minifi_container(args, work_dir, input_dir)
            wait_for_minifi_to_start(container)
            threads = [
                threading.Thread(
                    target=metrics_collector_loop,
                    args=(stop_event, container, samples, args.metrics_interval, start),
                    daemon=True,
                ),
            ]

        for thread in threads:
            thread.start()

        if args.input_file_generation_type != InputGenerationType.BURST:
            time.sleep(args.duration)
            stop_event.set()
        else:
            print(f"Waiting for {args.input_file_count} flow files to be processed...")
            wait_for_flow_files_to_be_processed(container, args.input_file_count)
            stop_event.set()

        for thread in threads:
            thread.join(timeout=10)

        return calculate_throughput(container)
    finally:
        stop_event.set()
        try:
            if container is not None:
                container.stop()
        finally:
            if container is not None:
                container.remove()
        shutil.rmtree(work_dir, ignore_errors=True)


def parse_timestamp(log_line: str) -> datetime | None:
    match = LOG_TIMESTAMP_RE.search(log_line)
    if match:
        return datetime.strptime(match.group(1), "%Y-%m-%d %H:%M:%S.%f")
    return None


def calculate_throughput(container) -> tuple[float, int]:
    logs = container.logs().decode(errors="replace")
    count = 0
    first_timestamp = None
    last_timestamp = None
    for line in logs.splitlines():
        if first_timestamp is None and "MiNiFi started" in line:
            timestamp = parse_timestamp(line)
            if timestamp is not None:
                first_timestamp = timestamp
                continue
        if "Logging for flow file" in line:
            timestamp = parse_timestamp(line)
            if timestamp is not None:
                last_timestamp = timestamp
            count += 1

    if first_timestamp is None or last_timestamp is None:
        return 0.0, count
    elapsed = (last_timestamp - first_timestamp).total_seconds()
    if elapsed <= 0:
        return 0.0, count
    return count / elapsed, count


def write_results(samples: list[dict], throughput: float, flow_files_processed: int, args: argparse.Namespace) -> str:
    result = {
        "config": {
            "image": args.image,
            "flowfile_repository": args.flowfile_repository,
            "content_repository": args.content_repository,
            "duration_s": args.duration,
            "input_interval_s": args.input_interval,
            "input_file_size_bytes": args.input_file_size,
            "metrics_interval_s": args.metrics_interval,
            "input_file_generation_type": args.input_file_generation_type,
            "input_file_count": args.input_file_count,
        },
        "samples": samples,
        "throughput": throughput,
        "flow_files_processed": flow_files_processed,
    }

    output_path = args.output
    if output_path is None:
        os.makedirs(RESULTS_DIR, exist_ok=True)
        timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        name = f"{timestamp}_{args.flowfile_repository}_{args.content_repository}_{args.input_file_generation_type}.json"
        output_path = os.path.join(RESULTS_DIR, name)
    else:
        os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)

    with open(output_path, "w") as output_file:
        json.dump(result, output_file, indent=2)

    print(f"Collected {len(samples)} samples; throughput: {throughput:.2f} flow files/sec; results written to {output_path}")
    return output_path


def run(args) -> str:
    samples: list[dict] = []
    throughput, flow_files_processed = run_threads(samples, args)
    return write_results(samples, throughput, flow_files_processed, args)


def main() -> None:
    parser = argparse.ArgumentParser(description="Run the MiNiFi C++ repository benchmark.")
    parser.add_argument("--image", required=True, help="Docker image to use for the benchmark.")
    parser.add_argument("--flowfile-repository", required=True,
                        choices=sorted(FLOWFILE_REPOSITORY_CLASSES), help="Flowfile repository type.")
    parser.add_argument("--content-repository", required=True,
                        choices=sorted(CONTENT_REPOSITORY_CLASSES), help="Content repository type.")
    parser.add_argument("--input-file-generation-type", default=InputGenerationType.TIMED_GETFILE.value,
                        choices=sorted(list([e.value for e in InputGenerationType])),
                        help="Input file generation type. timed_getfile: Generate input files at a fixed interval and use GetFile processor to ingest them. "
                             "timed_generateflowfile: Use GenerateFlowFile processor to generate flowfiles at a fixed interval. "
                             "burst: Generate a burst of input files at the start of the benchmark and use GetFile processor to ingest them.")
    parser.add_argument("--input-file-count", type=int, default=100,
                        help="Number of input files to generate for burst input generation type (default: 100).")
    parser.add_argument("--duration", type=int, default=120,
                        help="Total benchmark session length in seconds (default: 120).")
    parser.add_argument("--input-interval", type=float, default=1.0,
                        help="Seconds between input file generation cycles (default: 1).")
    parser.add_argument("--input-file-size", type=humanfriendly.parse_size, default=humanfriendly.parse_size("1M"),
                        help="Size of each generated input file, e.g. 512K, 1M, 1G (default: 1M).")
    parser.add_argument("--metrics-interval", type=float, default=1.0,
                        help="Seconds between metric samples (default: 1).")
    parser.add_argument("--output", default=None,
                        help="Output JSON path (default: results/<timestamp>_<ff>_<content>.json).")
    args = parser.parse_args()

    run(args)


if __name__ == "__main__":
    main()
