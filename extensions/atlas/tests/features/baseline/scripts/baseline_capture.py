#!/usr/bin/env python3
"""Baseline capture tool for NiFi's ReportLineageToAtlas reporting task.

Reproduces the captures under `extensions/atlas/tests/features/baseline/nifi/`:
brings up data-plane containers (Kafka, MinIO), builds a NiFi flow that
exercises the extractors, configures the reporting task, waits for provenance
events to flow into Atlas, then snapshots what Atlas received and writes it
as fixtures.

See scripts/README.md for prereqs and usage examples.
"""
from __future__ import annotations

import argparse
import copy
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

try:
    import requests
except ImportError:
    sys.exit("This tool needs the 'requests' package. Run:\n"
             "  pip install -r requirements.txt")

# ---------------------------------------------------------------------------
# Global configuration (argparse namespace mirror).
# ---------------------------------------------------------------------------

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_FIXTURE_DIR = SCRIPT_DIR.parent / "nifi"
DEFAULT_WORKDIR = Path("/tmp/atlas-baseline-capture")

CLIENT_ID = "atlas-baseline"

# NiFi ReportLineageToAtlas property descriptor keys (as returned by the REST API,
# probed once with a stub RT — captured here as a constant so we don't have to probe
# every run).
RT_PROPS_TEMPLATE = {
    "atlas-urls": None,                     # filled from --atlas-url
    "atlas-conf-create": "true",
    "atlas-conf-dir": None,                 # filled per run
    "atlas-default-cluster-name": None,     # filled from --namespace
    "nifi-lineage-strategy": None,          # filled from --strategy
    "provenance-start-position": "beginning-of-stream",
    "provenance-batch-size": "1000",
    "atlas-nifi-url": None,                 # filled from --nifi-url
    "atlas-authentication-method": "basic",
    "atlas-username": None,                 # filled from --atlas-user
    "atlas-password": None,                 # filled from --atlas-password
    "kafka-bootstrap-servers": None,        # filled from --atlas-kafka
    "kafka-security-protocol": "PLAINTEXT",
    "aws-s3-model-version": "v2",
    "filesystem-paths-level": None,         # filled from --fs-path-level
}

# ---------------------------------------------------------------------------
# NiFi REST client.
# ---------------------------------------------------------------------------


class Nifi:
    """Thin wrapper over the NiFi REST API. Supports both anonymous (unsecure)
    and token-authenticated (secure) NiFi installations."""

    def __init__(self, url: str, user: str | None = None,
                 password: str | None = None, verify_tls: bool = False):
        self.url = url.rstrip("/")
        self.user = user
        self.password = password
        self.session = requests.Session()
        self.session.verify = verify_tls
        if not verify_tls:
            # Suppress the "InsecureRequestWarning" spam for self-signed dev certs.
            import urllib3
            urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
        if user and password:
            self.session.headers["Authorization"] = f"Bearer {self._token()}"

    def _token(self) -> str:
        r = self.session.post(f"{self.url}/nifi-api/access/token",
                              data={"username": self.user, "password": self.password})
        r.raise_for_status()
        return r.text

    def _raise(self, resp: requests.Response) -> None:
        if not resp.ok:
            sys.stderr.write(f"ERROR {resp.status_code} on {resp.request.method} "
                             f"{resp.request.url}\n")
            sys.stderr.write(resp.text[:2000] + "\n")
            resp.raise_for_status()

    def get(self, path: str, **kw) -> Any:
        r = self.session.get(f"{self.url}{path}", **kw)
        self._raise(r)
        return r.json() if r.text else {}

    def post(self, path: str, **kw) -> Any:
        r = self.session.post(f"{self.url}{path}", **kw)
        self._raise(r)
        return r.json() if r.text else {}

    def put(self, path: str, **kw) -> Any:
        r = self.session.put(f"{self.url}{path}", **kw)
        self._raise(r)
        return r.json() if r.text else {}

    def delete(self, path: str, **kw) -> Any:
        r = self.session.delete(f"{self.url}{path}", **kw)
        self._raise(r)
        return r.json() if r.text else {}

    # Component factories -----------------------------------------------------

    def root_id(self) -> str:
        return self.get("/nifi-api/flow/process-groups/root")["processGroupFlow"]["id"]

    def create_pg(self, parent_id: str, name: str, x: float, y: float) -> str:
        body = {"revision": {"version": 0, "clientId": CLIENT_ID},
                "component": {"name": name, "position": {"x": x, "y": y}}}
        return self.post(f"/nifi-api/process-groups/{parent_id}/process-groups",
                         json=body)["id"]

    def create_processor(self, pg_id: str, ptype: str, name: str,
                         x: float, y: float, properties: dict | None = None,
                         auto_term: list[str] | None = None,
                         schedule_period: str | None = None) -> dict:
        body = {"revision": {"version": 0, "clientId": CLIENT_ID},
                "component": {"type": ptype, "name": name,
                              "position": {"x": x, "y": y}, "config": {}}}
        if properties:
            body["component"]["config"]["properties"] = properties
        if auto_term:
            body["component"]["config"]["autoTerminatedRelationships"] = auto_term
        if schedule_period:
            body["component"]["config"]["schedulingPeriod"] = schedule_period
        return self.post(f"/nifi-api/process-groups/{pg_id}/processors", json=body)

    def create_connection(self, pg_id: str, source: dict, dest: dict,
                          selected_rels: list[str]) -> dict:
        body = {"revision": {"version": 0, "clientId": CLIENT_ID},
                "component": {"source": source, "destination": dest,
                              "selectedRelationships": selected_rels,
                              "flowFileExpiration": "0 sec"}}
        return self.post(f"/nifi-api/process-groups/{pg_id}/connections", json=body)

    def create_input_port(self, pg_id: str, name: str, x: float, y: float) -> dict:
        body = {"revision": {"version": 0, "clientId": CLIENT_ID},
                "component": {"name": name, "position": {"x": x, "y": y}}}
        return self.post(f"/nifi-api/process-groups/{pg_id}/input-ports", json=body)

    def create_rpg(self, pg_id: str, target_uri: str, x: float, y: float,
                   transport: str = "HTTP") -> dict:
        body = {"revision": {"version": 0, "clientId": CLIENT_ID},
                "component": {"targetUri": target_uri, "targetUris": target_uri,
                              "transportProtocol": transport,
                              "position": {"x": x, "y": y}}}
        return self.post(f"/nifi-api/process-groups/{pg_id}/remote-process-groups",
                         json=body)

    def set_pg_state(self, pg_id: str, state: str) -> None:
        self.put(f"/nifi-api/flow/process-groups/{pg_id}",
                 json={"id": pg_id, "state": state,
                       "disconnectedNodeAcknowledged": False})

    def set_processor_config(self, proc_id: str, config_patch: dict) -> None:
        current = self.get(f"/nifi-api/processors/{proc_id}")
        ver = current["revision"]["version"]
        self.put(f"/nifi-api/processors/{proc_id}",
                 json={"revision": {"version": ver, "clientId": CLIENT_ID},
                       "component": {"id": proc_id, "config": config_patch}})

    def set_rpg_state(self, rpg_id: str, state: str) -> None:
        ver = self.get(f"/nifi-api/remote-process-groups/{rpg_id}")["revision"]["version"]
        self.put(f"/nifi-api/remote-process-groups/{rpg_id}/run-status",
                 json={"revision": {"version": ver, "clientId": CLIENT_ID},
                       "state": state, "disconnectedNodeAcknowledged": False})


def proc_ref(proc: dict) -> dict:
    c = proc["component"]
    return {"id": c["id"], "groupId": c["parentGroupId"], "type": "PROCESSOR"}


# ---------------------------------------------------------------------------
# Subcommand: containers-up / containers-down.
# ---------------------------------------------------------------------------


def _docker(*args: str, check: bool = True, quiet: bool = False,
            capture: bool = False) -> subprocess.CompletedProcess:
    kw: dict = {}
    if quiet:
        kw["stdout"] = subprocess.DEVNULL
        kw["stderr"] = subprocess.DEVNULL
    if capture:
        kw["capture_output"] = True
        kw["text"] = True
    return subprocess.run(["docker", *args], check=check, **kw)


def cmd_containers_up(args: argparse.Namespace) -> None:
    data_kafka_port = args.data_kafka.split(":")[-1]
    minio_port = args.minio_endpoint.rsplit(":", 1)[-1].split("/")[0]

    _docker("rm", "-f", "baseline-kafka", "baseline-minio", quiet=True, check=False)
    print("Starting baseline-kafka …")
    _docker(
        "run", "-d", "--name", "baseline-kafka",
        "-p", f"{data_kafka_port}:{data_kafka_port}",
        "-e", "KAFKA_NODE_ID=1",
        "-e", "KAFKA_PROCESS_ROLES=broker,controller",
        "-e", "KAFKA_LISTENERS=PLAINTEXT://:9092,CONTROLLER://:9093,"
              f"HOST://:{data_kafka_port}",
        "-e", "KAFKA_ADVERTISED_LISTENERS=PLAINTEXT://baseline-kafka:9092,"
              f"HOST://localhost:{data_kafka_port}",
        "-e", "KAFKA_LISTENER_SECURITY_PROTOCOL_MAP="
              "PLAINTEXT:PLAINTEXT,CONTROLLER:PLAINTEXT,HOST:PLAINTEXT",
        "-e", "KAFKA_CONTROLLER_LISTENER_NAMES=CONTROLLER",
        "-e", "KAFKA_INTER_BROKER_LISTENER_NAME=PLAINTEXT",
        "-e", "KAFKA_CONTROLLER_QUORUM_VOTERS=1@localhost:9093",
        "apache/kafka:4.1.0",
    )

    print("Starting baseline-minio …")
    _docker(
        "run", "-d", "--name", "baseline-minio",
        "-p", f"{minio_port}:9000",
        "-p", "19001:9001",
        "-e", f"MINIO_ROOT_USER={args.minio_user}",
        "-e", f"MINIO_ROOT_PASSWORD={args.minio_password}",
        "minio/minio", "server", "/data", "--console-address", ":9001",
    )

    # Wait for MinIO ready, then create the bucket.
    print("Waiting for MinIO to accept requests …")
    for _ in range(30):
        try:
            r = requests.get(f"{args.minio_endpoint}/minio/health/ready", timeout=2)
            if r.status_code == 200:
                break
        except requests.RequestException:
            pass
        time.sleep(1)

    print(f"Creating bucket {args.bucket} …")
    _docker(
        "run", "--rm", "--network", "host", "--entrypoint", "sh",
        "minio/mc", "-c",
        f"mc alias set local {args.minio_endpoint} "
        f"{args.minio_user} {args.minio_password} && "
        f"mc mb --ignore-existing local/{args.bucket}",
    )

    # Ensure host filesystem dirs exist (for FilePath extractor).
    for sub in ("in", "out", "kafka-out", "s2s-out"):
        os.makedirs(f"/tmp/atlas-baseline/{sub}", exist_ok=True)
    seed = Path("/tmp/atlas-baseline/in/hello.txt")
    if not seed.exists():
        seed.write_text("atlas-baseline-payload\n")
    print("Containers ready.")


def cmd_containers_down(args: argparse.Namespace) -> None:
    _docker("rm", "-f", "baseline-kafka", "baseline-minio",
            quiet=False, check=False)


# ---------------------------------------------------------------------------
# Subcommand: wipe / delete-rt.
# ---------------------------------------------------------------------------


def _stop_and_delete(nifi: Nifi, root: str) -> None:
    # Recursively stop every processor + RPG in the tree.
    try:
        nifi.put(f"/nifi-api/flow/process-groups/{root}",
                 json={"id": root, "state": "STOPPED",
                       "disconnectedNodeAcknowledged": False})
    except Exception as e:
        print(f"  warn: recursive stop failed: {e}")
    # Explicitly stop transmission on every RPG in the tree
    # (STOPPED on a PG doesn't disable RPG transmission — that's a separate axis).
    try:
        nifi.put(f"/nifi-api/remote-process-groups/process-group/{root}/run-status",
                 json={"state": "STOPPED",
                       "disconnectedNodeAcknowledged": False})
    except Exception:
        pass

    # Drop any queued FlowFiles so connections can be deleted.
    def _drop_queues(pg_id: str) -> None:
        flow = nifi.get(f"/nifi-api/flow/process-groups/{pg_id}"
                        )["processGroupFlow"]["flow"]
        for conn in flow.get("connections", []):
            try:
                nifi.post(f"/nifi-api/flowfile-queues/{conn['id']}/drop-requests")
            except Exception as e:
                print(f"  warn: drop queue {conn['id']} failed: {e}")
        for sub in flow.get("processGroups", []):
            _drop_queues(sub["id"])

    _drop_queues(root)

    # Now delete every direct child of root. Individual PGs cascade — deleting a PG
    # deletes everything inside it. Order matters: connections → processors → RPGs
    # → ports → PGs → funnels/labels.
    flow = nifi.get(f"/nifi-api/flow/process-groups/{root}")["processGroupFlow"]["flow"]
    delete_order = [
        ("connections", "connections"),
        ("processors", "processors"),
        ("remoteProcessGroups", "remote-process-groups"),
        ("inputPorts", "input-ports"),
        ("outputPorts", "output-ports"),
        ("processGroups", "process-groups"),
        ("funnels", "funnels"),
        ("labels", "labels"),
    ]
    for kind, path in delete_order:
        for item in flow.get(kind, []):
            try:
                nifi.delete(f"/nifi-api/{path}/{item['id']}",
                            params={"version": item["revision"]["version"],
                                    "clientId": CLIENT_ID,
                                    "disconnectedNodeAcknowledged": "false"})
                print(f"  deleted {path}/{item['id']}")
            except Exception as e:
                print(f"  warn: delete {path}/{item['id']} failed: {e}")


def _delete_reporting_tasks(nifi: Nifi,
                            type_contains: str = "ReportLineageToAtlas") -> None:
    for rt in nifi.get("/nifi-api/flow/reporting-tasks").get("reportingTasks", []):
        if type_contains not in rt["component"]["type"]:
            continue
        try:
            nifi.put(f"/nifi-api/reporting-tasks/{rt['id']}/run-status",
                     json={"revision": {"version": rt["revision"]["version"],
                                        "clientId": CLIENT_ID},
                           "state": "STOPPED",
                           "disconnectedNodeAcknowledged": False})
        except Exception:
            pass
        ver = nifi.get(f"/nifi-api/reporting-tasks/{rt['id']}")["revision"]["version"]
        nifi.delete(f"/nifi-api/reporting-tasks/{rt['id']}",
                    params={"version": ver, "clientId": CLIENT_ID,
                            "disconnectedNodeAcknowledged": "false"})
        print(f"  deleted reporting-task/{rt['id']}")


def cmd_wipe(args: argparse.Namespace) -> None:
    nifi = _mk_nifi(args)
    _delete_reporting_tasks(nifi)
    _stop_and_delete(nifi, nifi.root_id())
    print("Wipe complete.")


def cmd_delete_rt(args: argparse.Namespace) -> None:
    nifi = _mk_nifi(args)
    _delete_reporting_tasks(nifi)


# ---------------------------------------------------------------------------
# Subcommand: build-flow.
# ---------------------------------------------------------------------------


def cmd_build_flow(args: argparse.Namespace) -> None:
    nifi = _mk_nifi(args)
    root = nifi.root_id()
    print(f"root PG = {root}")

    # Root-level baseline-input port (S2S peer target) + PutFile sink.
    baseline_input = nifi.create_input_port(root, "baseline-input", 100, 100)
    print(f"baseline-input port = {baseline_input['id']}")
    s2s_sink = nifi.create_processor(
        root, "org.apache.nifi.processors.standard.PutFile", "s2s-sink",
        400, 100,
        properties={"Directory": "/tmp/atlas-baseline/s2s-out"},
        auto_term=["success", "failure"])
    nifi.create_connection(
        root,
        {"id": baseline_input["id"], "groupId": root, "type": "INPUT_PORT"},
        {"id": s2s_sink["id"], "groupId": root, "type": "PROCESSOR"},
        [""])

    # Umbrella PG.
    baseline_pg = nifi.create_pg(root, "AtlasBaseline", 100, 300)

    # ---- kafka-pg ----
    kafka_pg = nifi.create_pg(baseline_pg, "kafka-pg", 50, 50)
    k_source = nifi.create_processor(
        kafka_pg, "org.apache.nifi.processors.standard.GenerateFlowFile",
        "kafka-source", 50, 50,
        properties={"File Size": "10 B", "generate-ff-custom-text": "hello",
                    "Batch Size": "1"},
        schedule_period="10 sec")
    k_publish = nifi.create_processor(
        kafka_pg, "org.apache.nifi.processors.kafka.pubsub.PublishKafka_2_6",
        "kafka-publish", 50, 250,
        properties={"bootstrap.servers": args.data_kafka,
                    "topic": args.kafka_topic,
                    "use-transactions": "false"},
        auto_term=["failure", "success"])
    k_consume = nifi.create_processor(
        kafka_pg, "org.apache.nifi.processors.kafka.pubsub.ConsumeKafka_2_6",
        "kafka-consume", 300, 250,
        properties={"bootstrap.servers": args.data_kafka,
                    "topic": args.kafka_topic,
                    "topic_type": "names",
                    "group.id": "baseline",
                    "auto.offset.reset": "earliest"})
    k_sink = nifi.create_processor(
        kafka_pg, "org.apache.nifi.processors.standard.PutFile", "kafka-sink",
        300, 450,
        properties={"Directory": "/tmp/atlas-baseline/kafka-out"},
        auto_term=["success", "failure"])
    nifi.create_connection(kafka_pg, proc_ref(k_source), proc_ref(k_publish), ["success"])
    nifi.create_connection(kafka_pg, proc_ref(k_consume), proc_ref(k_sink), ["success"])

    # ---- filepath-pg ----
    file_pg = nifi.create_pg(baseline_pg, "filepath-pg", 500, 50)
    f_get = nifi.create_processor(
        file_pg, "org.apache.nifi.processors.standard.GetFile", "file-get",
        50, 50,
        properties={"Input Directory": "/tmp/atlas-baseline/in",
                    "Keep Source File": "false"},
        schedule_period="10 sec")
    f_put = nifi.create_processor(
        file_pg, "org.apache.nifi.processors.standard.PutFile", "file-put",
        50, 250,
        properties={"Directory": "/tmp/atlas-baseline/out"},
        auto_term=["success", "failure"])
    nifi.create_connection(file_pg, proc_ref(f_get), proc_ref(f_put), ["success"])

    # ---- s3-pg ----
    s3_pg = nifi.create_pg(baseline_pg, "s3-pg", 950, 50)
    s3_source = nifi.create_processor(
        s3_pg, "org.apache.nifi.processors.standard.GenerateFlowFile", "s3-source",
        50, 50,
        properties={"File Size": "10 B", "generate-ff-custom-text": "s3-baseline",
                    "Batch Size": "1"},
        schedule_period="10 sec")
    s3_put = nifi.create_processor(
        s3_pg, "org.apache.nifi.processors.aws.s3.PutS3Object", "s3-put",
        50, 250,
        properties={"Bucket": args.bucket,
                    "Object Key": "${filename}",
                    "Access Key": args.minio_user,
                    "Secret Key": args.minio_password,
                    "Region": "us-east-1",
                    "Endpoint Override URL": args.minio_endpoint,
                    "Signer Override": "AWSS3V4SignerType"},
        auto_term=["success", "failure"])
    nifi.create_connection(s3_pg, proc_ref(s3_source), proc_ref(s3_put), ["success"])

    # ---- s2s-pg ----
    s2s_pg = nifi.create_pg(baseline_pg, "s2s-pg", 50, 700)
    s2s_source = nifi.create_processor(
        s2s_pg, "org.apache.nifi.processors.standard.GenerateFlowFile", "s2s-source",
        50, 50,
        properties={"File Size": "10 B", "generate-ff-custom-text": "s2s-baseline",
                    "Batch Size": "1"},
        schedule_period="10 sec")

    ids = {"root": root, "baseline_pg": baseline_pg,
           "kafka_pg": kafka_pg, "file_pg": file_pg,
           "s3_pg": s3_pg, "s2s_pg": s2s_pg,
           "baseline_input": baseline_input["id"]}

    if args.with_s2s:
        rpg = nifi.create_rpg(s2s_pg, args.nifi_url + "/nifi", 50, 250, transport="HTTP")
        rpg_id = rpg["id"]

        # Wait for the RPG to discover the peer's baseline-input port.
        remote_port = None
        for i in range(30):
            info = nifi.get(f"/nifi-api/remote-process-groups/{rpg_id}")
            for p in (info.get("component", {}).get("contents", {})
                      or {}).get("inputPorts", []) or []:
                if p.get("name") == "baseline-input" \
                        or p.get("targetId") == baseline_input["id"]:
                    remote_port = p
                    break
            if remote_port:
                break
            print(f"  waiting for RPG peer discovery… "
                  f"(auth={info.get('component',{}).get('authorizationIssues')})")
            time.sleep(2)

        if not remote_port:
            sys.exit("RPG did not discover peer input port — is NiFi running in "
                     "unsecure mode? See scripts/README.md for the config toggle.")

        nifi.create_connection(
            s2s_pg,
            proc_ref(s2s_source),
            {"id": remote_port["id"], "groupId": rpg_id,
             "type": "REMOTE_INPUT_PORT"},
            ["success"])
        nifi.set_rpg_state(rpg_id, "TRANSMITTING")
        ids.update({"rpg": rpg_id, "remote_input_port": remote_port["id"]})
        print(f"remote input port = {remote_port['id']}")
    else:
        # Without an RPG, the s2s-source processor has an unconnected success
        # relationship. Auto-terminate it so the PG can start.
        nifi.set_processor_config(s2s_source["component"]["id"],
                                  {"autoTerminatedRelationships": ["success"]})

    args.workdir.mkdir(parents=True, exist_ok=True)
    (args.workdir / "flow-ids.json").write_text(json.dumps(ids, indent=2))
    print(f"\nFlow built. IDs saved to {args.workdir / 'flow-ids.json'}.")


# ---------------------------------------------------------------------------
# Subcommand: create-rt.
# ---------------------------------------------------------------------------


def cmd_create_rt(args: argparse.Namespace) -> None:
    nifi = _mk_nifi(args)

    conf_dir = Path(f"/tmp/atlas-baseline-conf/{args.namespace}-{args.fs_path_level}")
    conf_dir.mkdir(parents=True, exist_ok=True)

    props = copy.deepcopy(RT_PROPS_TEMPLATE)
    props.update({
        "atlas-urls": args.atlas_url,
        "atlas-conf-dir": str(conf_dir),
        "atlas-default-cluster-name": args.namespace,
        "nifi-lineage-strategy": args.strategy,
        "atlas-nifi-url": args.nifi_url + "/nifi",
        "atlas-username": args.atlas_user,
        "atlas-password": args.atlas_password,
        "kafka-bootstrap-servers": args.atlas_kafka,
        "filesystem-paths-level": args.fs_path_level,
    })

    body = {
        "revision": {"version": 0, "clientId": CLIENT_ID},
        "component": {
            "type": "org.apache.nifi.atlas.reporting.ReportLineageToAtlas",
            "name": f"ReportLineageToAtlas-{args.namespace}",
            "properties": props,
            "schedulingPeriod": args.schedule,
            "schedulingStrategy": "TIMER_DRIVEN",
        },
    }
    rt = nifi.post("/nifi-api/controller/reporting-tasks", json=body)
    rt_id = rt["id"]

    info = nifi.get(f"/nifi-api/reporting-tasks/{rt_id}")
    comp = info["component"]
    print(f"Created ReportLineageToAtlas: {rt_id}")
    print(f"  namespace: {args.namespace}   fs-path-level: {args.fs_path_level}")
    print(f"  validationStatus: {comp.get('validationStatus')}")
    if comp.get("validationErrors"):
        print(f"  validationErrors: {comp['validationErrors']}")


# ---------------------------------------------------------------------------
# Subcommand: run.
# ---------------------------------------------------------------------------


def _find_pg(nifi: Nifi, parent: str, name: str) -> str | None:
    flow = nifi.get(f"/nifi-api/flow/process-groups/{parent}")["processGroupFlow"]["flow"]
    for pg in flow.get("processGroups", []):
        if pg["component"]["name"] == name:
            return pg["id"]
    return None


def _find_reporting_task(nifi: Nifi, name_contains: str = "ReportLineageToAtlas"):
    for rt in nifi.get("/nifi-api/flow/reporting-tasks").get("reportingTasks", []):
        if name_contains in rt["component"]["name"]:
            return rt
    return None


ATLAS_TYPES_TO_DUMP = [
    "kafka_topic",
    "fs_path",
    "aws_s3_v2_directory",
    "aws_s3_v2_bucket",
    "aws_s3_pseudo_dir",
    "aws_s3_bucket",
    "nifi_flow",
    "nifi_flow_path",
    "nifi_queue",
    "nifi_input_port",
    "nifi_output_port",
    "nifi_data",
]


def _dump_atlas(atlas_url: str, auth: tuple[str, str]) -> tuple[dict, dict]:
    basic: dict = {}
    for t in ATLAS_TYPES_TO_DUMP:
        r = requests.get(f"{atlas_url}/api/atlas/v2/search/basic",
                         params={"typeName": t, "limit": 200},
                         auth=auth)
        try:
            basic[t] = r.json()
        except Exception:
            basic[t] = {"error": r.text[:500]}
        n = len(basic[t].get("entities") or [])
        print(f"  atlas {t}: {n} entities")

    detailed: dict = {}
    for t, res in basic.items():
        detailed[t] = []
        for e in res.get("entities") or []:
            guid = e.get("guid")
            r = requests.get(f"{atlas_url}/api/atlas/v2/entity/guid/{guid}",
                             auth=auth)
            try:
                detailed[t].append(r.json())
            except Exception:
                detailed[t].append({"error": r.text[:500], "guid": guid})

    return basic, detailed


def cmd_run(args: argparse.Namespace) -> None:
    nifi = _mk_nifi(args)
    out_dir = args.workdir / f"run-{args.namespace}-{args.fs_path_level}"
    out_dir.mkdir(parents=True, exist_ok=True)

    root = nifi.root_id()

    # Start flow, recorder, RT.
    print("Starting root PG (all processors) …")
    nifi.set_pg_state(root, "RUNNING")

    print("Starting ATLAS_HOOK recorder …")
    _docker("rm", "-f", "atlas-hook-recorder", quiet=True, check=False)
    _docker(
        "run", "-d", "--name", "atlas-hook-recorder", "--network", "host",
        "apache/kafka:4.1.0",
        "/opt/kafka/bin/kafka-console-consumer.sh",
        "--bootstrap-server", args.atlas_kafka,
        "--topic", "ATLAS_HOOK", "--from-beginning",
    )

    rt = _find_reporting_task(nifi)
    if not rt:
        sys.exit("ReportLineageToAtlas reporting task not found — run 'create-rt' first.")
    print(f"Starting reporting task {rt['id']} …")
    nifi.put(f"/nifi-api/reporting-tasks/{rt['id']}/run-status",
             json={"revision": {"version": rt["revision"]["version"],
                                "clientId": CLIENT_ID},
                   "state": "RUNNING",
                   "disconnectedNodeAcknowledged": False})

    print(f"Waiting {args.wait_secs}s for provenance events + RT ticks …")
    time.sleep(args.wait_secs)

    print("Stopping reporting task + flow …")
    rt2 = _find_reporting_task(nifi)
    if rt2:
        try:
            nifi.put(f"/nifi-api/reporting-tasks/{rt2['id']}/run-status",
                     json={"revision": {"version": rt2["revision"]["version"],
                                        "clientId": CLIENT_ID},
                           "state": "STOPPED",
                           "disconnectedNodeAcknowledged": False})
        except Exception as e:
            print(f"  warn: stop rt: {e}")
    try:
        nifi.set_pg_state(root, "STOPPED")
    except Exception as e:
        print(f"  warn: stop root: {e}")

    print("Snapshotting Atlas …")
    basic, detailed = _dump_atlas(args.atlas_url,
                                  (args.atlas_user, args.atlas_password))
    (out_dir / "atlas-basic.json").write_text(json.dumps(basic, indent=2))
    (out_dir / "atlas-detailed.json").write_text(json.dumps(detailed, indent=2))

    # Pull hook messages.
    time.sleep(1)
    with open(out_dir / "atlas-hook.jsonl", "w") as f:
        subprocess.run(["docker", "logs", "atlas-hook-recorder"],
                       stdout=f, stderr=subprocess.STDOUT)
    _docker("rm", "-f", "atlas-hook-recorder", quiet=True, check=False)

    # Auto-generated atlas-application.properties (best-effort — path is deterministic).
    conf_src = Path(f"/tmp/atlas-baseline-conf/{args.namespace}-{args.fs_path_level}/"
                    "atlas-application.properties")
    if conf_src.exists():
        shutil.copy(conf_src, out_dir / "atlas-application.properties")

    print(f"\nDone. Output in {out_dir}")


# ---------------------------------------------------------------------------
# Subcommand: persist.
# ---------------------------------------------------------------------------


VOLATILE_KEYS = {
    "guid", "createTime", "updateTime", "createdBy", "updatedBy", "version",
    "labels", "customAttributes", "provenanceType", "lastAccessTime",
    "homeId", "isIncomplete", "meanings", "status",
}


def _strip_volatile(obj: Any) -> Any:
    if isinstance(obj, dict):
        return {k: _strip_volatile(v) for k, v in obj.items() if k not in VOLATILE_KEYS}
    if isinstance(obj, list):
        return [_strip_volatile(v) for v in obj]
    return obj


def _filter_basic(basic: dict, ns: str) -> dict:
    out = {}
    for t, res in basic.items():
        keep = [e for e in (res.get("entities") or [])
                if str(e.get("attributes", {}).get("qualifiedName", "")
                       ).endswith(f"@{ns}")]
        out[t] = {"query": res.get("query"), "entities": keep}
    return out


def _filter_detailed(detailed: dict, ns: str) -> dict:
    out = {}
    for t, entries in detailed.items():
        keep = []
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            entity = entry.get("entity") or {}
            qn = str(entity.get("attributes", {}).get("qualifiedName", ""))
            if qn.endswith(f"@{ns}"):
                keep.append(entry)
        out[t] = keep
    return out


def _summarize(basic: dict, ns: str) -> dict:
    result = {"namespace": ns, "counts": {}, "flow_paths": [], "datasets": {}}
    for t, res in basic.items():
        ents = res.get("entities") or []
        if not ents:
            continue
        result["counts"][t] = len(ents)
        for e in ents:
            attrs = e.get("attributes", {})
            qn = attrs.get("qualifiedName", "")
            name = attrs.get("name", "")
            if t == "nifi_flow_path":
                result["flow_paths"].append({"name": name, "qualifiedName": qn})
            elif t in ("kafka_topic", "fs_path", "aws_s3_v2_directory",
                       "nifi_data", "nifi_input_port", "nifi_queue"):
                result["datasets"].setdefault(t, []).append({
                    "name": name, "qualifiedName": qn,
                    "path": attrs.get("path"),
                    "uri": attrs.get("uri"),
                })
    return result


def _write_summary_md(summary: dict, path: Path) -> None:
    lines = [f"# Baseline capture — namespace `{summary['namespace']}`\n",
             "## Entity counts\n"]
    for t, n in sorted(summary["counts"].items()):
        lines.append(f"- `{t}`: {n}")
    lines.append("\n## Flow paths\n")
    for fp in sorted(summary["flow_paths"], key=lambda x: x["qualifiedName"]):
        lines.append(f"- `{fp['name']}` — qn `{fp['qualifiedName']}`")
    lines.append("\n## Datasets\n")
    for t, ds in sorted(summary["datasets"].items()):
        lines.append(f"### `{t}`")
        for d in sorted(ds, key=lambda x: x["qualifiedName"]):
            extra = ""
            if d.get("path"):
                extra += f" path=`{d['path']}`"
            if d.get("uri"):
                extra += f" uri=`{d['uri']}`"
            lines.append(f"- qn `{d['qualifiedName']}` name=`{d['name']}`{extra}")
        lines.append("")
    path.write_text("\n".join(lines) + "\n")


def _parse_hook_line(line: str):
    line = line.strip()
    if not line or not line.startswith("{"):
        return None
    try:
        return json.loads(line)
    except Exception:
        return None


def _persist_one(src_dir: Path, dest_dir: Path, namespace: str) -> dict:
    dest_dir.mkdir(parents=True, exist_ok=True)

    basic = json.loads((src_dir / "atlas-basic.json").read_text())
    stripped = _strip_volatile(_filter_basic(basic, namespace))
    (dest_dir / "atlas-basic.json").write_text(
        json.dumps(stripped, indent=2, sort_keys=True))

    detailed = json.loads((src_dir / "atlas-detailed.json").read_text())
    stripped_detailed = _strip_volatile(_filter_detailed(detailed, namespace))
    (dest_dir / "atlas-detailed.json").write_text(
        json.dumps(stripped_detailed, indent=2, sort_keys=True))

    hook_src = src_dir / "atlas-hook.jsonl"
    if hook_src.exists():
        shutil.copy(hook_src, dest_dir / "atlas-hook.jsonl")
        parsed = []
        with open(hook_src) as f:
            for line in f:
                j = _parse_hook_line(line)
                if j is not None:
                    parsed.append(j)
        (dest_dir / "hook-parsed.json").write_text(
            json.dumps(parsed, indent=2, sort_keys=True))

    conf_src = src_dir / "atlas-application.properties"
    if conf_src.exists():
        shutil.copy(conf_src, dest_dir / "atlas-application.properties")

    summary = _summarize(_filter_basic(basic, namespace), namespace)
    (dest_dir / "summary.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True))
    _write_summary_md(summary, dest_dir / "summary.md")

    return summary


def cmd_persist(args: argparse.Namespace) -> None:
    src_dir = args.workdir / f"run-{args.namespace}-{args.fs_path_level}"
    if not src_dir.exists():
        sys.exit(f"No capture found at {src_dir} — run 'run' first.")

    dest_root = Path(args.dest) if args.dest else DEFAULT_FIXTURE_DIR
    preset = args.preset or _default_preset(args.fs_path_level, args.with_s2s)
    dest = dest_root / preset

    summary = _persist_one(src_dir, dest, args.namespace)
    print(f"Wrote fixtures to {dest}")
    print(f"  counts: {summary['counts']}")


def _default_preset(fs_level: str, with_s2s: bool) -> str:
    if with_s2s:
        return f"{fs_level.lower()}-mode-with-s2s"
    return f"{fs_level.lower()}-mode"


# ---------------------------------------------------------------------------
# Subcommand: all.
# ---------------------------------------------------------------------------


def cmd_all(args: argparse.Namespace) -> None:
    print("\n=== containers-up ===")
    cmd_containers_up(args)
    print("\n=== wipe ===")
    cmd_wipe(args)
    print("\n=== build-flow ===")
    cmd_build_flow(args)
    print("\n=== create-rt ===")
    cmd_create_rt(args)
    print("\n=== run ===")
    cmd_run(args)
    print("\n=== persist ===")
    cmd_persist(args)
    print("\n=== all done ===")


# ---------------------------------------------------------------------------
# CLI plumbing.
# ---------------------------------------------------------------------------


def _mk_nifi(args: argparse.Namespace) -> Nifi:
    return Nifi(args.nifi_url, args.nifi_user, args.nifi_password,
                verify_tls=args.nifi_verify_tls)


def _add_global_args(p: argparse.ArgumentParser) -> None:
    p.add_argument("--nifi-url",
                   default=os.environ.get("NIFI_URL", "http://nifi1:8080"))
    p.add_argument("--nifi-user", default=os.environ.get("NIFI_USER"))
    p.add_argument("--nifi-password", default=os.environ.get("NIFI_PASSWORD"))
    p.add_argument("--nifi-verify-tls", action="store_true")
    p.add_argument("--atlas-url", default="http://localhost:21000")
    p.add_argument("--atlas-user", default="admin")
    p.add_argument("--atlas-password", default="admin")
    p.add_argument("--atlas-kafka", default="localhost:9092",
                   help="Bootstrap servers for the ATLAS_HOOK Kafka (host:port).")
    p.add_argument("--data-kafka", default="localhost:19092",
                   help="Bootstrap servers the flow's Publish/ConsumeKafka use.")
    p.add_argument("--kafka-topic", default="baseline-kafka-topic")
    p.add_argument("--minio-endpoint", default="http://localhost:19000")
    p.add_argument("--minio-user", default="minioadmin")
    p.add_argument("--minio-password", default="minioadmin")
    p.add_argument("--bucket", default="baseline-bucket")
    p.add_argument("--workdir", type=Path, default=DEFAULT_WORKDIR)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Baseline NiFi's ReportLineageToAtlas output for the C++ port.")
    sub = parser.add_subparsers(dest="cmd", required=True)

    for name, fn, extra in [
        ("containers-up", cmd_containers_up, []),
        ("containers-down", cmd_containers_down, []),
        ("wipe", cmd_wipe, []),
        ("delete-rt", cmd_delete_rt, []),
    ]:
        p = sub.add_parser(name)
        _add_global_args(p)
        for x in extra:
            x(p)
        p.set_defaults(func=fn)

    # build-flow
    p = sub.add_parser("build-flow")
    _add_global_args(p)
    p.add_argument("--with-s2s", action="store_true",
                   help="Also wire the S2S self-loop (requires unsecure NiFi).")
    p.set_defaults(func=cmd_build_flow)

    # create-rt
    def add_rt_args(p):
        p.add_argument("--namespace", default="baseline")
        p.add_argument("--fs-path-level", choices=["FILE", "DIRECTORY"],
                       default="FILE")
        p.add_argument("--strategy", choices=["SimplePath", "CompletePath"],
                       default="SimplePath")
        p.add_argument("--schedule", default="30 sec")

    p = sub.add_parser("create-rt")
    _add_global_args(p)
    add_rt_args(p)
    p.set_defaults(func=cmd_create_rt)

    # run
    p = sub.add_parser("run")
    _add_global_args(p)
    p.add_argument("--namespace", default="baseline")
    p.add_argument("--fs-path-level", choices=["FILE", "DIRECTORY"], default="FILE")
    p.add_argument("--wait-secs", type=int, default=120)
    p.set_defaults(func=cmd_run)

    # persist
    p = sub.add_parser("persist")
    _add_global_args(p)
    p.add_argument("--namespace", default="baseline")
    p.add_argument("--fs-path-level", choices=["FILE", "DIRECTORY"], default="FILE")
    p.add_argument("--with-s2s", action="store_true",
                   help="Chooses the '<mode>-mode-with-s2s' preset by default.")
    p.add_argument("--dest",
                   help=f"Fixture root (default: {DEFAULT_FIXTURE_DIR}).")
    p.add_argument("--preset",
                   help="Subdir under --dest to write (default derived from "
                        "--fs-path-level and --with-s2s).")
    p.set_defaults(func=cmd_persist)

    # all
    p = sub.add_parser("all", help="Run the full pipeline end-to-end.")
    _add_global_args(p)
    add_rt_args(p)
    p.add_argument("--with-s2s", action="store_true")
    p.add_argument("--wait-secs", type=int, default=120)
    p.add_argument("--dest")
    p.add_argument("--preset")
    p.set_defaults(func=cmd_all)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
