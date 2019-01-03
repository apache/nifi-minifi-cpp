package org.apache.nifi.processor;

import org.apache.nifi.components.PropertyDescriptor;

import java.util.*;
import java.util.stream.Collectors;

/**
 * JNIComponent is a compnent object used for building the manifest within MiNiFi C++
 */
public class JniComponent {
    private String type;
    private String description;
    private List<PropertyDescriptor> descriptorsList;
    private Set<Relationship> relationships;
    private boolean dynamicProperties;
    private boolean dynamicRelationships;
    private boolean isControllerService;

    private JniComponent(final String type){
        this.type = type;
        this.descriptorsList = new ArrayList<>();
        this.relationships = new HashSet<>();
        this.dynamicProperties = false;
        this.dynamicRelationships = false;
        isControllerService = false;
    }


    public String getType(){
        return type;
    }

    public String getDescription(){
        return description;
    }

    public boolean isControllerService() { return isControllerService; }

    public List<PropertyDescriptor> getDescriptors(){
        return Collections.unmodifiableList(descriptorsList);
    }


    /**
     * Return a list of relationshipos. internally we capture this as a set, but converting this
     * to a list enables us to access this easier in JNI.
     * @return
     */
    public List<Relationship> getRelationships(){
        return Collections.unmodifiableList(relationships.stream().collect(Collectors.toList()));
    }

    public boolean getDynamicRelationshipsSupported(){
        return dynamicRelationships;
    }

    public boolean getDynamicPropertiesSupported(){
        return dynamicProperties;
    }

    public static class JniComponentBuilder{

        public static JniComponentBuilder create(final String type){
            return new JniComponentBuilder(type);
        }


        public JniComponentBuilder addProperty(final PropertyDescriptor property){
            component.descriptorsList.add(property);
            return this;
        }

        public JniComponentBuilder addProperties(final List<PropertyDescriptor> descriptorsList){
            component.descriptorsList.addAll(descriptorsList);
            return this;
        }

        public JniComponentBuilder addRelationships(final Set<Relationship> relationships){
            component.relationships.addAll(relationships);
            return this;
        }

        public JniComponentBuilder setDynamicProperties(){
            component.dynamicProperties = true;
            return this;
        }

        public JniComponentBuilder setDynamicRelationships(){
            component.dynamicRelationships = true;
            return this;
        }

        public JniComponentBuilder setIsControllerService(){
            component.isControllerService = true;
            return this;
        }

        public JniComponent build() {
            return component;
        }


        public JniComponentBuilder addDescription(String description) {
            component.description = description;
            return this;
        }

       private JniComponentBuilder(final String type){
        component = new JniComponent(type);
        }

        private JniComponent component;

    }


}
