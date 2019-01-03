package org.apache.nifi.processor;

import java.io.IOException;
import java.io.InputStream;

public class JniInputStream extends InputStream {

    private long nativePtr;

    @Override
    public native int read() throws IOException;
/*
    @Override
    public int read(byte[] copyTo, int offset, int length) throws IOException{
        return readWithOffset(copyTo,offset,length);
    }*/

    public native int readWithOffset(byte[] copyTo, int offset, int length) throws IOException;
}
