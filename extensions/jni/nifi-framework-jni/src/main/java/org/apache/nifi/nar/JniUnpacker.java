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
package org.apache.nifi.nar;

import org.apache.nifi.bundle.BundleCoordinate;
import org.apache.nifi.util.FileUtils;
import org.apache.nifi.util.StringUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.*;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.*;
import java.util.concurrent.TimeUnit;
import java.util.jar.Attributes;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.jar.Manifest;

/**
 *  Should change NarPacker so that we can depend on it a little more for our purposes.
 */
public class JniUnpacker {

    private static final Logger logger = LoggerFactory.getLogger(JniUnpacker.class);
    private static String HASH_FILENAME = "nar-md5sum";

    private static final Map<String,File> narMaps = new HashMap<>();
    private static final Map<String,String> narDependencies = new HashMap<>();

    private static final FileFilter NAR_FILTER = new FileFilter() {
        @Override
        public boolean accept(File pathname) {
            final String nameToTest = pathname.getName().toLowerCase();
            return nameToTest.endsWith(".nar") && pathname.isFile();
        }
    };


    public static Optional<String> getDependency(final String narId){
        return Optional.of( narDependencies.get(narId) );
    }

    public static Optional<File> getDirectory(final String narId){
        return Optional.of( narMaps.get(narId) );
    }

    public static void unpackNars(final File narFilesDir, final File unpackDirectory, List<File> paths ) throws IOException {
        final List<Path> narLibraryDirs = new ArrayList<>();
        final Map<File, BundleCoordinate> unpackedNars = new HashMap<>();

        final Path narFilesPath = narFilesDir.toPath();
        narLibraryDirs.add(narFilesPath);

        try {
            File unpackedFramework = null;
            final Set<File> unpackedExtensions = new HashSet<>();
            final List<File> narFiles = new ArrayList<>();

            // make sure the nar directories are there and accessible
            FileUtils.ensureDirectoryExistAndCanReadAndWrite(unpackDirectory);

            for (Path narLibraryDir : narLibraryDirs) {

                File narDir = narLibraryDir.toFile();

                // Test if the source NARs can be read
                FileUtils.ensureDirectoryExistAndCanRead(narDir);

                File[] dirFiles = narDir.listFiles(NAR_FILTER);
                if (dirFiles != null) {
                    List<File> fileList = Arrays.asList(dirFiles);
                    narFiles.addAll(fileList);
                }
            }

            if (!narFiles.isEmpty()) {
                final long startTime = System.nanoTime();
                logger.info("Expanding " + narFiles.size() + " NAR files with all processors...");
                for (File narFile : narFiles) {
                    logger.debug("Expanding NAR file: " + narFile.getAbsolutePath() + " to " + unpackDirectory);

                    // get the manifest for this nar
                    try (final JarFile nar = new JarFile(narFile)) {
                        logger.debug("Expanding NAR file: " + nar.getName());
                        final Manifest manifest = nar.getManifest();

                        // lookup the nar id
                        final Attributes attributes = manifest.getMainAttributes();
                        final String groupId = attributes.getValue(NarManifestEntry.NAR_GROUP.getManifestName());
                        final String narId = attributes.getValue(NarManifestEntry.NAR_ID.getManifestName());
                        final String version = attributes.getValue(NarManifestEntry.NAR_VERSION.getManifestName());
                        final String dependencyId = attributes.getValue(NarManifestEntry.NAR_DEPENDENCY_ID.getManifestName());
                        logger.debug("Expanding NAR file: " + nar.getName() + " has dependency " + dependencyId);
                        narMaps.put(narId,narFilesDir);
                        narDependencies.put(narId,dependencyId);
                        // determine if this is the framework
                        if (NarClassLoaders.FRAMEWORK_NAR_ID.equals(narId)) {
                            if (unpackedFramework != null) {
                                throw new IllegalStateException("Multiple framework NARs discovered. Only one framework is permitted.");
                            }

                            // unpack the framework nar
                            unpackedFramework = unpackNar(narFile,paths, unpackDirectory);
                        } else {
                            final File unpackedExtension = unpackNar(narFile,paths, unpackDirectory);

                            // record the current bundle
                            unpackedNars.put(unpackedExtension, new BundleCoordinate(groupId, narId, version));

                            // unpack the extension nar
                            unpackedExtensions.add(unpackedExtension);
                        }
                    }
                }

                final long duration = System.nanoTime() - startTime;
                logger.info("NAR loading process took " + duration + " nanoseconds "
                        + "(" + (int) TimeUnit.SECONDS.convert(duration, TimeUnit.NANOSECONDS) + " seconds).");
            }

        } catch (IOException e) {
            logger.warn("Unable to load NAR library bundles due to " + e + " Will proceed without loading any further Nar bundles");
        }
    }

    /**
     * Unpacks the specified nar into the specified base working directory.
     *
     * @param nar the nar to unpack
     * @param baseWorkingDirectory the directory to unpack to
     * @return the directory to the unpacked NAR
     * @throws IOException if unable to explode nar
     */
    private static File unpackNar(final File nar, List<File> paths,  final File baseWorkingDirectory) throws IOException {
        final File narWorkingDirectory = new File(baseWorkingDirectory, nar.getName() + "-unpacked");

        // if the working directory doesn't exist, unpack the nar
        if (!narWorkingDirectory.exists()) {
            narWorkingDirectory.mkdirs();
            paths.add(narWorkingDirectory);
            unpack(nar, narWorkingDirectory, calculateMd5sum(nar));
        } else {
            // the working directory does exist. Run MD5 sum against the nar
            // file and check if the nar has changed since it was deployed.
            final byte[] narMd5 = calculateMd5sum(nar);
            final File workingHashFile = new File(narWorkingDirectory, HASH_FILENAME);
            if (!workingHashFile.exists()) {
                FileUtils.deleteFile(narWorkingDirectory, true);
                unpack(nar, narWorkingDirectory, narMd5);
            } else {
                final byte[] hashFileContents = Files.readAllBytes(workingHashFile.toPath());
                if (!Arrays.equals(hashFileContents, narMd5)) {
                    FileUtils.deleteFile(narWorkingDirectory, true);
                    unpack(nar, narWorkingDirectory, narMd5);
                }
            }
        }

        return narWorkingDirectory;
    }

    /**
     * Unpacks the NAR to the specified directory. Creates a checksum file that
     * used to determine if future expansion is necessary.
     *
     * @param workingDirectory the root directory to which the NAR should be unpacked.
     * @throws IOException if the NAR could not be unpacked.
     */
    private static void unpack(final File nar, final File workingDirectory, final byte[] hash) throws IOException {

        try (JarFile jarFile = new JarFile(nar)) {
            Enumeration<JarEntry> jarEntries = jarFile.entries();
            while (jarEntries.hasMoreElements()) {
                JarEntry jarEntry = jarEntries.nextElement();
                String name = jarEntry.getName();

                if(name.contains("META-INF/bundled-dependencies")){
                    name = name.replace("META-INF/bundled-dependencies", "NAR-INF/bundled-dependencies");
                }
                File f = new File(workingDirectory, name);
                logger.info("Creating {}",f.getAbsolutePath());
                if (jarEntry.isDirectory()) {
                    f.mkdirs();
                    FileUtils.ensureDirectoryExistAndCanReadAndWrite(f);
                } else {
                    makeFile(jarFile.getInputStream(jarEntry), f);
                }
            }
        }

        final File hashFile = new File(workingDirectory, HASH_FILENAME);
        try (final FileOutputStream fos = new FileOutputStream(hashFile)) {
            fos.write(hash);
        }
    }


    /**
     * Creates the specified file, whose contents will come from the
     * <tt>InputStream</tt>.
     *
     * @param inputStream
     *            the contents of the file to create.
     * @param file
     *            the file to create.
     * @throws IOException
     *             if the file could not be created.
     */
    private static void makeFile(final InputStream inputStream, final File file) throws IOException {
        try (final InputStream in = inputStream;
             final FileOutputStream fos = new FileOutputStream(file)) {
            byte[] bytes = new byte[65536];
            int numRead;
            while ((numRead = in.read(bytes)) != -1) {
                fos.write(bytes, 0, numRead);
            }
        }
    }

    /**
     * Calculates an md5 sum of the specified file.
     *
     * @param file
     *            to calculate the md5sum of
     * @return the md5sum bytes
     * @throws IOException
     *             if cannot read file
     */
    private static byte[] calculateMd5sum(final File file) throws IOException {
        try (final FileInputStream inputStream = new FileInputStream(file)) {
            final MessageDigest md5 = MessageDigest.getInstance("md5");

            final byte[] buffer = new byte[1024];
            int read = inputStream.read(buffer);

            while (read > -1) {
                md5.update(buffer, 0, read);
                read = inputStream.read(buffer);
            }

            return md5.digest();
        } catch (NoSuchAlgorithmException nsae) {
            throw new IllegalArgumentException(nsae);
        }
    }

    private JniUnpacker() {
    }
}
