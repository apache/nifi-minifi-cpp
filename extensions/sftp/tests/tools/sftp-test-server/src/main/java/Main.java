// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

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
