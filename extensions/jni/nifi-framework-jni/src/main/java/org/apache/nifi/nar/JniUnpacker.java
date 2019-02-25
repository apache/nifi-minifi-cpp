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

    public static ExtensionMapping unpackNars(final File narFilesDir, final File unpackDirectory, final File docsWorkingDir, List<File> paths ) throws IOException {
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

            // attempt to delete any docs files that exist so that any components that have been removed
            // will no longer have entries in the docs folder
            final File[] docsFiles = docsWorkingDir.listFiles();
            if (docsFiles != null) {
                for (final File file : docsFiles) {
                    FileUtils.deleteFile(file, true);
                }
            }

            final ExtensionMapping extensionMapping = new ExtensionMapping();
            mapExtensions(unpackedNars, docsWorkingDir, extensionMapping);

            return extensionMapping;
        } catch (IOException e) {
            logger.warn("Unable to load NAR library bundles due to " + e + " Will proceed without loading any further Nar bundles");
            if (logger.isDebugEnabled()) {
                logger.warn("", e);
            }
        }

        return null;
    }

    private static void mapExtensions(final Map<File, BundleCoordinate> unpackedNars, final File docsDirectory, final ExtensionMapping mapping) throws IOException {
        for (final Map.Entry<File, BundleCoordinate> entry : unpackedNars.entrySet()) {
            final File unpackedNar = entry.getKey();
            final BundleCoordinate bundleCoordinate = entry.getValue();

            final File bundledDependencies = new File(unpackedNar, "NAR-INF/bundled-dependencies");

            unpackBundleDocs(docsDirectory, mapping, bundleCoordinate, bundledDependencies);
        }
    }

    private static void unpackBundleDocs(final File docsDirectory, final ExtensionMapping mapping, final BundleCoordinate bundleCoordinate, final File bundledDirectory) throws IOException {
        final File[] directoryContents = bundledDirectory.listFiles();
        if (directoryContents != null) {
            for (final File file : directoryContents) {
                if (file.getName().toLowerCase().endsWith(".jar")) {
                    unpackDocumentation(bundleCoordinate, file, docsDirectory, mapping);
                }
            }
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

    private static void unpackDocumentation(final BundleCoordinate coordinate, final File jar, final File docsDirectory, final ExtensionMapping extensionMapping) throws IOException {
        final ExtensionMapping jarExtensionMapping = determineDocumentedNiFiComponents(coordinate, jar);

        // skip if there are not components to document
        if (jarExtensionMapping.isEmpty()) {
            return;
        }

        // merge the extension mapping found in this jar
        extensionMapping.merge(jarExtensionMapping);

        // look for all documentation related to each component
        try (final JarFile jarFile = new JarFile(jar)) {
            for (final String componentName : jarExtensionMapping.getAllExtensionNames().keySet()) {
                final String entryName = "docs/" + componentName;

                // go through each entry in this jar
                for (final Enumeration<JarEntry> jarEnumeration = jarFile.entries(); jarEnumeration.hasMoreElements();) {
                    final JarEntry jarEntry = jarEnumeration.nextElement();

                    // if this entry is documentation for this component
                    if (jarEntry.getName().startsWith(entryName)) {
                        final String name = StringUtils.substringAfter(jarEntry.getName(), "docs/");
                        final String path = coordinate.getGroup() + "/" + coordinate.getId() + "/" + coordinate.getVersion() + "/" + name;

                        // if this is a directory create it
                        if (jarEntry.isDirectory()) {
                            final File componentDocsDirectory = new File(docsDirectory, path);

                            // ensure the documentation directory can be created
                            if (!componentDocsDirectory.exists() && !componentDocsDirectory.mkdirs()) {
                                logger.warn("Unable to create docs directory " + componentDocsDirectory.getAbsolutePath());
                                break;
                            }
                        } else {
                            // if this is a file, write to it
                            final File componentDoc = new File(docsDirectory, path);
                            makeFile(jarFile.getInputStream(jarEntry), componentDoc);
                        }
                    }
                }

            }
        }
    }

    /*
     * Returns true if this jar file contains a NiFi component
     */
    private static ExtensionMapping determineDocumentedNiFiComponents(final BundleCoordinate coordinate, final File jar) throws IOException {
        final ExtensionMapping mapping = new ExtensionMapping();

        try (final JarFile jarFile = new JarFile(jar)) {
            final JarEntry processorEntry = jarFile.getJarEntry("META-INF/services/org.apache.nifi.processor.Processor");
            final JarEntry reportingTaskEntry = jarFile.getJarEntry("META-INF/services/org.apache.nifi.reporting.ReportingTask");
            final JarEntry controllerServiceEntry = jarFile.getJarEntry("META-INF/services/org.apache.nifi.controller.ControllerService");

            if (processorEntry==null && reportingTaskEntry==null && controllerServiceEntry==null) {
                return mapping;
            }

            mapping.addAllProcessors(coordinate, determineDocumentedNiFiComponents(jarFile, processorEntry));
            mapping.addAllReportingTasks(coordinate, determineDocumentedNiFiComponents(jarFile, reportingTaskEntry));
            mapping.addAllControllerServices(coordinate, determineDocumentedNiFiComponents(jarFile, controllerServiceEntry));
            return mapping;
        }
    }

    private static List<String> determineDocumentedNiFiComponents(final JarFile jarFile, final JarEntry jarEntry) throws IOException {
        final List<String> componentNames = new ArrayList<>();

        if (jarEntry == null) {
            return componentNames;
        }

        try (final InputStream entryInputStream = jarFile.getInputStream(jarEntry);
             final BufferedReader reader = new BufferedReader(new InputStreamReader(entryInputStream))) {

            String line;
            while ((line = reader.readLine()) != null) {
                final String trimmedLine = line.trim();
                if (!trimmedLine.isEmpty() && !trimmedLine.startsWith("#")) {
                    final int indexOfPound = trimmedLine.indexOf("#");
                    final String effectiveLine = (indexOfPound > 0) ? trimmedLine.substring(0, indexOfPound) : trimmedLine;
                    componentNames.add(effectiveLine);
                }
            }
        }

        return componentNames;
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
