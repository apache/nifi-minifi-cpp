# NiFi `ReportLineageToAtlas` baseline captures

Captures of what Apache NiFi 1.28's `org.apache.nifi.atlas.reporting.ReportLineageToAtlas`
pushes to Apache Atlas, used to spec behave integration tests for the C++ port in
`extensions/atlas/`.

## Environment used to produce these captures

- NiFi 1.28.0-SNAPSHOT (single-node, running on host).
  - `file-mode/` and `directory-mode/` captures used the secured configuration
    (`https://nifi1:8443`, `SingleUserAuthorizer`).
  - `file-mode-with-s2s/` capture used an unsecured configuration
    (`http://nifi1:8080`, no auth, `nifi.remote.input.secure=false`) so a
    self-loop RPG could authenticate as an anonymous peer. `nifi.properties`
    was patched to blank out all `nifi.security.*` and `nifi.web.https.*` keys
    and set the HTTP listener + `nifi.security.allow.anonymous.authentication=true`.
    See `common/` for the auto-generated `atlas-application.properties` from each run.
- Atlas 2.5.0 in Docker (`atlas` container, REST on `localhost:21000`, embedded Kafka
  on `localhost:9092` with topic `ATLAS_HOOK`, creds `admin`/`admin`).
- Data-plane containers spun up specifically for the capture:
  - `baseline-kafka` — `apache/kafka:4.1.0` on host port `19092`.
  - `baseline-minio` — `minio/minio` on host ports `19000` / `19001`,
    bucket `baseline-bucket`, creds `minioadmin`/`minioadmin`.

## Flow shape

Root PG:
- Input port `baseline-input` → PutFile `s2s-sink` (`/tmp/atlas-baseline/s2s-out`).
- Sub-PG `AtlasBaseline` containing four sub-PGs:
  - `kafka-pg`: GenerateFlowFile → PublishKafka_2_6 (topic `baseline-kafka-topic`),
    plus ConsumeKafka_2_6 → PutFile.
  - `filepath-pg`: GetFile `/tmp/atlas-baseline/in` → PutFile `/tmp/atlas-baseline/out`.
  - `s3-pg`: GenerateFlowFile → PutS3Object (bucket `baseline-bucket`).
  - `s2s-pg`: GenerateFlowFile → RPG(`http://nifi1:8080/nifi`) → root `baseline-input`
    port (via HTTP-transport S2S). Only wired in the `file-mode-with-s2s/` capture,
    which used the unsecured NiFi profile.

## Reporting task config (both runs)

- Atlas URLs: `http://localhost:21000`
- Auth: basic `admin`/`admin`
- NiFi URL for Atlas: `https://nifi1:8443/nifi`
- Lineage Strategy: `SimplePath`
- Provenance Batch Size: 1000
- Provenance Start Position: `beginning-of-stream`
- Create Atlas Configuration File: `true`
- Kafka Bootstrap Servers: `localhost:9092` (Atlas embedded Kafka)
- AWS S3 Model Version: `v2`
- Schedule: `30 sec`
- Runs differ only in `Filesystem Path Entities Level` (`FILE` vs `DIRECTORY`)
  and Atlas Default Metadata Namespace (`baseline` vs `baseline-dir`).

See `common/atlas-application.properties.*` for the auto-generated Atlas client conf.

## What's in each capture

`file-mode/` and `directory-mode/` each contain:
- `atlas-basic.json` — filtered result of `GET /api/atlas/v2/search/basic?typeName=…`
  for each expected type, restricted to the run's namespace, with volatile fields
  (guid, createTime, updateTime, …) stripped.
- `atlas-detailed.json` — filtered per-guid entity dumps (`GET /api/atlas/v2/entity/guid/{guid}`).
- `atlas-hook.jsonl` — raw messages consumed from the `ATLAS_HOOK` Kafka topic during
  the run.
- `hook-parsed.json` — same messages, JSON-decoded and pretty-printed for readability.
- `summary.json` / `summary.md` — quick overview of counts, flow paths, and datasets.

## Extractor coverage

| C++ extractor        | Baselined here? | Notes |
|----------------------|-----------------|-------|
| `KafkaTopicExtractor`   | ✅ yes          | `kafka_topic` entity, qn `baseline-kafka-topic@baseline`, uri `PLAINTEXT://localhost:19092/baseline-kafka-topic`. |
| `FilePathExtractor`     | ✅ yes          | Two modes captured — FILE: `/tmp/atlas-baseline/in/hello.txt@baseline`; DIRECTORY: `/tmp/atlas-baseline/in@baseline-dir`. |
| `SiteToSitePortExtractor` | ✅ yes (unsecure NiFi, `file-mode-with-s2s/`) | Full 3-flow-path chain captured: `s2s-source` → `nifi_queue` → `Remote Input Port` → `nifi_input_port` → `baseline-input, s2s-sink`. Transit URI shape is `http://nifi1:8080/nifi-api/data-transfer/input-ports/<remote-port-uuid>/transactions/…` for HTTP transport. Requires unauth'd NiFi (SingleUserAuthorizer blocks self-loop RPG under secure mode). |

## Notable observations from the captures

1. **`nifi_flow_path.qualifiedName`** = `<processor-id-of-path-head>@<namespace>`.
   The path name concatenates all processor names in the linear chain, e.g.
   `"kafka-source, kafka-publish"` and `"file-get, file-put"`.
2. **`kafka_topic.uri`** is composed as `<security-protocol>://<host>:<port>/<topic>` — for
   plaintext PublishKafka against `localhost:19092`/`baseline-kafka-topic` this is
   `PLAINTEXT://localhost:19092/baseline-kafka-topic`.
3. **`nifi_data`** is the fallback dataset type — every `unknown.*` analyzer produces
   one, using `<processor-id>@<namespace>` as qualifiedName and the processor type name
   (`GenerateFlowFile`, `PutS3Object`, …) as `name`.
4. **Filesystem path level** materially changes the `fs_path.qualifiedName`:
   - FILE: `/tmp/atlas-baseline/in/hello.txt@baseline`.
   - DIRECTORY: `/tmp/atlas-baseline/in@baseline-dir` (the containing directory only).
5. **Hook messages** come in two flavors:
   - `ENTITY_CREATE_V2` (a batched entity registration).
   - Legacy `ENTITY_CREATE` / `ENTITY_PARTIAL_UPDATE` (per-entity/per-flow-path).
   The reporting task sends both. Partial updates carry the `outputs`/`inputs` reference
   arrays that later become the lineage edges in Atlas.
6. **Site-to-Site produces a 3-flow-path chain**:
   `<sender flow_path>` → `nifi_queue` → `Remote Input Port` flow_path → `nifi_input_port`
   → `<receiver flow_path>`.
   - `nifi_queue.qualifiedName` = `<remote-port-uuid>@<namespace>` (name is literally
     `"queue"`).
   - The `Remote Input Port` flow_path has `qualifiedName = <remote-port-uuid>@<namespace>`
     and `name = "Remote Input Port"`.
   - The receiving `nifi_input_port` entity has `qualifiedName = <local-input-port-uuid>@<namespace>`
     and `name = <port-name>` (e.g. `"baseline-input"`).

## Known gaps in this baseline

- S3 lineage was not extracted by NiFi due to the transit-URI regex mismatch — see table.
- Only the SimplePath lineage strategy was captured; CompletePath (backward
  lineage walk from DROP events) would require a separate run.
- The `file-mode` and `directory-mode` captures were done before the S2S self-loop was
  wired end-to-end — those directories don't contain `nifi_queue` entities or the
  Remote-Input-Port flow_path. For a full S2S lineage chain (including FS-path-level
  DIRECTORY semantics for the S2S sink), a re-run would be needed under the unsecure
  NiFi config.

## How to regenerate

The scripts in `/tmp/atlas-baseline-capture/` were used to produce these captures:
- `nifi_wipe.py`         — clear the NiFi canvas.
- `nifi_flow_builder.py` — build the AtlasBaseline PG + reporting task.
- `create_rt.py`         — create the reporting task (parameterized by
                            `ATLAS_NAMESPACE` and `FS_PATH_LEVEL` env vars).
- `run_and_capture.py`   — start the flow + hook recorder, wait, snapshot Atlas.
- `persist_fixtures.py`  — filter, strip volatile fields, and copy to this dir.

They are intentionally not committed — they hard-code the local `nifi1:8443` /
Docker Atlas environment. Save the important logic into behave-framework
container/step files if we want a repeatable baseline job.
