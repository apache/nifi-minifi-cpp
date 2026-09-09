# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

import hashlib
import os
import shutil
import ssl
import urllib.request

import certifi

from minifi_behave.core.hooks import (
    add_extension_to_minifi_container,
    common_after_scenario,
    common_before_scenario,
)


_SSL_CONTEXT = ssl.create_default_context(cafile=certifi.where())


class RemoteAsset:
    def __init__(self, url: str, sha256: str):
        self.url = url
        self.sha256 = sha256

    def acquire(self, cache_dir: str, filename: str) -> str:
        dest = os.path.join(cache_dir, filename)
        if os.path.exists(dest) and self._verify(dest):
            return dest
        os.makedirs(cache_dir, exist_ok=True)
        tmp = dest + ".part"
        print(f"[minifi_tensor tests] fetching {filename} from {self.url}")
        with urllib.request.urlopen(self.url, context=_SSL_CONTEXT) as response, open(tmp, "wb") as out:
            shutil.copyfileobj(response, out)
        if not self._verify(tmp):
            actual = self._digest(tmp)
            os.remove(tmp)
            raise RuntimeError(
                f"sha256 mismatch for {filename}: expected {self.sha256}, got {actual}"
            )
        os.replace(tmp, dest)
        return dest

    def _verify(self, path: str) -> bool:
        return self._digest(path) == self.sha256

    @staticmethod
    def _digest(path: str) -> str:
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
        return h.hexdigest()


# Model / label / image assets fetched on first use. All hosted on
# public buckets or the sonos/tract repo
REMOTE_ASSETS: dict[str, RemoteAsset] = {
    # ImageNet MobileNetV2 classifier (~14 MB) — the reference model used by tract's unit tests
    "mobilenetv2-7.onnx": RemoteAsset(
        "https://s3.amazonaws.com/tract-ci-builds/tests/mobilenetv2-7.onnx",
        "c1c513582d56afceff8516c73804e484c81c6a830712ab6d682253f4a3cd042f",
    ),
    # 1000-class ImageNet labels (line N = class N; line 0 is "dummy")
    "imagenet_slim_labels.txt": RemoteAsset(
        "https://raw.githubusercontent.com/sonos/tract/main/examples/"
        "onnx-mobilenet-v2/imagenet_slim_labels.txt",
        "e8d2cef25bb7b3c8c6923ad3c463b47de8b8535cadf4bd62a2ca2532c587eb9f",
    ),
    # Same test image tract's example uses. MobileNetV2 confidently
    # classifies this as "military uniform".
    "grace_hopper.jpg": RemoteAsset(
        "https://raw.githubusercontent.com/sonos/tract/main/examples/"
        "onnx-mobilenet-v2/grace_hopper.jpg",
        "e1f57e98cf38076c0f9a058d74ffddf90f20453e436033784606b63c8ed2e49a",
    ),
    # UltraFace RFB-320 (~1.2 MB): 2-output SSD-style detector matching the
    # existing FilterBoundingBoxes defaults (Xyxy boxes, class 0 = background,
    # softmax over 2 classes: background/face). 320x240 RGB, mean=127, std=128.
    "version-RFB-320.onnx": RemoteAsset(
        "https://github.com/onnx/models/raw/refs/heads/main/validated/vision/"
        "body_analysis/ultraface/models/version-RFB-320.onnx",
        "34cd7e60aeff28744c657de7a3dc64e872d506741de66987f3426f2b79f88017",
    ),
}


def before_all(context):
    dir_path = os.path.dirname(os.path.realpath(__file__))
    build_path = os.path.normpath(os.path.join(dir_path, "../../../target/release/"))
    deps_build_path = os.path.normpath(os.path.join(dir_path, "../../../target/release/deps/"))
    add_extension_to_minifi_container("minifi_tensor", [build_path, deps_build_path], context)

    context.tensor_resource_dir = os.path.join(dir_path, "resources")
    os.makedirs(context.tensor_resource_dir, exist_ok=True)
    for name, asset in REMOTE_ASSETS.items():
        asset.acquire(context.tensor_resource_dir, name)


def before_scenario(context, scenario):
    context.minifi_container_image = "apacheminificpp:minifi_tensor"
    common_before_scenario(context, scenario)
    context.resource_dir = context.tensor_resource_dir


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
