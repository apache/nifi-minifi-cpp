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

#include <ecu_api/ecuapi.h>
#include <coap/c2payload.h>
#include <stdio.h>

typedef enum {
    TYPE,
    NAME,
    INPUT,
    OUTPUT,
    IDENTIFIER,
    DISPLAYNAME,
    DESCRIPTION,
    VALIDATOR,
    SENSITIVE,
    DYNAMIC,
    REQUIRED,
    PROPERTIES,
    VERSION,
    STATUS,
    DEVICEINFO,
    SYSTEMFINO,
    MACHINEARCH,
    VCORES,
    PHYSICALMEMORYBYTES,
    NETWORKINFO,
    HOSTNAME,
    IPADDRESS,
    AGENTINFO,
    AGENTCLASS,
    UPTIME,
    AGENTMANIFEST,
    AGENTTYPE,
    IOMANIFEST,
    PROPERTYDESCRIPTORS,
    ECUINFO,
    OPERATION,
    OPERAND,
    FILEPATH,
    CHUNKSIZEBYTES,
    DELIMITER,
    TAILFREQMILLISECONDS,
    TCPPORT,
    NIFIPORT,
    REQUESTEDOPERATIONS
} c2_keys_t;

value_t value_uint8(uint8_t value) {
    value_t val;
    memset(&val, 0, sizeof(value_t));
    val.v_uint8 = value;
    val.val_type = UINT8_TYPE;
    return val;
}

value_t value_uint16(uint16_t value) {
    value_t val;
    memset(&val, 0, sizeof(value_t));
    val.v_uint16 = value;
    val.val_type = UINT16_TYPE;
    return val;
}

value_t value_uint32(uint32_t value) {
    value_t val;
    memset(&val, 0, sizeof(value_t));
    val.v_uint32 = value;
    val.val_type = UINT32_TYPE;
    return val;
}

value_t value_uint64(uint64_t value) {
    value_t val;
    memset(&val, 0, sizeof(value_t));
    val.v_uint64 = value;
    val.val_type = UINT64_TYPE;
    return val;
}

value_t value_string(const char * value) {
    value_t val;
    memset(&val, 0, sizeof(value_t));
    copystr(value, &val.v_str);
    val.val_type = STRING_TYPE;
    return val;
}

value_t value_nstring(const unsigned char * value, size_t len) {
    value_t val;
    memset(&val, 0, sizeof(value_t));
    val.v_str = (char *)malloc(len + 1);
    memset(val.v_str, 0, len + 1);
    copynstr(value, len, val.v_str);
    val.val_type = STRING_TYPE;
    return val;
}

value_t value_map(c2_payload_map_t * value) {
    value_t val;
    memset(&val, 0, sizeof(value_t));
    val.v_map = value;
    val.val_type = HASH_TYPE;
    return val;
}

value_t value_list(c2_payload_list_t * value) {
    value_t val;
    memset(&val, 0, sizeof(value_t));
    val.v_maplist = value;
    val.val_type = LIST_TYPE;
    return val;
}

value_t value_property(properties_t * props) {
    value_t val;
    memset(&val, 0, sizeof(value_t));
    val.v_props = clone_properties(props);
    val.val_type = PROP_TYPE;
    return val;
}

value_t value_none() {
    value_t val;
    memset(&val, 0, sizeof(value_t));
    val.val_type = NONE_TYPE;
    return val;
}

int is_value_none(value_t val) {
    return val.val_type == NONE_TYPE;
}

int is_value_uint(value_t val) {
    return val.val_type == UINT8_TYPE ||
            val.val_type == UINT16_TYPE ||
            val.val_type == UINT32_TYPE ||
            val.val_type == UINT64_TYPE;
}

int is_value_string(value_t val) {
    return val.val_type == STRING_TYPE;
}

int is_value_map(value_t val) {
    return val.val_type == HASH_TYPE;
}

int is_value_list(value_t val) {
    return val.val_type == LIST_TYPE;
}

c2_payload_map_t * allocate_c2_payload() {
    c2_payload_map_t * map = (c2_payload_map_t *)malloc(sizeof(c2_payload_map_t));
    memset(map, 0, sizeof(c2_payload_map_t));
    return map;
}

c2_payload_map_t * add_kvp(c2_payload_map_t ** map, uint16_t key, value_t value) {
    c2_payload_map_t * field = allocate_c2_payload();
    field->key = key;
    field->value = value;
    HASH_ADD_INT((*map), key, field);
    return field;
}

c2_payload_list_t * add_list(c2_payload_list_t ** list, value_t value) {
    if (!list) return NULL;
    c2_payload_list_t * node = (c2_payload_list_t *)malloc(sizeof(c2_payload_list_t));
    memset(node, 0, sizeof(c2_payload_list_t));
    node->value = value;
    LL_APPEND((*list), node);
    return node;
}

void add_system_info(c2_payload_map_t ** dev_map, uint16_t key, const systeminfo * const sysinfo) {
    if (!sysinfo || !dev_map) return;
    c2_payload_map_t * kvp = add_kvp(dev_map, key, value_map(NULL));
    c2_payload_map_t ** sub_map = &kvp->value.v_map;
    add_kvp(sub_map, MACHINEARCH, value_string(sysinfo->machine_arch));
    add_kvp(sub_map, VCORES, value_uint16(sysinfo->v_cores));
    add_kvp(sub_map, PHYSICALMEMORYBYTES, value_uint64(sysinfo->physical_mem));
}

void add_network_info(c2_payload_map_t ** dev_map, uint16_t key, const networkinfo * const netinfo) {
    if (!netinfo || !dev_map) return;
    c2_payload_map_t * kvp = add_kvp(dev_map, key, value_map(NULL));
    c2_payload_map_t ** sub_map = &kvp->value.v_map;
    add_kvp(sub_map, IDENTIFIER, value_string(netinfo->device_id));
    add_kvp(sub_map, HOSTNAME, value_nstring(netinfo->host_name, sizeof(netinfo->host_name)));
    add_kvp(sub_map, IPADDRESS, value_nstring(netinfo->ip_address, sizeof(netinfo->ip_address)));
}

void add_device_info(c2_payload_map_t ** c2payload, uint16_t key, const deviceinfo * const devinfo) {
    if (!devinfo || !c2payload) return;
    c2_payload_map_t * kvp = add_kvp(c2payload, key, value_map(NULL));
    c2_payload_map_t ** sub_map = &kvp->value.v_map;
    add_kvp(sub_map, IDENTIFIER, value_string(devinfo->ident));
    add_system_info(sub_map, SYSTEMFINO, &devinfo->system_info);
    add_network_info(sub_map, NETWORKINFO, &devinfo->network_info);
}

void prepare_property_desc(int num_props, c2_payload_map_t ** prop_map, io_descriptor descr) {
    c2_payload_map_t * prop_desc_kvp = add_kvp(prop_map, PROPERTYDESCRIPTORS, value_list(NULL));
    int i;
    for (i = 0; i < num_props; ++i) {
        c2_payload_map_t * prop_desc = NULL;
        add_kvp(&prop_desc, NAME, value_string(descr.prop_descrs[i].property_name));
        add_kvp(&prop_desc, DISPLAYNAME, value_string(descr.prop_descrs[i].display_name));
        add_kvp(&prop_desc, DESCRIPTION, value_string(descr.prop_descrs[i].description));
        add_kvp(&prop_desc, REQUIRED, value_uint8(descr.prop_descrs[i].required));
        add_kvp(&prop_desc, SENSITIVE, value_uint8(descr.prop_descrs[i].sensitive));
        add_kvp(&prop_desc, DYNAMIC, value_uint8(descr.prop_descrs[i].dynamic));
        add_kvp(&prop_desc, VALIDATOR, value_string(descr.prop_descrs[i].validator));
        add_list(&prop_desc_kvp->value.v_maplist, value_map(prop_desc));
    }
}

void add_agent_manifest(c2_payload_map_t ** aginfo_map, uint16_t key, const agent_manifest * const ag_mnfst) {
    c2_payload_map_t * ag_mnfst_kvp = add_kvp(aginfo_map, key, value_map(NULL));
    c2_payload_map_t ** ag_mnfst_map = &ag_mnfst_kvp->value.v_map;
    add_kvp(ag_mnfst_map, IDENTIFIER, value_nstring(ag_mnfst->manifest_id, sizeof(ag_mnfst->manifest_id)));
    add_kvp(ag_mnfst_map, AGENTTYPE, value_nstring(ag_mnfst->agent_type, sizeof(ag_mnfst->agent_type)));
    add_kvp(ag_mnfst_map, VERSION, value_string(ag_mnfst->version));

    c2_payload_map_t * io_mnfst_kvp = add_kvp(ag_mnfst_map, IOMANIFEST, value_map(NULL));
    c2_payload_map_t ** io_mnfst_map = &io_mnfst_kvp->value.v_map;

    c2_payload_map_t * input_kvp = add_kvp(io_mnfst_map, INPUT, value_list(NULL));
    int i;
    for(i = 0; i < ag_mnfst->io.num_ips; ++i) {
        c2_payload_map_t * ip = NULL;
        add_kvp(&ip, NAME, value_string(ag_mnfst->io.input_descrs[i].name));
        uint8_t num_props = ag_mnfst->io.input_descrs[i].num_props;
        prepare_property_desc(num_props, &ip, ag_mnfst->io.input_descrs[i]);
        add_list(&input_kvp->value.v_maplist, value_map(ip));
    }

    c2_payload_map_t * output_kvp = add_kvp(io_mnfst_map, OUTPUT, value_list(NULL));
    for(i = 0; i < ag_mnfst->io.num_ops; ++i) {
        c2_payload_map_t * op = NULL;
        add_kvp(&op, NAME, value_string(ag_mnfst->io.output_descrs[i].name));
        uint8_t num_props = ag_mnfst->io.output_descrs[i].num_props;
        prepare_property_desc(num_props, &op, ag_mnfst->io.output_descrs[i]);
        add_list(&output_kvp->value.v_maplist, value_map(op));
    }

    if (ag_mnfst->ecus) {
        c2_payload_map_t * ecu_info_kvp = add_kvp(ag_mnfst_map, ECUINFO, value_list(NULL));
        c2_payload_list_t ** ecu_info_list = &ecu_info_kvp->value.v_maplist;
        int i;
        for (i = 0; i < ag_mnfst->num_ecus; ++i) {
            c2_payload_map_t * ecu_info = NULL;
            add_kvp(&ecu_info, NAME, value_string(ag_mnfst->ecus[i].name));
            add_kvp(&ecu_info, IDENTIFIER, value_nstring(ag_mnfst->ecus[i].uuid, sizeof(ag_mnfst->ecus[i].uuid)));

            //input properties
            c2_payload_map_t * ip_kvp = add_kvp(&ecu_info, INPUT, value_map(NULL));
            c2_payload_map_t ** ip_map = &ip_kvp->value.v_map;
            add_kvp(ip_map, NAME, value_string(ag_mnfst->ecus[i].input));
            properties_t * ip_args = ag_mnfst->ecus[i].ip_args;
            add_kvp(ip_map, PROPERTIES, value_property(ip_args));

            //output properties
            c2_payload_map_t * op_kvp = add_kvp(&ecu_info, OUTPUT, value_map(NULL));
            c2_payload_map_t ** op_map = &op_kvp->value.v_map;
            add_kvp(op_map, NAME, value_string(ag_mnfst->ecus[i].output));
            properties_t * op_args = ag_mnfst->ecus[i].op_args;
            add_kvp(op_map, PROPERTIES, value_property(op_args));

            add_list(ecu_info_list, value_map(ecu_info));
        }
    }
}

void add_agent_info(c2_payload_map_t ** c2payload, uint16_t key, const agentinfo * const aginfo, const agent_manifest * const ag_mnfst) {
    if (!aginfo || !c2payload) {
        return;
    }
    c2_payload_map_t * kvp = add_kvp(c2payload, key, value_map(NULL));
    add_kvp(&kvp->value.v_map, IDENTIFIER, value_nstring(aginfo->ident, sizeof(aginfo->ident)));
    add_kvp(&kvp->value.v_map, AGENTCLASS, value_string(aginfo->agent_class));
    c2_payload_map_t * status_kvp = add_kvp(&kvp->value.v_map, STATUS, value_map(NULL));
    c2_payload_map_t ** status_sub_map = &status_kvp->value.v_map;
    add_kvp(status_sub_map, UPTIME, value_uint64(aginfo->uptime));
    if (ag_mnfst) {
        add_agent_manifest(&kvp->value.v_map, AGENTMANIFEST, ag_mnfst);
    }
}

c2_payload_map_t * c2_payload_heartbeat(c2heartbeat_t hb) {
    c2_payload_map_t * c2payload = NULL;
    add_kvp(&c2payload, TYPE, value_uint8(1));
    add_device_info(&c2payload, DEVICEINFO, &hb.device_info);
    hb.has_ag_manifest ? add_agent_info(&c2payload, AGENTINFO, &hb.agent_info, &hb.ag_manifest)
            : add_agent_info(&c2payload, AGENTINFO, &hb.agent_info, NULL);
    return c2payload;
}

c2_payload_map_t * c2_payload_server_response(c2_server_response_t * sp) {
    c2_payload_map_t * c2payload = NULL;
    c2_payload_map_t * kvp = add_kvp(&c2payload, REQUESTEDOPERATIONS, value_list(NULL));
    c2_payload_list_t ** req_list = &kvp->value.v_maplist;

    c2_server_response_t * el;
    LL_FOREACH(sp, el) {
        c2_payload_map_t * map = NULL;
        add_kvp(&map, IDENTIFIER, value_string(sp->ident));
        add_kvp(&map, OPERATION, value_uint8(sp->operation));
        add_kvp(&map, OPERAND, value_string(sp->operand));
        add_kvp(&map, PROPERTIES, value_property(sp->args));
        add_list(req_list, value_map(map));
    }
    return c2payload;
}

c2_payload_map_t * c2_payload_agent_response(c2_response_t * ap) {
    c2_payload_map_t * c2payload = NULL;
    add_kvp(&c2payload, TYPE, value_uint8(0));
    add_kvp(&c2payload, OPERATION, value_uint8(ACKNOWLEDGE));
    add_kvp(&c2payload, IDENTIFIER, value_string(ap->ident));
    return c2payload;
}

void free_value(value_t value) {
    switch (value.val_type) {
    case STRING_TYPE: {
        free(value.v_str);
        break;
    }
    case HASH_TYPE: {
        struct c2_payload_map * el, *tmp;
        HASH_ITER(hh, value.v_map, el, tmp) {
            free_value(el->value);
            HASH_DEL(value.v_map, el);
            free(el);
        }
        break;
    }
    case LIST_TYPE: {
        c2_payload_list_t * el, *tmp;
        LL_FOREACH_SAFE(value.v_maplist, el, tmp) {
            free_value(el->value);
            LL_DELETE(value.v_maplist, el);
            free(el);
        }
        break;
    }
    case PROP_TYPE: {
        free_properties(value.v_props);
        break;
    }
    }
}

void free_c2_payload(c2_payload_map_t * c2payload) {
    c2_payload_map_t * el, *tmp;
    HASH_ITER(hh, c2payload, el, tmp) {
        free_value(el->value);
        HASH_DEL(c2payload, el);
        free(el);
    }
}
value_t c2_payload_map_get(uint16_t key_val, const c2_payload_map_t * c2_payload) {
    c2_payload_map_t * el;
    int k = key_val;
    HASH_FIND_INT(c2_payload, &k, el);
    if (el) {
        return el->value;
    }
    return value_none();
}

int extract_property_descriptor(const c2_payload_map_t * c2_payload, io_descriptor * io_desc) {
    value_t name_val = c2_payload_map_get(NAME, c2_payload);
    value_t disp_val = c2_payload_map_get(DISPLAYNAME, c2_payload);
    value_t desc_val = c2_payload_map_get(DESCRIPTION, c2_payload);
    value_t vald_val = c2_payload_map_get(VALIDATOR, c2_payload);
    value_t reqr_val = c2_payload_map_get(REQUIRED, c2_payload);
    value_t sens_val = c2_payload_map_get(SENSITIVE, c2_payload);
    value_t dynm_val = c2_payload_map_get(DYNAMIC, c2_payload);
    if (!is_value_string(name_val)
        || !is_value_string(disp_val)
        || !is_value_string(desc_val)
        || !is_value_string(vald_val)
        || !is_value_uint(reqr_val)
        || !is_value_uint(sens_val)
        || !is_value_uint(dynm_val)) {
        return -1;
    }
    io_desc->prop_descrs = (property_descriptor *)malloc(sizeof(property_descriptor));
    copystr(name_val.v_str, &io_desc->prop_descrs->property_name);
    copystr(disp_val.v_str, &io_desc->prop_descrs->display_name);
    copystr(desc_val.v_str, &io_desc->prop_descrs->description);
    copystr(vald_val.v_str, &io_desc->prop_descrs->validator);
    io_desc->prop_descrs->required = reqr_val.v_uint8;
    io_desc->prop_descrs->sensitive = sens_val.v_uint8;
    io_desc->prop_descrs->dynamic = dynm_val.v_uint8;
    return 0;
}

int extract_input_manifest(const c2_payload_list_t * inputs, io_manifest * io_mnfst) {
    c2_payload_list_t * ip;
    LL_FOREACH(inputs, ip) {
        if (!is_value_map(ip->value)) {
            return -1;
        }
        value_t name_val = c2_payload_map_get(NAME, ip->value.v_map);
        value_t props_val = c2_payload_map_get(PROPERTYDESCRIPTORS, ip->value.v_map);
        if (!is_value_string(name_val)
            || !is_value_list(props_val)) {
            return -1;
        }

        io_mnfst->input_descrs = (io_descriptor *)malloc(sizeof(io_descriptor));
        copystr(name_val.v_str, &io_mnfst->input_descrs->name);
        c2_payload_list_t * prop;
        LL_FOREACH(props_val.v_maplist, prop) {
            if (!is_value_map(prop->value)) return -1;
            if (extract_property_descriptor(prop->value.v_map, io_mnfst->input_descrs) < 0) {
                return -1;
            }
        }
    }
    return 0;
}

int extract_output_manifest(const c2_payload_list_t * outputs, io_manifest * io_mnfst) {
    c2_payload_list_t * op;
    LL_FOREACH(outputs, op) {
        if (!is_value_map(op->value)) {
            return -1;
        }
        value_t name_val = c2_payload_map_get(NAME, op->value.v_map);
        value_t props_val = c2_payload_map_get(PROPERTYDESCRIPTORS, op->value.v_map);
        if (!is_value_string(name_val)
            || !is_value_list(props_val)) {
            return -1;
        }

        io_mnfst->output_descrs = (io_descriptor *)malloc(sizeof(io_descriptor));
        copystr(name_val.v_str, &io_mnfst->output_descrs->name);
        c2_payload_list_t * prop;
        LL_FOREACH(props_val.v_maplist, prop) {
            if (!is_value_map(prop->value)) return -1;
            if (extract_property_descriptor(prop->value.v_map, io_mnfst->output_descrs) < 0) {
                return -1;
            }
        }
    }
    return 0;
}

int extract_io_manifest(const c2_payload_map_t * c2_payload, io_manifest * io_mnfst) {
    value_t input_val = c2_payload_map_get(INPUT, c2_payload);
    value_t output_val = c2_payload_map_get(OUTPUT, c2_payload);
    if (!is_value_list(input_val)
        || !is_value_list(output_val)
        || extract_input_manifest(input_val.v_maplist, io_mnfst) < 0
        || extract_output_manifest(output_val.v_maplist, io_mnfst) < 0) {
        return -1;
    }
    return 0;
}

int extract_ecu_info(const c2_payload_map_t * c2_payload, ecuinfo * ecu_info) {
    return 0;
}

int extract_agent_manifest(const c2_payload_map_t * c2_payload, agent_manifest * ag_mnfst) {
    value_t id_val = c2_payload_map_get(IDENTIFIER, c2_payload);
    value_t agtype_val = c2_payload_map_get(AGENTTYPE, c2_payload);
    value_t version_val = c2_payload_map_get(VERSION, c2_payload);
    value_t iomnfst_val = c2_payload_map_get(IOMANIFEST, c2_payload);
    value_t ecuinfo_val = c2_payload_map_get(ECUINFO, c2_payload);
    if (!is_value_string(id_val)
        || !is_value_string(agtype_val)
        || !is_value_string(version_val)
        || !is_value_map(iomnfst_val)
        || !is_value_map(ecuinfo_val)
        || extract_io_manifest(iomnfst_val.v_map, &ag_mnfst->io) < 0
        || extract_ecu_info(ecuinfo_val.v_map, ag_mnfst->ecus) < 0) {
        return -1;
    }
    return 0;
}

int extract_agent_info(const c2_payload_map_t * c2_payload, agentinfo * ag_info) {
    value_t id_val = c2_payload_map_get(IDENTIFIER, c2_payload);
    value_t agclass_val = c2_payload_map_get(AGENTCLASS, c2_payload);
    value_t status_val = c2_payload_map_get(STATUS, c2_payload);
    if (!is_value_string(id_val)
        || !is_value_string(agclass_val)
        || !is_value_map(status_val)) {
        return -1;
    }
    value_t uptime_val = c2_payload_map_get(UPTIME, status_val.v_map);
    if (!is_value_uint(uptime_val)) {
        return -1;
    }
    copynstr(id_val.v_str, strlen(id_val.v_str), ag_info->ident);
    copystr(agclass_val.v_str, &ag_info->agent_class);
    ag_info->uptime = uptime_val.v_uint64;
    return 0;
}

int extract_net_info(const c2_payload_map_t * c2_payload, networkinfo * net_info) {
    value_t id_val = c2_payload_map_get(IDENTIFIER, c2_payload);
    value_t host_val = c2_payload_map_get(HOSTNAME, c2_payload);
    value_t ip_val = c2_payload_map_get(IPADDRESS, c2_payload);

    if (!is_value_string(id_val)
        || !is_value_string(host_val)
        || !is_value_string(ip_val)) {
        return -1;
    }
    copystr(id_val.v_str, &net_info->device_id);
    copynstr(host_val.v_str, strlen(host_val.v_str), net_info->host_name);
    copynstr(ip_val.v_str, strlen(ip_val.v_str), net_info->ip_address);
    return 0;
}

int extract_sys_info(const c2_payload_map_t * c2_payload, systeminfo * sys_info) {
    value_t arch_val = c2_payload_map_get(MACHINEARCH, c2_payload);
    value_t vcores_val = c2_payload_map_get(VCORES, c2_payload);
    value_t phymem_val = c2_payload_map_get(PHYSICALMEMORYBYTES, c2_payload);

    if (!is_value_string(arch_val)
        || !is_value_uint(vcores_val)
        || !is_value_uint(phymem_val)) {
        return -1;
    }
    copystr(arch_val.v_str, &sys_info->machine_arch);
    sys_info->v_cores = vcores_val.v_uint16;
    sys_info->physical_mem = phymem_val.v_uint64;
    return 0;
}

int extract_device_info(const c2_payload_map_t * c2_payload, deviceinfo * dev_info) {
    value_t id_val = c2_payload_map_get(IDENTIFIER, c2_payload);
    value_t sys_val = c2_payload_map_get(SYSTEMFINO, c2_payload);
    value_t net_val = c2_payload_map_get(NETWORKINFO, c2_payload);
    if (!is_value_string(id_val)
        || !is_value_map(sys_val)
        || !is_value_map(net_val)
        || extract_sys_info(sys_val.v_map, &dev_info->system_info) < 0
        || extract_net_info(net_val.v_map, &dev_info->network_info) < 0) {
        return -1;
    }
    copystr(id_val.v_str, &dev_info->ident);
    return 0;
}

c2heartbeat_t * extract_c2_heartbeat(const c2_payload_map_t * c2_payload) {
    if (!c2_payload) return NULL;
    value_t type_val = c2_payload_map_get(TYPE, c2_payload);
    value_t devinfo_val = c2_payload_map_get(DEVICEINFO, c2_payload);
    value_t aginfo_val = c2_payload_map_get(AGENTINFO, c2_payload);
    if (!is_value_uint(type_val)
        || type_val.v_uint8 != 1
        || !is_value_map(devinfo_val)
        || !is_value_map(aginfo_val)) {
        return NULL;
    }

    c2heartbeat_t * hb = (c2heartbeat_t *)malloc(sizeof(c2heartbeat_t));
    memset(hb, 0, sizeof(c2heartbeat_t));
    if (extract_device_info(devinfo_val.v_map, &hb->device_info) < 0
        || extract_agent_info(aginfo_val.v_map, &hb->agent_info) < 0) {
        free_c2heartbeat(hb);
        return NULL;
    }

    value_t agmnfst_val = c2_payload_map_get(AGENTMANIFEST, aginfo_val.v_map);
    if (is_value_map(agmnfst_val)
        && extract_agent_manifest(agmnfst_val.v_map, &hb->ag_manifest) == 0) {
        hb->has_ag_manifest = 1;
    }
    return hb;
}

c2_server_response_t * extract_c2_server_response(const c2_payload_map_t * c2_payload) {
    c2_server_response_t * responses = NULL;
    value_t reqop_val = c2_payload_map_get(REQUESTEDOPERATIONS, c2_payload);
    if (reqop_val.val_type != LIST_TYPE) {
        return NULL;
    }
    c2_payload_list_t * el;
    LL_FOREACH(reqop_val.v_maplist, el) {
        if (!is_value_map(el->value)) {
            return NULL;
        }
        value_t id_val = c2_payload_map_get(IDENTIFIER, el->value.v_map);
        if (!is_value_string(id_val)) {
            return NULL;
        }
        c2_server_response_t * response = (c2_server_response_t *)malloc(sizeof(c2_server_response_t));
        memset(response, 0, sizeof(c2_server_response_t));

        copystr(id_val.v_str, &response->ident);
        value_t operation_val = c2_payload_map_get(OPERATION, el->value.v_map);

        if (!is_value_uint(operation_val)) {
            free_c2_server_responses(responses);
            free_c2_server_responses(response);
            return NULL;
        }
        response->operation = get_operation(operation_val.v_uint8);
        value_t operand_val = c2_payload_map_get(OPERAND, el->value.v_map);
        if (!is_value_string(operand_val)) {
            free_c2_server_responses(responses);
            free_c2_server_responses(response);
            return NULL;
        }
        copystr(operand_val.v_str, &response->operand);

        value_t props_val = c2_payload_map_get(PROPERTIES, el->value.v_map);
        if (props_val.val_type != PROP_TYPE) {
            free_c2_server_responses(responses);
            free_c2_server_responses(response);
            return NULL;
        }
        response->args = clone_properties(props_val.v_props);
        LL_APPEND(responses, response);
    }
    return responses;
}

void print_key(uint16_t key) {
    printf("%d : ", key);
}

void print_value(value_t value) {
    switch(value.val_type) {
    case UINT8_TYPE:
        printf("{ %d }", value.v_uint8);
        break;
    case UINT16_TYPE:
        printf("{ %llu }", value.v_uint16);
        break;
    case UINT32_TYPE:
        printf("{ %llu }", value.v_uint32);
        break;
    case UINT64_TYPE:
        printf("{ %llu }", value.v_uint64);
        break;
    case STRING_TYPE:
        printf("{ %s }", value.v_str);
        break;
    case HASH_TYPE: {
        c2_payload_map_t * el, *tmp;
        HASH_ITER(hh, value.v_map, el, tmp) {
            print_key(el->key);
            print_value(el->value);
            printf("\n");
        }
        break;
    }
    case LIST_TYPE: {
        c2_payload_list_t * el;
        LL_FOREACH(value.v_maplist, el) {
            print_value(el->value);
            printf(" , ");
        }
        printf("\n");
        break;
    }
    case PROP_TYPE: {
        properties_t * el, * tmp;
        HASH_ITER(hh, value.v_props, el, tmp) {
            printf("%s : %s\n", el->key, el->value);
        }
        break;
    }
    default:
        break;
    }
}

void print_c2_payload(const c2_payload_map_t * c2_payload) {
    if (!c2_payload) return;
    c2_payload_map_t * el, *tmp;
    HASH_ITER(hh, c2_payload, el, tmp) {
        print_key(el->key);
        print_value(el->value);
        printf("\n");
    }
}
