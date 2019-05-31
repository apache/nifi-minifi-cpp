/**
 *
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

import org.apache.commons.cli.*;
import java.io.FileWriter;
import java.nio.file.Paths;

public class Main {
    public static void main(String[] args) {
        Options options = new Options();

        Option workingDirectoryArg = new Option("w", "working-directory", true, "working directory");
        workingDirectoryArg.setRequired(true);
        options.addOption(workingDirectoryArg);

        Option hostKeyArg = new Option("k", "host-key", true, "path to the host key file");
        hostKeyArg.setRequired(false);
        options.addOption(hostKeyArg);

        CommandLineParser parser = new DefaultParser();
        HelpFormatter formatter = new HelpFormatter();
        CommandLine cmd = null;

        try {
            cmd = parser.parse(options, args);
        } catch (ParseException e) {
            System.err.println(e.getMessage());
            formatter.printHelp("SFTPTestServer", options);

            System.exit(1);
        }

        String workingDirectory = cmd.getOptionValue("working-directory");

        SFTPTestServer server = new SFTPTestServer();
        server.setVirtualFileSystemPath(Paths.get(workingDirectory, "vfs").toString());

        if (cmd.hasOption("host-key")) {
            server.setHostKeyFile(Paths.get(cmd.getOptionValue("host-key")));
        }
        try {
            server.startServer();
            FileWriter portFile = new FileWriter(Paths.get(workingDirectory, "port.txt").toFile());
            portFile.write(Integer.toString(server.getSSHPort()));
            portFile.close();
            Thread.sleep(Long.MAX_VALUE);
        } catch (Exception e) {
            e.printStackTrace();
            System.exit(2);
        }
    }
}
