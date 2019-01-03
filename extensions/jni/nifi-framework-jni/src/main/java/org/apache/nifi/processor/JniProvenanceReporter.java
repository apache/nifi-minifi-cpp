package org.apache.nifi.processor;

import org.apache.nifi.flowfile.FlowFile;
import org.apache.nifi.provenance.ProvenanceReporter;

import java.util.Collection;

public class JniProvenanceReporter implements ProvenanceReporter {
    @Override
    public void receive(FlowFile flowFile, String transitUri) {

    }

    @Override
    public void receive(FlowFile flowFile, String transitUri, String sourceSystemFlowFileIdentifier) {

    }

    @Override
    public void receive(FlowFile flowFile, String transitUri, long transmissionMillis) {

    }

    @Override
    public void receive(FlowFile flowFile, String transitUri, String details, long transmissionMillis) {

    }

    @Override
    public void receive(FlowFile flowFile, String transitUri, String sourceSystemFlowFileIdentifier, String details, long transmissionMillis) {

    }

    @Override
    public void fetch(FlowFile flowFile, String transitUri) {

    }

    @Override
    public void fetch(FlowFile flowFile, String transitUri, long transmissionMillis) {

    }

    @Override
    public void fetch(FlowFile flowFile, String transitUri, String details, long transmissionMillis) {

    }

    @Override
    public void send(FlowFile flowFile, String transitUri) {

    }

    @Override
    public void send(FlowFile flowFile, String transitUri, String details) {

    }

    @Override
    public void send(FlowFile flowFile, String transitUri, long transmissionMillis) {

    }

    @Override
    public void send(FlowFile flowFile, String transitUri, String details, long transmissionMillis) {

    }

    @Override
    public void send(FlowFile flowFile, String transitUri, boolean force) {

    }

    @Override
    public void send(FlowFile flowFile, String transitUri, String details, boolean force) {

    }

    @Override
    public void send(FlowFile flowFile, String transitUri, long transmissionMillis, boolean force) {

    }

    @Override
    public void send(FlowFile flowFile, String transitUri, String details, long transmissionMillis, boolean force) {

    }

    @Override
    public void invokeRemoteProcess(FlowFile flowFile, String transitUri) {

    }

    @Override
    public void invokeRemoteProcess(FlowFile flowFile, String transitUri, String details) {

    }

    @Override
    public void associate(FlowFile flowFile, String alternateIdentifierNamespace, String alternateIdentifier) {

    }

    @Override
    public void fork(FlowFile parent, Collection<FlowFile> children) {

    }

    @Override
    public void fork(FlowFile parent, Collection<FlowFile> children, String details) {

    }

    @Override
    public void fork(FlowFile parent, Collection<FlowFile> children, long forkDuration) {

    }

    @Override
    public void fork(FlowFile parent, Collection<FlowFile> children, String details, long forkDuration) {

    }

    @Override
    public void join(Collection<FlowFile> parents, FlowFile child) {

    }

    @Override
    public void join(Collection<FlowFile> parents, FlowFile child, String details) {

    }

    @Override
    public void join(Collection<FlowFile> parents, FlowFile child, long joinDuration) {

    }

    @Override
    public void join(Collection<FlowFile> parents, FlowFile child, String details, long joinDuration) {

    }

    @Override
    public void clone(FlowFile parent, FlowFile child) {

    }

    @Override
    public void modifyContent(FlowFile flowFile) {

    }

    @Override
    public void modifyContent(FlowFile flowFile, String details) {

    }

    @Override
    public void modifyContent(FlowFile flowFile, long processingMillis) {

    }

    @Override
    public void modifyContent(FlowFile flowFile, String details, long processingMillis) {

    }

    @Override
    public void modifyAttributes(FlowFile flowFile) {

    }

    @Override
    public void modifyAttributes(FlowFile flowFile, String details) {

    }

    @Override
    public void route(FlowFile flowFile, Relationship relationship) {

    }

    @Override
    public void route(FlowFile flowFile, Relationship relationship, String details) {

    }

    @Override
    public void route(FlowFile flowFile, Relationship relationship, long processingDuration) {

    }

    @Override
    public void route(FlowFile flowFile, Relationship relationship, String details, long processingDuration) {

    }

    @Override
    public void create(FlowFile flowFile) {

    }

    @Override
    public void create(FlowFile flowFile, String details) {

    }
}
