
#include "cbor.h"
#include "uthash.h"
#include <core/core_utils.h>

int add_property(struct properties ** head, const char * name,
    const char * value) {
  if (!head || !name || !value) {
    return -1;
  }
  properties_t * el = NULL;
  HASH_FIND_STR(*head, name, el);
  if (el) {
    HASH_DEL(*head, el);
    free_property(el);
    free(el);
  }

  properties_t * new_prop = (properties_t *) malloc(sizeof(struct properties));
  size_t name_len = strlen(name);
  new_prop->key = (char *) malloc(name_len + 1);
  strcpy(new_prop->key, name);

  size_t value_len = strlen(value);
  new_prop->value = (char *) malloc(value_len + 1);
  strcpy(new_prop->value, value);

  HASH_ADD_KEYPTR(hh, *head, new_prop->key, strlen(new_prop->key), new_prop);
  return 0;
}

properties_t * clone_properties(properties_t * props) {
  if (!props) {
    return NULL;
  }

  properties_t * clone = NULL;
  properties_t * el, *tmp;
  HASH_ITER(hh, props, el, tmp) {
    properties_t * entry = (properties_t *) malloc(sizeof(properties_t));
    size_t key_len = strlen(el->key);
    size_t val_len = strlen(el->value);
    entry->key = (char *) malloc(key_len + 1);
    entry->value = (char *) malloc(val_len + 1);
    strcpy(entry->key, el->key);
    strcpy(entry->value, el->value);
    HASH_ADD_KEYPTR(hh, clone, entry->key, strlen(entry->key), entry);
  }
  return clone;
}

void free_property(properties_t * prop) {
  if (prop) {
    free(prop->key);
    free(prop->value);
  }
}

void free_properties(properties_t * prop) {
  if (prop) {
    properties_t * el, *tmp = NULL;
    HASH_ITER(hh, prop, el, tmp) {
      HASH_DELETE(hh, prop, el);
      free(el->key);
      free(el->value);
      free(el);
    }
  }
}

void serialize_properties(properties_t * props, char ** data, size_t * len) {
  if (!props || HASH_COUNT(props) == 0) {
    *data = NULL;
    *len = 0;
    return;
  }
  cbor_item_t * items = cbor_new_indefinite_map();
  properties_t * el, *tmp;
  HASH_ITER(hh, props, el, tmp) {
    cbor_map_add(items,
                  (struct cbor_pair){
                     .key = cbor_move(cbor_build_string(el->key)),
                     .value = cbor_move(cbor_build_string(el->value))
                 });
  }
  cbor_serialize_alloc(items, (unsigned char **)data, len);
  cbor_decref(&items);
  free_properties(props);
}

attribute_set prepare_attributes(properties_t * attributes) {
  attribute_set as;
  memset(&as, 0, sizeof(attribute_set));
  if (!attributes)
    return as;

  as.size = HASH_COUNT(attributes);
  attribute * attrs = (attribute *) malloc(as.size * sizeof(attribute));

  properties_t *p, *tmp;
  int i = 0;
  HASH_ITER(hh, attributes, p, tmp)
  {
    attrs[i].key = (char *) malloc(strlen(p->key) + 1);
    strcpy((char *) attrs[i].key, p->key);
    char * value = (char *) malloc(strlen(p->value) + 1);
    strcpy(value, p->value);
    attrs[i].value = (void *) value;
    attrs[i].value_size = strlen(value);
    i++;
  }
  as.attributes = attrs;
  return as;
}

void free_attributes(attribute_set as) {
  int i;
  for (i = 0; i < as.size; ++i) {
    free((void *) (as.attributes[i].key));
    free((void *) (as.attributes[i].value));
  }
  free(as.attributes);
}

attribute_set copy_attributes(attribute_set as) {
  attribute_set copy;
  memset(&copy, 0, sizeof(attribute_set));
  copy.size = as.size;
  copy.attributes = (attribute *) malloc(copy.size * sizeof(attribute));
  int i;
  for (i = 0; i < copy.size; ++i) {
    size_t key_len = strlen(as.attributes[i].key);
    char * key = (char *) malloc(key_len + 1);
    memset(key, 0, key_len + 1);
    strcpy(key, as.attributes[i].key);
    copy.attributes[i].key = key;
    char * value = (char *) malloc(as.attributes[i].value_size + 1);
    strcpy(value, (char *) (as.attributes[i].value));
    copy.attributes[i].value = (void *) value;
    copy.attributes[i].value_size = as.attributes[i].value_size;
  }
  return copy;
}

attribute * find_attribute(attribute_set as, const char * key) {
  int i;
  for (i = 0; i < as.size; ++i) {
    if (strcmp(as.attributes[i].key, key) == 0) {
        return &as.attributes[i];
    }
  }
  return NULL;
}

attribute_set unpack_metadata(char * meta, size_t len) {
  attribute_set as;
  memset(&as, 0, sizeof(as));
  struct cbor_load_result result;
  cbor_item_t * item = cbor_load(meta, len, &result);
  if (result.error.code != CBOR_ERR_NONE || !cbor_map_is_indefinite(item)
      || cbor_map_size(item) == 0) {
    cbor_decref(&item);
    return as;
  }

  size_t sz = cbor_map_size(item);
  struct cbor_pair * kvps = cbor_map_handle(item);
  as.size = sz;
  attribute * attrs = (attribute *)malloc(as.size * sizeof(attribute));

  int i;
  for (i = 0; i < sz; ++i) {
    unsigned char * key_item = cbor_string_handle(kvps[i].key);
    size_t key_len = cbor_string_length(kvps[i].key);
    char * key = (char *)malloc(key_len + 1);
    memset(key, 0, key_len + 1);
    memcpy(key, key_item, key_len);
    key[key_len] = '\0';
    attrs[i].key = key;

    unsigned char * val_item = cbor_string_handle(kvps[i].value);
    size_t val_len = cbor_string_length(kvps[i].value);
    char * val = (char *)malloc(val_len + 1);
    memset(val, 0, val_len + 1);
    memcpy(val, val_item, val_len);
    val[val_len] = '\0';
    attrs[i].value = (void *)val;
    attrs[i].value_size = val_len + 1;
  }
  as.attributes = attrs;
  cbor_decref(&item);
  return as;
}

