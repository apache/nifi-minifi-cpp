// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

import java.util.Random;

import org.apache.sshd.common.random.AbstractRandom;

public class InsecureRandom extends AbstractRandom {
    public static final String NAME = "Insecure";

    private byte[] tmp = new byte[16];
    private final Random random = new Random();

    public InsecureRandom() {
        super();
    }

    @Override
    public String getName() {
        return NAME;
    }

    @Override
    public synchronized void fill(byte[] foo, int start, int len) {
        if ((start == 0) && (len == foo.length)) {
            random.nextBytes(foo);
        } else {
            if (len > tmp.length) {
                tmp = new byte[len];
            }
            random.nextBytes(tmp);
            System.arraycopy(tmp, 0, foo, start, len);
        }
    }

    @Override
    public synchronized int random(int n) {
        return random.nextInt(n);
    }
}
