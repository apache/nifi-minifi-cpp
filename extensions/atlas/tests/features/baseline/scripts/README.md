# Baseline capture scripts

`baseline_capture.py` reproduces the fixtures under `../nifi/` — the captures of what
Apache NiFi's `ReportLineageToAtlas` pushes to Atlas for the extractors the C++ port
in `extensions/atlas/` reimplements.

## Prereqs

- Docker.
- Python 3.10+.
- `pip install -r requirements.txt` (single dep: `requests`).
- A running Apache Atlas 2.5+ reachable at the URL passed via `--atlas-url` (default
  `http://localhost:21000`), with its embedded Kafka's `ATLAS_HOOK` topic reachable
  as `--atlas-kafka` (default `localhost:9092`).
- A running Apache NiFi with the `nifi-atlas-nar` bundle loaded, reachable at
  `--nifi-url` (default `http://nifi1:8080`).

## Quick start

```
cd extensions/atlas/tests/features/baseline/scripts
pip install -r requirements.txt
python3 baseline_capture.py all
```

This runs the full pipeline against the defaults: brings up `baseline-kafka` and
`baseline-minio`, wipes NiFi, builds the flow, creates the reporting task, waits
120 s, snapshots Atlas, and writes fixtures to `../nifi/file-mode/`.

## Subcommands

All subcommands accept the same global options — see `python3 baseline_capture.py
<sub> --help` for the full list. Global defaults reproduce the local dev
environment used to produce the committed fixtures.

| Subcommand         | What it does                                                 |
|--------------------|--------------------------------------------------------------|
| `containers-up`    | `docker run` `baseline-kafka` + `baseline-minio`, create the bucket, create `/tmp/atlas-baseline/{in,out,kafka-out,s2s-out}` on the host, drop a seed file for `GetFile`. |
| `containers-down`  | `docker rm -f` both containers.                              |
| `wipe`             | Clear NiFi's root canvas + delete any `ReportLineageToAtlas` reporting tasks. |
| `build-flow`       | Create the `AtlasBaseline` PG (kafka-pg, filepath-pg, s3-pg, s2s-pg). With `--with-s2s`, also wire the RPG self-loop to the root `baseline-input` port and enable transmission. |
| `create-rt`        | Create the reporting task with the given `--namespace`, `--fs-path-level`, and `--strategy`. |
| `delete-rt`        | Stop + delete every `ReportLineageToAtlas` reporting task.   |
| `run`              | Start the flow + reporting task, start the `ATLAS_HOOK` recorder in a throwaway `apache/kafka` container, wait `--wait-secs` seconds, then snapshot Atlas via the search-basic + per-guid endpoints and pull the hook stream. Output goes to `<workdir>/run-<namespace>-<fs-path-level>/`. |
| `persist`          | Read the latest `run-…` output, filter to entities in `--namespace`, strip GUIDs/timestamps, and write to `../nifi/<preset>/`. Preset defaults to `<fs-path-level lower>-mode[-with-s2s]`. |
| `all`              | `wipe` → `containers-up` → `build-flow` → `create-rt` → `run` → `persist`. |

## Reproducing each committed fixture directory

The three subdirs under `../nifi/` were produced with:

```
# ../nifi/file-mode/            (secured NiFi, no S2S)
python3 baseline_capture.py all \
    --nifi-url https://nifi1:8443 --nifi-user <u> --nifi-password <p> \
    --namespace baseline --fs-path-level FILE

# ../nifi/directory-mode/       (secured NiFi, no S2S, DIRECTORY fs_path level)
python3 baseline_capture.py all \
    --nifi-url https://nifi1:8443 --nifi-user <u> --nifi-password <p> \
    --namespace baseline-dir --fs-path-level DIRECTORY

# ../nifi/file-mode-with-s2s/   (UNSECURE NiFi, S2S wired)
python3 baseline_capture.py all --with-s2s \
    --namespace baseline-s2s --fs-path-level FILE
```

## Site-to-Site: NiFi must be running unsecure

A self-loop RPG has to authenticate to its own peer to retrieve the peer's
input-port list. NiFi's default `SingleUserAuthorizer` recognizes only the one
username/password user and rejects any peer, so the RPG stays in
`Establishing connection to <url>` forever.

The simplest way around it — used to produce the committed `file-mode-with-s2s/`
capture — is to run NiFi with transport security disabled and anonymous auth
enabled. Patch `nifi.properties` as follows (back up first):

```
# blank all security keys
nifi.security.keystore=
nifi.security.keystoreType=
nifi.security.keystorePasswd=
nifi.security.keyPasswd=
nifi.security.truststore=
nifi.security.truststoreType=
nifi.security.truststorePasswd=
nifi.security.user.authorizer=
nifi.security.user.login.identity.provider=
nifi.security.allow.anonymous.authentication=true

# switch web listener from HTTPS to HTTP
nifi.web.http.host=<host>
nifi.web.http.port=8080
nifi.web.https.host=
nifi.web.https.port=

# unsecure S2S
nifi.remote.input.secure=false
nifi.remote.input.host=<host>
nifi.remote.input.socket.port=10000
```

Restart NiFi. Then `--with-s2s` works. Restore the original file when done.

## Iterating on part of the pipeline

The subcommands are composable — you don't have to redo the whole capture to
tweak one thing.

Reshape the flow but keep the reporting task config:

```
python3 baseline_capture.py wipe
python3 baseline_capture.py build-flow --with-s2s
# (leave RT alone if it's already there, or)
python3 baseline_capture.py delete-rt
python3 baseline_capture.py create-rt --namespace baseline-experiment
python3 baseline_capture.py run --namespace baseline-experiment --wait-secs 90
python3 baseline_capture.py persist --namespace baseline-experiment \
    --preset my-experiment
```

Re-persist without re-running (e.g. after changing the volatile-fields filter):

```
python3 baseline_capture.py persist --namespace baseline-s2s --with-s2s
```

## What's in the fixture output

See `../nifi/README.md` for the shape of what each fixture directory holds and how
those captures inform the C++ behave-test spec — extractor coverage table,
notable observations about `qualifiedName` shapes, hook message format, etc.
