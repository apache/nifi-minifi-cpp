package org.apache.nifi.processor;

import org.apache.nifi.controller.ControllerServiceLookup;
import org.apache.nifi.controller.NodeTypeProvider;
import org.apache.nifi.logging.ComponentLog;

import java.io.File;

public class JniInitializationContext implements ProcessorInitializationContext {


    private long nativePtr;

    JniLogger logger = null;


    @Override
    public String getIdentifier() {
        return null;
    }


    /**
     * Native method to set the logger instance.
     * @param logger logger instance
     */
    public void setLogger(final JniLogger logger){
        this.logger = logger;
    }


    @Override
    public ComponentLog getLogger() {
        return new JniComponentLogger(logger);
    }

    @Override
    public ControllerServiceLookup getControllerServiceLookup() {
        return null;
    }

    @Override
    public NodeTypeProvider getNodeTypeProvider() {
        return null;
    }

    @Override
    public String getKerberosServicePrincipal() {
        return null;
    }

    @Override
    public File getKerberosServiceKeytab() {
        return null;
    }

    @Override
    public File getKerberosConfigurationFile() {
        return null;
    }
}
