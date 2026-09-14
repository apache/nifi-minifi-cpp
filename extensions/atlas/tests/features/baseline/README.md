# NiFi ↔ Atlas baseline

This directory holds the reference material for the C++ `ReportLineageToAtlas`
port in `extensions/atlas/`:

- **`nifi/`** — captured fixtures of what Apache NiFi's own
  `org.apache.nifi.atlas.reporting.ReportLineageToAtlas` pushes to Apache Atlas
  for the same set of extractors the C++ port implements. Used to spec the
  behave integration tests. See `nifi/README.md` for extractor coverage and
  notable behavioral observations.
- **`scripts/`** — the CLI tool that produced (and can reproduce) those
  fixtures. See `scripts/README.md` for prereqs and usage.
