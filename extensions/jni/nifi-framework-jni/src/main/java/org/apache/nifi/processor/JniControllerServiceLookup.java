package org.apache.nifi.processor;

import org.apache.nifi.controller.ControllerService;
import org.apache.nifi.controller.ControllerServiceLookup;

import java.util.HashSet;
import java.util.Set;

public class JniControllerServiceLookup implements ControllerServiceLookup {

    long nativePtr;

    @Override
    public native ControllerService getControllerService(String s);

    @Override
    public native boolean isControllerServiceEnabled(String s);

    @Override
    public native boolean isControllerServiceEnabling(String s);

    @Override
    public boolean isControllerServiceEnabled(ControllerService controllerService){
        return isControllerServiceEnabled(controllerService.getIdentifier());
    }

    @Override
    public Set<String> getControllerServiceIdentifiers(Class<? extends ControllerService> aClass) throws IllegalArgumentException
    {
        return new HashSet<>();
    }

    @Override
    public native String getControllerServiceName(String s);
}
