package org.apache.nifi.processor;

import org.apache.nifi.logging.ComponentLog;
import org.apache.nifi.logging.LogLevel;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.helpers.MessageFormatter;

/**
 * JniComponent Logger enables us to access the log instance within MiNiFi C++
 */
public class JniComponentLogger implements ComponentLog {

    private static final Logger logger = LoggerFactory
            .getLogger(JniComponentLogger.class);

    private JniLogger jniLogger = null;

    public JniComponentLogger(JniLogger logger){
        jniLogger = logger;
        if (null != jniLogger){
            jniLogger.info("Starting NiFi component logger..");
        }
    }

    @Override
    public void warn(String msg, Throwable t) {
        if (jniLogger != null){
            jniLogger.warn(MessageFormatter.format(msg,t).getMessage());
        }
        else {
            logger.warn(msg, t);
        }
    }

    @Override
    public void warn(String msg, Object[] os) {
        if (jniLogger != null){
            jniLogger.warn(MessageFormatter.arrayFormat(msg,os).getMessage());
        }
        else {
            logger.warn(msg, os);
        }
    }

    @Override
    public void warn(String msg, Object[] os, Throwable t) {
        if (jniLogger != null){
            jniLogger.warn(MessageFormatter.arrayFormat(msg,os,t).getMessage());
        }
        else {
            logger.warn(msg, os);
            logger.warn("", t);
        }
    }

    @Override
    public void warn(String msg) {
        if (jniLogger != null){
            jniLogger.warn(msg);
        }
        else {
            logger.warn(msg);
        }
    }

    @Override
    public void trace(String msg, Throwable t) {
        if (jniLogger != null){
            jniLogger.trace(MessageFormatter.format(msg,t).getMessage());
        }
        else {
            logger.trace(msg, t);
        }
    }

    @Override
    public void trace(String msg, Object[] os) {
        if (jniLogger != null){
            jniLogger.trace(MessageFormatter.arrayFormat(msg,os).getMessage());
        }
        else {
            logger.trace(msg, os);
        }
    }

    @Override
    public void trace(String msg) {
        if (jniLogger != null){
            jniLogger.trace(msg);
        }
        else {
            logger.trace(msg);
        }
    }

    @Override
    public void trace(String msg, Object[] os, Throwable t) {
        if (jniLogger != null){
            jniLogger.trace(MessageFormatter.arrayFormat(msg,os,t).getMessage());
        }
        else {
            logger.trace(msg, os);
            logger.trace("", t);
        }
    }

    @Override
    public boolean isWarnEnabled() {
        if (jniLogger != null)
            return jniLogger.isWarnEnabled();
        return logger.isWarnEnabled();
    }

    @Override
    public boolean isTraceEnabled() {
        if (jniLogger != null)
            return jniLogger.isTraceEnabled();
        return logger.isTraceEnabled();
    }

    @Override
    public boolean isInfoEnabled() {
        if (jniLogger != null)
            return jniLogger.isInfoEnabled();
        return logger.isInfoEnabled();
    }

    @Override
    public boolean isErrorEnabled() {
        if (jniLogger != null)
            return jniLogger.isErrorEnabled();
        return logger.isErrorEnabled();
    }

    @Override
    public boolean isDebugEnabled() {
        if (jniLogger != null)
            return jniLogger.isDebugEnabled();
        return logger.isDebugEnabled();
    }

    @Override
    public void info(String msg, Throwable t) {
        if (jniLogger != null){
            jniLogger.info(MessageFormatter.format(msg,t).getMessage());
        }
        else {
            logger.info(msg, t);
        }
    }

    @Override
    public void info(String msg, Object[] os) {
        if (jniLogger != null){
            jniLogger.info(MessageFormatter.arrayFormat(msg,os).getMessage());
        }
        else {
            logger.info(msg, os);
        }
    }

    @Override
    public void info(String msg) {
        if (jniLogger != null){
            jniLogger.info(msg);
        }
        else {
            logger.info(msg);
        }
    }

    @Override
    public void info(String msg, Object[] os, Throwable t) {
        if (jniLogger != null){
            jniLogger.info(MessageFormatter.format(msg,os,t).getMessage());
        }
        else {
            logger.info(msg, os);
        }
    }

    @Override
    public String getName() {
        return logger.getName();
    }

    @Override
    public void error(String msg, Throwable t) {
        if (jniLogger != null){
            jniLogger.info(MessageFormatter.format(msg,t).getMessage());
        }
        else {
            logger.error(msg, t);
        }
    }

    @Override
    public void error(String msg, Object[] os) {
        if (jniLogger != null){
            jniLogger.error(MessageFormatter.arrayFormat(msg,os).getMessage());
        }
        else {
            logger.error(msg, os);
        }
    }

    @Override
    public void error(String msg) {
        if (jniLogger != null){
            jniLogger.error(msg);
        }
        else {
            logger.error(msg);
        }
    }

    @Override
    public void error(String msg, Object[] os, Throwable t) {
        if (jniLogger != null){
            jniLogger.error(MessageFormatter.arrayFormat(msg,os).getMessage());
        }
        else {
            logger.error(msg, os);
            logger.error("",t.getMessage());
        }
    }

    @Override
    public void debug(String msg, Throwable t) {
        if (jniLogger != null){
            jniLogger.debug(MessageFormatter.format(msg,t).getMessage());

        }
        else {
            logger.debug(msg, t);
        }
    }

    @Override
    public void debug(String msg, Object[] os) {
        if (jniLogger != null){
            jniLogger.debug(MessageFormatter.arrayFormat(msg,os).getMessage());
        }
        else {
            logger.debug(msg, os);
        }
    }

    @Override
    public void debug(String msg, Object[] os, Throwable t) {
        if (jniLogger != null){
            jniLogger.debug(MessageFormatter.arrayFormat(msg,os,t).getMessage());
        }
        else {
            logger.debug(msg, os);
            logger.debug("",t.getMessage());
        }
    }

    @Override
    public void debug(String msg) {
        if (jniLogger != null){
            jniLogger.debug(msg);
        }
        else {
            logger.debug(msg);
        }
    }

    @Override
    public void log(LogLevel level, String msg, Throwable t) {
        switch (level) {
            case DEBUG:
                debug(msg, t);
                break;
            case ERROR:
            case FATAL:
                error(msg, t);
                break;
            case INFO:
                info(msg, t);
                break;
            case TRACE:
                trace(msg, t);
                break;
            case WARN:
                warn(msg, t);
                break;
        }
    }

    @Override
    public void log(LogLevel level, String msg, Object[] os) {
        switch (level) {
            case DEBUG:
                debug(msg, os);
                break;
            case ERROR:
            case FATAL:
                error(msg, os);
                break;
            case INFO:
                info(msg, os);
                break;
            case TRACE:
                trace(msg, os);
                break;
            case WARN:
                warn(msg, os);
                break;
        }
    }

    @Override
    public void log(LogLevel level, String msg) {
        switch (level) {
            case DEBUG:
                debug(msg);
                break;
            case ERROR:
            case FATAL:
                error(msg);
                break;
            case INFO:
                info(msg);
                break;
            case TRACE:
                trace(msg);
                break;
            case WARN:
                warn(msg);
                break;
        }
    }

    @Override
    public void log(LogLevel level, String msg, Object[] os, Throwable t) {
        switch (level) {
            case DEBUG:
                debug(msg, os, t);
                break;
            case ERROR:
            case FATAL:
                error(msg, os, t);
                break;
            case INFO:
                info(msg, os, t);
                break;
            case TRACE:
                trace(msg, os, t);
                break;
            case WARN:
                warn(msg, os, t);
                break;
        }
    }
}
