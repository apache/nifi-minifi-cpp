package org.apache.nifi.processor;

import org.apache.nifi.bundle.BundleDetails;

import java.util.Collections;
import java.util.List;

/**
 * Simply defines a bundle reference within JNI.
 */
public class JniBundle {
    private BundleDetails details;
    private List<JniComponent> components;



    public JniBundle(BundleDetails details, List<JniComponent> components){
        this.details = details;
        this.components = components;
    }

    public BundleDetails getDetails(){
        return details;
    }

    public List<JniComponent> getComponents(){
        return Collections.unmodifiableList(components);
    }
}
