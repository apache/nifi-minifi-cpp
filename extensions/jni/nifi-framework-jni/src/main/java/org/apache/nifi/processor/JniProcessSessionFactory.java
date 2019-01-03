package org.apache.nifi.processor;

public class JniProcessSessionFactory implements ProcessSessionFactory {

    private long nativePtr;

    @Override
    public native ProcessSession createSession();
}
