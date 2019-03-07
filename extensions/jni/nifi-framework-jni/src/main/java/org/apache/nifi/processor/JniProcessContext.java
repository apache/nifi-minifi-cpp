package org.apache.nifi.processor;

import org.apache.nifi.attribute.expression.language.StandardPropertyValue;
import org.apache.nifi.components.AbstractConfigurableComponent;
import org.apache.nifi.components.PropertyDescriptor;
import org.apache.nifi.components.PropertyValue;
import org.apache.nifi.components.state.StateManager;
import org.apache.nifi.controller.ConfigurationContext;
import org.apache.nifi.controller.ControllerService;
import org.apache.nifi.controller.ControllerServiceLookup;

import java.util.*;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

public class JniProcessContext implements ProcessContext, ControllerServiceLookup{

    private long nativePtr;

    @Override
    public ControllerService getControllerService(String serviceIdentifier) {
        return getControllerServiceLookup().getControllerService(serviceIdentifier);
    }

    @Override
    public boolean isControllerServiceEnabled(String serviceIdentifier) {
        return getControllerServiceLookup().isControllerServiceEnabled(serviceIdentifier);
    }

    @Override
    public boolean isControllerServiceEnabling(String serviceIdentifier) {
        return getControllerServiceLookup().isControllerServiceEnabling(serviceIdentifier);
    }

    @Override
    public boolean isControllerServiceEnabled(ControllerService service) {
        return getControllerServiceLookup().isControllerServiceEnabled(service);
    }

    @Override
    public Set<String> getControllerServiceIdentifiers(Class<? extends ControllerService> serviceType) throws IllegalArgumentException {
        return getControllerServiceLookup().getControllerServiceIdentifiers(serviceType);
    }

    @Override
    public String getControllerServiceName(String serviceIdentifier) {
        return getControllerServiceLookup().getControllerServiceName(serviceIdentifier);
    }

    @Override
    public PropertyValue getProperty(String propertyName) {
        String value = getPropertyValue(propertyName);
        return new StandardPropertyValue(value,this);
    }



    @Override
    public PropertyValue newPropertyValue(String rawValue) {
        return new StandardPropertyValue(rawValue,this);
    }

    public native String getPropertyValue(final String propertyName);

    @Override
    public void yield() {

    }

    @Override
    public int getMaxConcurrentTasks() {
        return 0;
    }

    @Override
    public String getAnnotationData() {
        return null;
    }

    @Override
    public Map<PropertyDescriptor, String> getProperties() {
        List<String> propertyNames = getPropertyNames();
        AbstractConfigurableComponent process = getComponent();


            if (process != null) {
                return propertyNames.stream().collect(Collectors.toMap(process::getPropertyDescriptor, this::getPropertyValue));
            }


        return null;
    }

    private native List<String> getPropertyNames();

    private native AbstractConfigurableComponent getComponent();

    @Override
    public String encrypt(String unencrypted) {
        return null;
    }

    @Override
    public String decrypt(String encrypted) {
        return null;
    }

    @Override
    public native ControllerServiceLookup getControllerServiceLookup();

    @Override
    public Set<Relationship> getAvailableRelationships() {
        return new HashSet<>();
    }

    @Override
    public boolean hasIncomingConnection() {
        return false;
    }

    @Override
    public boolean hasNonLoopConnection() {
        return false;
    }

    @Override
    public boolean hasConnection(Relationship relationship) {
        return false;
    }

    @Override
    public boolean isExpressionLanguagePresent(PropertyDescriptor property) {
        return property.isExpressionLanguageSupported();
    }

    @Override
    public StateManager getStateManager() {
        return null;
    }

    @Override
    public native String getName();

    @Override
    public PropertyValue getProperty(PropertyDescriptor descriptor) {
        String value = getPropertyValue(descriptor.getName());
        if (value == null || "null".equals(value))
            value = descriptor.getDefaultValue();
        return new StandardPropertyValue(value,this);
    }

    @Override
    public Map<String, String> getAllProperties() {
        Map<PropertyDescriptor, String> map = getProperties();
        Map<String,String> newProps = new HashMap<>();
        map.forEach((x,y) ->
        {
           newProps.put(x.getName(),y);
        });
        return newProps;

    }
}
