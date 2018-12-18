/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIBMINIFI_INCLUDE_CAPI_NANOFI_H_
#define LIBMINIFI_INCLUDE_CAPI_NANOFI_H_

#include <stddef.h>
#include <stdint.h>

#include "core/cstructs.h"
#include "core/processors.h"

int initialize_api();

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Updates with every release. Functions used here constitute the public API of NanoFi.
 *
 * Changes here will follow semver
 */
#define API_VERSION "0.02"

#define SUCCESS_RELATIONSHIP "success"
#define FAILURE_RELATIONSHIP "failure"

/**
 * Enables logging (disabled by default)
 **/
void enable_logging();

/**
 * Sets terminate callback. The callback is executed upon termination (undhandled exception in C++ backend)
 * @param terminate_callback the callback to execute
 **/
void set_terminate_callback(void (*terminate_callback)());

/****
 * ##################################################################
 *  BASE NIFI OPERATIONS
 * ##################################################################
 */

/**
 * Creates a new MiNiFi instance
 * @param url remote URL the instance connects to
 * @param port remote port the instance connects to
 * @return pointer to the new instance
 **/
nifi_instance *create_instance(const char *url, nifi_port *port);

/**
 * Initialize remote connection of instance for transfers
 * @param instance
 **/
void initialize_instance(nifi_instance * instance);

/**
 * Frees instance
 * @attention Any action on flows that belong to the freed instance are undefined after this is done!
 * It's recommended to free all flows before freeing the instance.
 * @param instance instance to be freed
 **/
void free_instance(nifi_instance * instance);

/****
 * ##################################################################
 *  C2 OPERATIONS
 * ##################################################################
 */


typedef int c2_update_callback(char *);

typedef int c2_stop_callback(char *);

typedef int c2_start_callback(char *);

void enable_async_c2(nifi_instance *, C2_Server *, c2_stop_callback *, c2_start_callback *, c2_update_callback *);

/**
 * Creates a new, empty flow
 * @param instance the instance new flow will belong to
 * @return a pointer to the created flow
 **/
flow *create_new_flow(nifi_instance * instance);


/**
 * Creates new flow and adds the first processor in case a valid name is provided
 * @deprecated  as there is no proper indication of processor adding errors,
 * usage of "create_new_flow" and "add_processor is recommended instead
 * @param instance the instance new flow will belong to
 * @param first_processor name of the first processor to be instanciated
 * @attention in case first processor is empty or doesn't name any existing processor, an empty flow is returned.
 * @return a pointer to the created flow
 **/
DEPRECATED flow *create_flow(nifi_instance * instance, const char * first_processor);

/**
 * Add a getfile processor to "parent" flow.
 * Creates new flow in instance in case "parent" is nullptr
 * @param instance the instance the flow belongs to
 * @param parent the flow to be extended with a new getfile processor
 * @param c configuration of the new processor
 * @return parent in case it wasn't null, otherwise a pointer to a new flow
 */
flow *create_getfile(nifi_instance *instance, flow *parent, GetFileConfig *c);

/**
 * Extend a flow with a new processor
 * @param flow the flow to be extended with the new processor
 * @param name name of the new processor
 * @return pointer to the new processor or nullptr in case it cannot be instantiated (wrong name?)
 **/
processor *add_processor(flow * flow, const char * name);

processor *add_python_processor(flow *, processor_logic* logic);

/**
 * Create a standalone instance of the given processor.
 * Standalone instances can be invoked without having an instance/flow that contains them.
 * @param name the name of the processor to instanciate
 * @return pointer to the new processor or nullptr in case it cannot be instantiated (wrong name?)
 **/
standalone_processor *create_processor(const char * name);

/**
 * Free a standalone processor
 * @param processor the processor to be freed
 */
void free_standalone_processor(standalone_processor* processor);

/**
 * Register your callback to received flow files that the flow failed to process
 * @attention The flow file ownership is transferred to the callback!
 * @attention The first callback should be registered before the flow is used. Can be changed later during runtime.
 * @param flow flow the callback belongs to
 * @param onerror_callback callback to execute in case of failure
 * @return 0 in case of success, -1 otherwise (flow is already in use)
 **/
int add_failure_callback(flow *flow, void (*onerror_callback)(flow_file_record*));

/**
 * Set failure strategy. Please use the enum defined in cstructs.h
 * Can be changed runtime.
 * The default strategy is AS IS.
 * @param flow the flow to set strategy for
 * @param strategy the strategy to be set
 * @return 0 (success), -1 (strategy cannot be set - no failure callback added?)
 **/
int set_failure_strategy(flow *flow, FailureStrategy strategy);

/**
 * Set property for a processor
 * @param processor the processor the property is set for
 * @param name name of the property
 * @param value value of the property
 * @return 0 in case of success, -1 otherwise (the processor doesn't support such property)
 **/
int set_property(processor * processor, const char * name, const char * value);

/**
 * Set property for a standalone processor
 * @param processor the processor the property is set for
 * @param name name of the property
 * @param value value of the property
 * @return 0 in case of success, -1 otherwise (the processor doesn't support such property)
 **/
int set_standalone_property(standalone_processor * processor, const char * name, const char * value);

/**
 * Set property for an instance
 * @param instance the instance the property is set for
 * @param name name of the property
 * @param value value of the property
 * @return 0 in case of success, -1 otherwise. Always succeeds unless instance or name is nullptr/emtpy.
 **/
int set_instance_property(nifi_instance *instance, const char * name, const char * value);

/**
 * Get a property. Should be used in custom processor logic callbacks.
 * Writes the value of the property to the buffer.
 * Nothing is written to the buffer in case the property is not found (return value != 0)
 * The result is always null-terminated, at most size-1 characters are written to the buffer.
 * @param context the current processor context
 * @param name name of the property
 * @param buffer buffer to write the value of the property
 * @param size size of the buffer
 * @return 0 in case of success (property found), -1 otherwise
 **/
uint8_t get_property(const processor_context * context, const char * name, char * buffer, size_t size);

/**
 * Free a flow
 * @param flow the flow to free
 * @attention All the processor in the flow are freed, too! Actions performed on freed processors are undefined!
 * @return 0 in case of success, -1 otherwise. Always succeeds unless flow is nullptr.
 **/
int free_flow(flow * flow);

/**
 * Get the next flow file of the given flow
 * @param instance the instance the flow belongs to
 * @param flow the flow to get flowfile from
 * @return a flow file record or nullptr in case no flowfile was generated by the flow
 **/
flow_file_record *get_next_flow_file(nifi_instance *, flow *);

/**
 * Get all flow files of the given flow
 * @param instance the instance the flow belongs to
 * @param flow the flow to get flowfiles from
 * @param flowfiles target area to copy the flowfiles to
 * @param size the maximum number of flowfiles to copy to target (size of target)
 * @return the number of flow files copies to target. Less or equal to size.
 **/
size_t get_flow_files(nifi_instance * instance, flow * flow, flow_file_record ** flowfiles, size_t size);

/**
 * Invoke a standalone processor without input data.
 * The processor is expected to generate flow file.
 * @return a flow file record or nullptr in case no flowfile was generated
 **/
flow_file_record *invoke(standalone_processor* proc);

/**
 * Invoke a standalone processor with input flow file
 * @param input_ff input flow file, which can belong be the output of another processor or flow
 * @return a flow file record or nullptr in case no flowfile was generated
 **/
flow_file_record *invoke_ff(standalone_processor* proc, const flow_file_record *input_ff);

/**
 * Invoke a standalone processor with file input
 * @param path specifies the file system path of the input file
 * @return a flow file record or nullptr in case no flowfile was generated
 **/
flow_file_record *invoke_file(standalone_processor* proc, const char* path);

/**
 * Invoke a standalone processor with some in-memory data
 * @param buf specifies the beginning of the input buffer
 * @param size specifies the size of the buffer
 * @return a flow file record or nullptr in case no flowfile was generated
 **/
flow_file_record *invoke_chunk(standalone_processor *proc, uint8_t *buf, uint64_t size);

int transfer(processor_session* session, flow *flow, const char *rel);

/**
 * Creates a flow file record based on a file
 * @param file source file
 * @param len length of the file name
 * @return a flow file record or nullptr in case no flowfile was generated
 */
flow_file_record* create_flowfile(const char *file, const size_t len);

/**
 * Creates a flow file record based on a file
 * @param file source file
 * @param len length of the file name
 * @param size size of the file
 * @return a flow file record or nullptr in case no flowfile was generated
 */
flow_file_record* create_ff_object(const char *file, const size_t len, const uint64_t size);

/**
 * Creates a flow file record based on a file, without attributes
 * @attention attributes cannot be added later!
 * @param file source file
 * @param len length of the file name
 * @param size size of the file
 * @return a flow file record or nullptr in case no flowfile was generated
 */
flow_file_record* create_ff_object_na(const char *file, const size_t len, const uint64_t size);

/**
 * Creates a flow file record without content. Only attributes can be added.
 * @attention content cannot be added later!
 * @return a flow file record or nullptr in case no flowfile was generated
 */
flow_file_record* create_ff_object_nc();

/**
 * Get incoming flow file. To be used in processor logic callbacks.
 * @param session current processor session
 * @param context current processor context
 * @return a flow file record or nullptr in case there is none in the session
 **/
flow_file_record* get(processor_session *session, processor_context *context);


/**
 * Free flow file
 * @param ff flow file
 **/
void free_flowfile(flow_file_record* ff);

/**
 * Adds an attribute, fails in case there is already an attribute with the given key.
 * @param ff flow file
 * @param key name of attribute
 * @param value location of value
 * @size size size of the data pointed by "value"
 * @return 0 in case of success, -1 otherwise (already existed)
 **/
int8_t add_attribute(flow_file_record*, const char *key, void *value, size_t size);

/**
 * Updates an attribute (adds if it hasn't existed before)
 * @param ff flow file
 * @param key name of attribute
 * @param value location of value
 * @size size size of the data pointed by "value"
 **/
void update_attribute(flow_file_record* ff, const char *key, void *value, size_t size);

/**
 * Get the value of an attribute. Value and value size are written to parameter "caller_attribute"
 * @param ff flow file
 * @param caller_attribute attribute structure to provide name and get value, size
 * @return 0 in case of success, -1 otherwise (no such attribute)
 **/
int8_t get_attribute(const flow_file_record *ff, attribute *caller_attribute);

/**
 * Get the quantity of attributes
 * @param ff flow file
 * @return the number of attributes
 **/
int get_attribute_quantity(const flow_file_record *ff);


/**
 * Copies all attributes of the flowfile that fits target.
 * @param ff flow file
 * @param target attribute set to copy to. target->size determines the maximum number of attributes copied
 * @return the number of attributes copied, which is the minimum of attribute quantity and target size
 **/
int get_all_attributes(const flow_file_record* ff, attribute_set *target);

/**
 * reads the content of a flow file
 * @param target reference in which will set the result
 * @param size max number of bytes to read (use flow_file_record->size to get the whole content)
 * @return resulting read size (<=size)
 **/
int get_content(const flow_file_record* ff, uint8_t* target, int size);

/**
 * Removes an attribute
 * @param name name of the attribute
 * @return 0 on success, -1 otherwise (doesn't exist)
 **/
int8_t remove_attribute(flow_file_record*, const char * key);

/****
 * ##################################################################
 *  Remote NIFI OPERATIONS
 * ##################################################################
 */

int transmit_flowfile(flow_file_record *, nifi_instance *);

/**
 * Adds a custom processor for later instantiation
 * @param name name of the processor
 * @param logic the callback to be invoked when the processor is triggered
 * @return 0 on success, -1 otherwise (name already in use for eg.)
 **/
int add_custom_processor(const char * name, processor_logic* logic);

/**
 * Removes a custom processor
 * @param name name of the processor
 * @return 0 on success, -1 otherwise (didn't exist)
 **/
int delete_custom_processor(const char * name);

/**
 * Transfers a flowfile to the given relationship
 * This function is only to be used within processor logic callback
 * @param ffr flow file to be transfered
 * @param ps processor session the transfer happens within
 * @param relationship name of the relationship ("success" and "failure" are supported currently)
 * @return 0 on success, -1 otherwise (didn't exist)
 **/
int transfer_to_relationship(flow_file_record * ffr, processor_session * ps, const char * relationship);

/****
 * ##################################################################
 *  Persistence Operations
 * ##################################################################
 */


#ifdef __cplusplus
}
#endif

#endif /* LIBMINIFI_INCLUDE_CAPI_NANOFI_H_ */
