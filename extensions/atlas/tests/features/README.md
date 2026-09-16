# Atlas extractor behave tests

This directory contains behave integration tests for MiNiFi's C++
`ReportLineageToAtlas` reporting task. Each feature file targets one extractor
under the `KafkaTopicExtractor` / `FilePathExtractor` /
`SiteToSitePortExtractor` triangle -- the extractors for which we have a NiFi
baseline under `baseline/nifi/` to conform to.

## Prerequisites

### `atlas:latest` image

Apache does not publish an official Atlas image. **You must build (or otherwise
obtain) an `atlas:latest` image locally** before invoking the tests. Verify with:

    docker images | grep atlas:latest

### Atlas container lifecycle

`AtlasServerContainer` self-provisions the atlas container - you don't need to
start one yourself. On the first scenario in a run it either:

- Reuses a running container named `atlas` if its `/api/atlas/admin/status`
  endpoint returns ACTIVE within a few seconds, OR
- Removes any existing `atlas` container and starts a fresh one from
  `atlas:latest`, publishing REST on `21000` and Kafka on `9092`, then waits
  up to 5 minutes for cold start.

Subsequent scenarios in the same run reuse the container (per-scenario
namespacing via `@<scenario_id>` in every qualifiedName keeps them isolated on
Atlas' side). Only network attach/detach happens per scenario.

Set `ATLAS_FORCE_RECREATE=1` in the environment to force a clean rebuild - useful
when the atlas image has gotten wedged (the `atlas:latest` image is known to hang
its bulk-entity endpoint after some load; a fresh container clears the corruption).

### Namespace isolation

To keep multiple scenarios (or reruns of the same scenario) from colliding on
the shared Atlas state, each scenario uses `context.scenario_id` as its Atlas
metadata namespace. Every `qualifiedName` we assert on ends with
`@<scenario_id>`, so the graph accumulates entities per scenario forever but
they never overwrite each other. Bulk-clean the Atlas dev instance
periodically if the graph gets unwieldy.

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
