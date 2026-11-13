// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import org.apache.sshd.common.random.Random;
import org.apache.sshd.common.random.AbstractRandomFactory;

public class InsecureRandomFactory extends AbstractRandomFactory {
    public static final String NAME = "default";
    public static final InsecureRandomFactory INSTANCE = new InsecureRandomFactory();

    public InsecureRandomFactory() {
        super(NAME);
    }

    @Override
    public boolean isSupported() {
        return true;
    }

    @Override
    public Random create() {
        return new InsecureRandom();
    }
}
