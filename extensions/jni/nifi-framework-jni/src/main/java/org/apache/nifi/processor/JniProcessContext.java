package org.apache.nifi.processor;

import org.apache.nifi.attribute.expression.language.StandardPropertyValue;
import org.apache.nifi.components.AbstractConfigurableComponent;
import org.apache.nifi.components.PropertyDescriptor;
import org.apache.nifi.components.PropertyValue;
import org.apache.nifi.components.state.StateManager;
import org.apache.nifi.controller.ControllerService;
import org.apache.nifi.controller.ControllerServiceLookup;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

public class JniProcessContext implements ProcessContext, ControllerServiceLookup {

    private long nativePtr;

    @Override
    public ControllerService getControllerService(String serviceIdentifier) {
        return null;
    }

    @Override
    public boolean isControllerServiceEnabled(String serviceIdentifier) {
        return false;
    }

    @Override
    public boolean isControllerServiceEnabling(String serviceIdentifier) {
        return false;
    }

    @Override
    public boolean isControllerServiceEnabled(ControllerService service) {
        return false;
    }

    @Override
    public Set<String> getControllerServiceIdentifiers(Class<? extends ControllerService> serviceType) throws IllegalArgumentException {
        return null;
    }

    @Override
    public String getControllerServiceName(String serviceIdentifier) {
        return null;
    }

    @Override
    public PropertyValue getProperty(String propertyName) {
        String value = getPropertyValue(propertyName);
        System.out.println("for " + propertyName + " got " + value);
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
        Processor processor = getProcessor();
        if (processor instanceof AbstractConfigurableComponent) {
            AbstractConfigurableComponent process = AbstractConfigurableComponent.class.cast(getProcessor());
            if (process != null) {
                return propertyNames.stream().collect(Collectors.toMap(process::getPropertyDescriptor, this::getPropertyValue));
            }
        }

        return null;
    }

    private native List<String> getPropertyNames();

    private native Processor getProcessor();

    @Override
    public String encrypt(String unencrypted) {
        return null;
    }

    @Override
    public String decrypt(String encrypted) {
        return null;
    }

    @Override
    public ControllerServiceLookup getControllerServiceLookup() {
        return null;
    }

    @Override
    public Set<Relationship> getAvailableRelationships() {
        return null;
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
        return false;
    }

    @Override
    public StateManager getStateManager() {
        return null;
    }

    @Override
    public String getName() {
        return null;
    }

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
