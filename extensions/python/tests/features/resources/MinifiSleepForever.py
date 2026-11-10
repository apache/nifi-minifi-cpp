# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
# SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
import time


def describe(processor):
    processor.setDescription("Sleep forever processor.")


def onTrigger(context, session):
    log.info("Sleeping forever")
    while True:
        time.sleep(1)
