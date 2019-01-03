package org.apache.nifi.processor;

import org.apache.nifi.flowfile.FlowFile;

import java.util.Map;

public class JniFlowFile implements FlowFile {

    private long nativePtr;

    public JniFlowFile(){
    }

    @Override
    public native long getId();

    @Override
    public native long getEntryDate();

    @Override
    public native long getLineageStartDate();

    @Override
    public native long getLineageStartIndex();

    @Override
    public Long getLastQueueDate(){
        return getLastQueueDatePrim();
    }

    private native long getLastQueueDatePrim();

    @Override
    public native long getQueueDateIndex();

    @Override
    public native boolean isPenalized();

    @Override
    public native String getAttribute(String key);

    @Override
    public native long getSize();

    @Override
    public native Map<String, String> getAttributes();

    @Override
    public int compareTo(FlowFile o) {
        return 0;
    }

    public native String getUUIDStr();

    @Override
    public String toString(){
        return getUUIDStr();
    }
}
