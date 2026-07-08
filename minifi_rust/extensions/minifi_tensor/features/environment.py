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

import os
import urllib.request

from minifi_behave.core.hooks import (
    add_extension_to_minifi_container,
    common_after_scenario,
    common_before_scenario,
)

# Model / label / image assets fetched on first use. All hosted on
# public buckets or the sonos/tract repo
REMOTE_ASSETS: dict[str, str] = {
    # ImageNet MobileNetV2 classifier (~14 MB) — the reference model used by tract's unit tests
    "mobilenetv2-7.onnx": "https://s3.amazonaws.com/tract-ci-builds/tests/mobilenetv2-7.onnx",
    # 1000-class ImageNet labels (line N = class N; line 0 is "dummy")
    "imagenet_slim_labels.txt": "https://raw.githubusercontent.com/sonos/tract/main/examples/"
    "onnx-mobilenet-v2/imagenet_slim_labels.txt",
    # Same test image tract's example uses. MobileNetV2 confidently
    # classifies this as "military uniform".
    "grace_hopper.jpg": "https://raw.githubusercontent.com/sonos/tract/main/examples/"
    "onnx-mobilenet-v2/grace_hopper.jpg",
    # UltraFace RFB-320 (~1.2 MB): 2-output SSD-style detector matching the
    # existing FilterBoundingBoxes defaults (Xyxy boxes, class 0 = background,
    # softmax over 2 classes: background/face). 320x240 RGB, mean=127, std=128.
    "version-RFB-320.onnx": "https://github.com/onnx/models/raw/refs/heads/main/validated/vision/"
    "body_analysis/ultraface/models/version-RFB-320.onnx",
}


def _ensure_asset(cache_dir: str, filename: str) -> str:
    dest = os.path.join(cache_dir, filename)
    if os.path.exists(dest):
        return dest
    url = REMOTE_ASSETS[filename]
    os.makedirs(cache_dir, exist_ok=True)
    tmp = dest + ".part"
    print(f"[minifi_tensor tests] fetching {filename} from {url}")
    urllib.request.urlretrieve(url, tmp)
    os.replace(tmp, dest)
    return dest


def before_all(context):
    dir_path = os.path.dirname(os.path.realpath(__file__))
    build_path = os.path.normpath(os.path.join(dir_path, "../../../target/release/"))
    deps_build_path = os.path.normpath(os.path.join(dir_path, "../../../target/release/deps/"))
    add_extension_to_minifi_container("minifi_tensor", [build_path, deps_build_path], context)

    context.tensor_resource_dir = os.path.join(dir_path, "resources")
    os.makedirs(context.tensor_resource_dir, exist_ok=True)
    for name in REMOTE_ASSETS:
        _ensure_asset(context.tensor_resource_dir, name)


def before_scenario(context, scenario):
    context.minifi_container_image = "apacheminificpp:minifi_tensor"
    common_before_scenario(context, scenario)
    context.resource_dir = context.tensor_resource_dir


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
