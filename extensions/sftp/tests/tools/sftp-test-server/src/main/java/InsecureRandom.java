/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

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