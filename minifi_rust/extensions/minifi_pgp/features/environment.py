import os
from typing import List

from minifi_behave.containers.docker_image_builder import DockerImageBuilder
from minifi_behave.core.hooks import common_after_scenario
from minifi_behave.core.hooks import common_before_scenario, get_minifi_container_image
from minifi_behave.core.minifi_test_context import MinifiTestContext


def add_extension_to_minifi_container(
    extension_name: str, possible_paths: List[str], context: MinifiTestContext
):
    new_container_name = f"apacheminificpp:{extension_name}"
    is_windows = os.name == "nt"
    if is_windows:
        lib_filename = f"{extension_name}.dll"
        container_extension_dir = (
            "C:/Program Files/ApacheNiFiMiNiFi/nifi-minifi-cpp/extensions"
        )
    else:
        lib_filename = f"lib{extension_name}.so"
        container_extension_dir = "/opt/minifi/minifi-current/extensions/"

    host_path = None
    for path in possible_paths:
        if os.path.exists(os.path.join(path, lib_filename)):
            host_path = os.path.join(path, lib_filename)
            break

    assert host_path is not None, (
        f"Could not find {lib_filename} in {[p for p in possible_paths]}"
    )

    with open(host_path, "rb") as f:
        lib_content = f.read()

    base_img = get_minifi_container_image()

    if is_windows:
        dockerfile = f"""
FROM {base_img}
COPY ["{lib_filename}", "{container_extension_dir}/{lib_filename}"]
"""
    else:
        dockerfile = f"""
FROM {base_img}
COPY --chown=minificpp:minificpp {lib_filename} {container_extension_dir}
RUN chmod 755 {container_extension_dir}{lib_filename}
"""

    builder = DockerImageBuilder(
        image_tag=new_container_name,
        dockerfile_content=dockerfile,
        files_on_context={lib_filename: lib_content},
    )

    builder.build()
    return new_container_name


def before_all(context):
    dir_path = os.path.dirname(os.path.realpath(__file__))
    build_path = os.path.normpath(os.path.join(dir_path, "../../../target/release/"))
    add_extension_to_minifi_container("minifi_pgp", [build_path], context)


def before_scenario(context, scenario):
    context.minifi_container_image = "apacheminificpp:minifi_pgp"
    common_before_scenario(context, scenario)


def after_scenario(context, scenario):
    common_after_scenario(context, scenario)
