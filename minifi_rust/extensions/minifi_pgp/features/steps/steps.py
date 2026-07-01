import os
from pathlib import Path

import humanfriendly
from behave import step, then

from minifi_behave.steps import checking_steps  # noqa: F401
from minifi_behave.steps import configuration_steps  # noqa: F401
from minifi_behave.steps import core_steps  # noqa: F401
from minifi_behave.steps import flow_building_steps  # noqa: F401
from minifi_behave.core.helpers import wait_for_condition
from minifi_behave.core.minifi_test_context import MinifiTestContext
from minifi_behave.minifi.controller_service import ControllerService
from minifi_behave.minifi.processor import Processor


@step("an EncryptContentPGP processor with a PGPPublicKeyService is set up")
def step_encrypt_content_with_service(context: MinifiTestContext):
    dir_path = os.path.dirname(os.path.realpath(__file__))

    public_key_service = ControllerService(
        class_name="PGPPublicKeyService", service_name="my_public_keys"
    )
    alice_public_key = Path(f"{dir_path}/../../test_keys/keyring.asc").read_text()
    public_key_service.add_property("Keyring", alice_public_key)
    context.get_or_create_default_minifi_container().flow_definition.controller_services.append(
        public_key_service
    )

    processor = Processor("EncryptContentPGP", "EncryptContentPGP")
    processor.add_property("Public Key Service", "my_public_keys")
    context.get_or_create_default_minifi_container().flow_definition.processors.append(
        processor
    )


@step(
    "a DecryptContentPGP processor named DecryptAlice with a PGPPrivateKeyService is set up for Alice"
)
def step_decrypt_content_for_alice(context: MinifiTestContext):
    dir_path = os.path.dirname(os.path.realpath(__file__))

    private_key_service = ControllerService(
        class_name="PGPPrivateKeyService", service_name="alice_private_key"
    )
    alice_private_key = Path(
        f"{dir_path}/../../test_keys/alice_private.asc"
    ).read_text()
    private_key_service.add_property("Key", alice_private_key)
    private_key_service.add_property("Key Passphrase", "whiterabbit")
    context.get_or_create_default_minifi_container().flow_definition.controller_services.append(
        private_key_service
    )

    processor = Processor("DecryptContentPGP", "DecryptAlice")
    processor.add_property("Private Key Service", "alice_private_key")
    context.get_or_create_default_minifi_container().flow_definition.processors.append(
        processor
    )


@step(
    "a DecryptContentPGP processor named DecryptBob with a PGPPrivateKeyService is set up for Bob"
)
def step_decrypt_content_for_bob(context: MinifiTestContext):
    dir_path = os.path.dirname(os.path.realpath(__file__))

    private_key_service = ControllerService(
        class_name="PGPPrivateKeyService", service_name="bob_private_key"
    )
    bob_private_key = Path(f"{dir_path}/../../test_keys/bob_private.asc").read_text()
    private_key_service.add_property("Key", bob_private_key)
    context.get_or_create_default_minifi_container().flow_definition.controller_services.append(
        private_key_service
    )

    processor = Processor("DecryptContentPGP", "DecryptBob")
    processor.add_property("Private Key Service", "bob_private_key")
    context.get_or_create_default_minifi_container().flow_definition.processors.append(
        processor
    )


@then(
    'an encrypted armored pgp file is placed in the "{directory}" directory in less than {duration}'
)
def then_armored_pgp_file_in_dir(
    context: MinifiTestContext, directory: str, duration: str
):
    duration_seconds = humanfriendly.parse_timespan(duration)
    assert wait_for_condition(
        condition=lambda: (
            context.get_or_create_default_minifi_container().directory_contains_file_with_regex(
                directory, "-----BEGIN PGP MESSAGE-----"
            )
        ),
        timeout_seconds=duration_seconds,
        bail_condition=lambda: False,
        context=context,
    )
