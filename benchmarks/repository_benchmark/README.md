<!--
Licensed to the Apache Software Foundation (ASF) under one or more
contributor license agreements.  See the NOTICE file distributed with
this work for additional information regarding copyright ownership.
The ASF licenses this file to You under the Apache License, Version 2.0
(the "License"); you may not use this file except in compliance with
the License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# Repository benchmark

Tools for comparing MiNiFi C++ FlowFile and content repository implementations
(RocksDB, LMDB, filesystem, volatile) under a controlled workload. Each run
starts a MiNiFi container with a chosen repository combination, samples its
resource usage over time, and records throughput; a report overlays several runs
so the implementations can be compared side by side.

## Prerequisites

- Docker (the current user must be able to run containers).
- A MiNiFi C++ Docker image (e.g. built via `make docker`), referenced with
  `--image`.
- Python 3.10+ and the dependencies:

  ```bash
  python3 -m venv venv && source venv/bin/activate
  pip install -r requirements.txt
  ```

## Single run

```bash
python3 run_benchmark.py \
  --image apacheminificpp:1.0.0 \
  --flowfile-repository lmdb \
  --content-repository lmdb
```

Results are written to `results/<timestamp>_<flowfile>_<content>_<type>.json`
unless `--output` is given.

### Input generation modes (`--input-file-generation-type`)

- `timed_getfile` (default): write input files into a mounted directory at a
  fixed interval; a `GetFile` processor ingests them. Runs for `--duration`.
- `timed_generateflowfile`: a `GenerateFlowFile` processor produces flow files
  in-process at a fixed interval. Runs for `--duration`.
- `burst`: generate `--input-file-count` files up front, then ingest them all
  with `GetFile`. Ends once every file has been processed (bounded by a timeout).

Other useful flags: `--duration`, `--input-interval`, `--input-file-size`
(accepts `512K`, `1M`, `1G`), `--input-file-count`, `--metrics-interval`.

## Batch run (compare several combinations)

`run_batch.py` runs a set of repository combinations against the same workload,
**sequentially** (containers do not compete for CPU/IO, which keeps the numbers
comparable), and can generate the report automatically:

```bash
python3 run_batch.py \
  --image apacheminificpp:1.0.0 \
  --combo lmdb:lmdb \
  --combo rocksdb:rocksdb \
  --combo rocksdb:filesystem \
  --input-file-generation-type burst \
  --input-file-count 100 \
  --report report.html
```

`--combo FLOWFILE:CONTENT` is repeatable. The remaining workload flags are shared
by every combo and match `run_benchmark.py`. If one combo fails the others still
run, and the failure is listed in the final summary.

## Report

To build a report from existing result files directly:

```bash
python3 generate_report.py results/*.json -o report.html
```

The report contains a **summary table** for quick comparison plus time-series
charts. Metrics:

| Metric | Meaning |
| --- | --- |
| Throughput (ff/s) | Flow files processed per second (from container logs). |
| Files processed | Total flow files processed during the run. |
| Peak / mean memory | Container memory usage (`inactive_file` excluded). |
| Mean / p95 CPU | Container CPU usage; 100% = one core. |
| Final FF / content repo | On-disk repository size at the end of the run (`du`). |
| Peak content repo | Largest content repository size observed. |
