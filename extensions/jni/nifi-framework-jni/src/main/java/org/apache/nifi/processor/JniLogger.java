package org.apache.nifi.processor;

public class JniLogger {

    private long nativePtr;


    public native boolean isWarnEnabled();
    public native boolean isTraceEnabled();
    public native boolean isInfoEnabled();
    public native boolean isErrorEnabled();
    public native boolean isDebugEnabled();


    public native void warn(String msg);
    public native void error(String msg);
    public native void info(String msg);
    public native void debug(String msg);
    public native void trace(String msg);

}
