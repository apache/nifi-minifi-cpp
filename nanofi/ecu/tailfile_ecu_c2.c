/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
    *
    *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef _WIN32
#include <unistd.h>
#endif
#include <stdio.h>
#include <signal.h>
#include <ecu_api/ecuapi.h>
#include <c2_api/c2api.h>
#include <processors/file_input.h>
#include <core/log.h>

volatile sig_atomic_t stop_ecu = 0;

void ecu_signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        stop_ecu = 1;
    }
}

void setup_ecu_signal_action() {
#ifdef _WIN32
    signal(SIGINT, ecu_signal_handler);
    signal(SIGTERM, ecu_signal_handler);
#else
    struct sigaction action;
    memset(&action, 0, sizeof(sigaction));
    action.sa_handler = ecu_signal_handler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
#endif
}

int main(int argc, char ** argv) {

    if (argc < 3) {
        printf("usage ./tailfile_ecu_c2 <c2 host name/ip> <c2 port>\n");
        return 1;
    }
    set_log_level(trace);
    setup_ecu_signal_action();

    io_context_t * io = create_io_context();
    input_context_t * ip = create_input(TAILFILE);
    output_context_t * op = create_output(SITE2SITE);
    ecu_context_t * ecu = create_ecu(io, "tailfile_s2s", ip, op);
    if (!ecu) {
        free_input(ip);
        free_output(ip);
        destroy_io_context(io);
        logc(err, "%s", "ecu creation failed");
        return 1;
    }

    c2context_t * c2 = create_c2_agent(argv[1], argv[2]);

    set_start_callback(c2, on_start);
    set_stop_callback(c2, on_stop);
    set_clear_callback(c2, on_clear);
    set_update_callback(c2, on_update);

    register_ecu(ecu, c2);

    if (start_c2_agent(c2) < 0) {
        logc(err, "%s", "Could not start c2 agent");
        unregister_ecu(ecu, c2);
        destroy_c2_context(c2);
        destroy_io_context(io);
        return 1;
    }

    while (!stop_ecu) {
#ifndef WIN32
        sleep(1);
#else
        Sleep(1000);
#endif
    }

    destroy_c2_context(c2);
    destroy_io_context(io);
    logc(info, "%s", "destroyed c2 context");
}
