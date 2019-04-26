package org.apache.nifi.processor;

import org.apache.commons.lang3.NotImplementedException;
import org.apache.nifi.controller.queue.QueueSize;
import org.apache.nifi.flowfile.FlowFile;
import org.apache.nifi.processor.exception.FlowFileAccessException;
import org.apache.nifi.processor.io.InputStreamCallback;
import org.apache.nifi.processor.io.OutputStreamCallback;
import org.apache.nifi.processor.io.StreamCallback;
import org.apache.nifi.provenance.ProvenanceReporter;

import java.io.*;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.*;
import java.util.regex.Pattern;

public class JniProcessSession implements ProcessSession {


    private long nativePtr;

    @Override
    public native void commit();
    @Override
    public native void rollback();

    @Override
    public void rollback(boolean penalize){
        rollback();
    }



    @Override
    public void migrate(ProcessSession newOwner, Collection<FlowFile> flowFiles) {

    }

    @Override
    public void adjustCounter(String name, long delta, boolean immediate) {
    }

    @Override
    public native FlowFile get();

    @Override
    public List<FlowFile> get(int maxResults) {
        List<FlowFile> flowfiles = new ArrayList<>();
        FlowFile ff = null;
        int i=0;
        do{
            ff = get();
            if (ff == null)
                break;
            flowfiles.add(ff);
            i++;
        }while(i < maxResults);
        return flowfiles;
    }

    @Override
    public List<FlowFile> get(FlowFileFilter filter) {
        return null;
    }

    @Override
    public QueueSize getQueueSize() {
        return null;
    }

    @Override
    public native FlowFile create();

    @Override
    public FlowFile create(FlowFile parent){
        return createWithParent(parent);
    }

    private native FlowFile createWithParent(FlowFile parent);

    @Override
    public FlowFile create(Collection<FlowFile> parents) {
        return null;
    }

    @Override
    public native FlowFile clone(FlowFile example);

    @Override
    public FlowFile clone(FlowFile parent, long offset, long size){
        return clonePortion(parent,offset,size);
    }

    private native FlowFile clonePortion(FlowFile parent,  long offset, long size);

    @Override
    public native FlowFile penalize(FlowFile flowFile);

    @Override
    public native FlowFile putAttribute(FlowFile flowFile, String key, String value);

    @Override
    public FlowFile putAllAttributes(FlowFile flowFile, Map<String, String> attributes) {
        for(Map.Entry<String,String> entry : attributes.entrySet()){
            putAttribute(flowFile,entry.getKey(),entry.getValue());
        }
        return flowFile;
    }

    @Override
    public native FlowFile removeAttribute(FlowFile flowFile, String key);

    @Override
    public FlowFile removeAllAttributes(FlowFile flowFile, Set<String> keys){
        for(String attr : keys){
            removeAttribute(flowFile,attr);
        }
        return flowFile;
    }

    @Override
    public FlowFile removeAllAttributes(FlowFile flowFile, Pattern keyPattern){
        if (flowFile != null){
            Map<String,String> attributes = flowFile.getAttributes();
            Set<String> keys = new HashSet<>();
            for(Map.Entry<String,String> attr : attributes.entrySet()){
                if (keyPattern.matcher(attr.getKey()).matches()){
                    keys.add(attr.getKey());
                }
            }
            return removeAllAttributes(flowFile,keys);
        }
        return null;
    }

    @Override
    public void transfer(FlowFile flowFile, Relationship relationship){
        transfer(flowFile,relationship.getName());
    }

    protected native void transfer(FlowFile flowFile, String relationship);

    @Override
    public void transfer(FlowFile flowFile){
        transfer(flowFile,"success");
    }

    @Override
    public void transfer(Collection<FlowFile> flowFiles) {
        for(FlowFile ff : flowFiles){
            transfer(ff);
        }
    }

    @Override
    public void transfer(Collection<FlowFile> flowFiles, Relationship relationship) {
        for(FlowFile flowFile : flowFiles){
            transfer(flowFile,relationship.getName());
        }
    }

    @Override
    public native void remove(FlowFile flowFile);

    @Override
    public void remove(Collection<FlowFile> flowFiles) {
        for(FlowFile ff: flowFiles){
            remove(ff);
        }
    }

    /**
     * I don't like surrounding this with a Buffered Input Stream, but it seems that certain features expect this
     * Case in point, CSV Reader:
     *   createRecordReader(final Map<String, String> variables, final InputStream in, final ComponentLog logger)
     *
     * In this method we've erased the concrete type and are assuming the InputStream is a BufferedInputStream.
     * While we can fix this, there is no guarantee that others don't abide by this. As a result we'll use
     * BufferedInputStream here until we can safely move away.
     */
    @Override
    public void read(FlowFile source, InputStreamCallback reader) throws FlowFileAccessException {
        try {
            final BufferedInputStream input = new BufferedInputStream( readFlowFile(source) );
            if (input != null)
                reader.process(input);
        } catch (IOException e) {
            e.printStackTrace();
            throw new FlowFileAccessException("Could not read from native source", e);
        }
    }

    @Override
    public InputStream read(FlowFile flowFile) {
        /**
         * I don't like surrounding this with a Buffered Input Stream, but it seems that certain features expect this
         * Case in point, CSV Reader:
         *   createRecordReader(final Map<String, String> variables, final InputStream in, final ComponentLog logger)
         *
         * In this method we've erased the concrete type and are assuming the InputStream is a BufferedInputStream.
         * While we can fix this, there is no guarantee that others don't abide by this. As a result we'll use
         * BufferedInputStream here until we can safely move away.
         */
        return new BufferedInputStream(readFlowFile(flowFile));
    }

    @Override
    public void read(FlowFile source, boolean allowSessionStreamManagement, InputStreamCallback reader) throws FlowFileAccessException {
        throw new NotImplementedException("Currently not implemented");
    }

    @Override
    public FlowFile merge(Collection<FlowFile> sources, FlowFile destination) {
        throw new NotImplementedException("Currently not implemented");
    }

    @Override
    public FlowFile merge(Collection<FlowFile> sources, FlowFile destination, byte[] header, byte[] footer, byte[] demarcator) {
        throw new NotImplementedException("Currently not implemented");
    }

    @Override
    public FlowFile write(FlowFile source, OutputStreamCallback writer) throws FlowFileAccessException {
        // must write data.
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        try {
            writer.process(bos);
        }catch(IOException os){
            throw new FlowFileAccessException("IOException while processing ff data");
        }
        write(source, bos.toByteArray());

        return source;
    }

    protected native JniInputStream readFlowFile(FlowFile source);

    protected native boolean write(FlowFile source, byte [] array);

    protected native boolean append(FlowFile source , byte [] array);

    @Override
    public OutputStream write(final FlowFile source) {
        return new OutputStream() {
            ByteArrayOutputStream bin = new ByteArrayOutputStream();
            @Override
            public void write(int b) throws IOException {
                synchronized (this) {
                    bin.write(b);
                    // better suited to writing pages of memory
                    if (bin.size() > 4096) {
                        flushByterArray();
                    }
                }
            }

            @Override
            public void flush() throws IOException {
                synchronized (this) {
                    // flush as an append.
                    flushByterArray();
                }
            }


            @Override
            public void close() throws IOException {
                synchronized (this) {
                    // flush as an append.
                    flushByterArray();
                }
            }

            private void flushByterArray(){
                append(source,bin.toByteArray());
                bin = new ByteArrayOutputStream();
            }

        };
    }

    @Override
    public FlowFile write(FlowFile source, StreamCallback writer) throws FlowFileAccessException {
        // must write data.
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        try {
            writer.process(read(source),bos);
        }catch(IOException os){
            throw new FlowFileAccessException("IOException while processing ff data");
        }
        write(source, bos.toByteArray());

        return source;
    }

    @Override
    public FlowFile append(FlowFile source, OutputStreamCallback writer) throws FlowFileAccessException {
        // must write data.
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        try {
            writer.process(bos);
            append(source,bos.toByteArray());
        }catch(IOException os){
            throw new FlowFileAccessException("IOException while processing ff data");
        }
        write(source, bos.toByteArray());

        return source;
    }

    /**
     * IOUtils was slow due to non-buffering. Underlying streams may buffer but this alleviates
     * pressure with a small footprint.
     * @param in input stream
     * @param out output stream
     * @throws IOException
     */
    private static void copyData(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1 * 1024];
        int len;
        while ((len = in.read(buffer)) > 0) {
            out.write(buffer, 0, len);
        }
    }


    @Override
    public FlowFile importFrom(Path source, boolean keepSourceFile, FlowFile destination){
        try {
            try(OutputStream out = write(destination)) {
                copyData(Files.newInputStream(source), out);
                if (!keepSourceFile) {
                    Files.delete(source);
                }
            }
        } catch (IOException e) {
            return null;
        }
        return destination;
    }

    @Override
    public FlowFile importFrom(InputStream source, FlowFile destination){
        try {
            try(OutputStream out = write(destination)) {
                copyData(source, out);
            }
        } catch (IOException e) {
            return null;
        }
        return destination;
    }

    @Override
    public void exportTo(FlowFile flowFile, Path destination, boolean append) {

    }

    @Override
    public void exportTo(FlowFile flowFile, OutputStream destination) {

    }

    @Override
    public ProvenanceReporter getProvenanceReporter() {
        return new JniProvenanceReporter();
    }
}
