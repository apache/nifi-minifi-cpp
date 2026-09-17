# Atlas extractor behave tests

This directory contains behave integration tests for MiNiFi's C++
`ReportLineageToAtlas` reporting task. Each feature file targets one extractor
under the `KafkaTopicExtractor` / `FilePathExtractor` /
`SiteToSitePortExtractor` triangle -- the extractors for which we have a NiFi
baseline under `baseline/nifi/` to conform to.

## Prerequisites

### Atlas image

The tests use the `adamdebreceni/atlas:latest` image (Apache does not publish
an official Atlas image). `before_all` in `environment.py` pulls it from Docker
Hub at the start of the run, and `LinuxContainer.deploy()` auto-pulls on demand
too, so you don't need to fetch it yourself. If you want to verify it locally:

    docker images | grep adamdebreceni/atlas

### Atlas container lifecycle

`AtlasServerContainer` provisions a fresh `atlas-<scenario_id>` container from
`atlas:latest` at the start of every scenario, publishing REST on host port
`21000` and Kafka on `9092`, and waits up to 5 minutes for cold start. The
container is destroyed at the end of the scenario by the generic teardown hook
like every other per-scenario container (Kafka, MiNiFi, etc.).

Note: because every scenario now pays the full 2-3 minute Atlas cold start,
overall behave wall-clock grows materially compared to the earlier
shared-instance model. Budget CI (and `--behave-timeout`) accordingly.

### Namespace isolation

Each scenario uses `context.scenario_id` as its Atlas metadata namespace and
every `qualifiedName` we assert on ends with `@<scenario_id>`. Now that each
scenario runs against a fresh Atlas container, this suffix is no longer required
for isolation between scenarios in the same run; it is kept for scenario-log
clarity and to keep qualifiedNames stable across reruns.

### CMake flags

- `-DENABLE_ATLAS=ON` -- required. `cmake/DockerConfig.cmake` auto-appends
  `ENABLE_ATLAS` to the behave tag filter (`ENABLED_TAGS`) so the scenarios
  actually run.
- `-DENABLE_KAFKA=ON` -- required for `kafka_topic_extractor.feature`.
- Site-to-Site (`site_to_site_extractor.feature`) doesn't gate on an extra
  option -- standard S2S code is always built.

## Feature file matrix

| File                                | Depends on                                                   | Extractor              |
|-------------------------------------|--------------------------------------------------------------|------------------------|
| `filepath_extractor.feature`        | `GetFile`/`PutFile` (already emit provenance today)          | `FilePathExtractor`    |
| `kafka_topic_extractor.feature`     | `PublishKafka`/`ConsumeKafka` + provenance calls added in    | `KafkaTopicExtractor`  |
|                                     | this change set                                              |                        |
| `site_to_site_extractor.feature`    | `SiteToSiteClient` (already emits provenance today)          | `SiteToSitePortExtractor` |

## Running

Set up Atlas (see above), build MiNiFi with the flags above, then invoke the
usual behave entrypoint with tags including `ENABLE_ATLAS`:

    docker/RunBehaveTests.sh <version> CORE,ENABLE_ATLAS,ENABLE_KAFKA

Individual features can be driven directly with `behave` -- run from
`extensions/atlas/tests/features/` after activating `behave_venv`:

    behave --tags @ENABLE_ATLAS filepath_extractor.feature

Cold-start is dominated by NiFi (for `site_to_site_extractor.feature`) and
Atlas' Kafka-hook consumption cadence (30-60 s per push). Plan
`--behave-timeout` accordingly if you're wrapping this in CI.
