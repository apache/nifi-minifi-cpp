package org.apache.nifi.processor;

import org.apache.nifi.attribute.expression.language.StandardPropertyValue;
import org.apache.nifi.components.AbstractConfigurableComponent;
import org.apache.nifi.components.PropertyDescriptor;
import org.apache.nifi.components.PropertyValue;
import org.apache.nifi.controller.ConfigurationContext;
import org.apache.nifi.controller.ControllerService;
import org.apache.nifi.controller.ControllerServiceLookup;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

public class JniConfigurationContext implements ConfigurationContext, ControllerServiceLookup{

    long nativePtr;


    private native List<String> getPropertyNames();

    private native AbstractConfigurableComponent getComponent();
    @Override
    public Map<PropertyDescriptor, String> getProperties() {
        List<String> propertyNames = getPropertyNames();
        AbstractConfigurableComponent component = getComponent();

        return propertyNames.stream().collect(Collectors.toMap(component::getPropertyDescriptor, this::getPropertyValue));

    }

    @Override
    public String getSchedulingPeriod() {
        return "1";
    }

    @Override
    public Long getSchedulingPeriod(TimeUnit timeUnit) {
        return 1L;
    }

    @Override
    public native String getName();

    public native String getPropertyValue(final String propertyName);

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


    @Override
    public ControllerService getControllerService(String s) {
        return null;
    }

    @Override
    public boolean isControllerServiceEnabled(String s) {
        return false;
    }

    @Override
    public boolean isControllerServiceEnabling(String s) {
        return false;
    }

    @Override
    public boolean isControllerServiceEnabled(ControllerService controllerService) {
        return false;
    }

    @Override
    public Set<String> getControllerServiceIdentifiers(Class<? extends ControllerService> aClass) throws IllegalArgumentException {
        return null;
    }

    @Override
    public String getControllerServiceName(String s) {
        return null;
    }
}
