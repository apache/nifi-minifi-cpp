import time


def onTrigger(context, session):
    log.info("Sleeping forever")
    while True:
        time.sleep(1)
