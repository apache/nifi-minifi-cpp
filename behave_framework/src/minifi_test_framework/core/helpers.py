from __future__ import annotations

import logging
import time
from typing import Callable

from minifi_test_framework.core.minifi_test_context import MinifiTestContext


def log_due_to_failure(context: MinifiTestContext | None):
    if context is not None:
        for container in context.containers:
            container.log_app_output()
        context.minifi_container.log_app_output()


def wait_for_condition(condition: Callable[[], bool], timeout_seconds: float, bail_condition: Callable[[], bool], context: MinifiTestContext | None) -> bool:
    if bail_condition():
        logging.warning("Bail condition evaluated to 'True', aborting wait.")
        log_due_to_failure(context)
        return False
    start_time = time.monotonic()
    try:
        while time.monotonic() - start_time < timeout_seconds:
            if condition():
                return True
            if bail_condition():
                logging.warning("Bail condition evaluated to 'True', aborting wait.")
                log_due_to_failure(context)
                return False
            remaining_time = timeout_seconds - (time.monotonic() - start_time)
            sleep_time = min(timeout_seconds / 10, remaining_time)
            if sleep_time > 0:
                time.sleep(sleep_time)
    except (Exception,):
        logging.warning("Exception while waiting for condition")
        log_due_to_failure(context)
        return False
    logging.warning("Timed out after %d seconds while waiting for condition", timeout_seconds)
    log_due_to_failure(context)
    return False
