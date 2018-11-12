#include "core/cstructs.h"
#include "api/nanofi.h"
#include "core/processors.h"

void *create_cxx_instance(const char *url, nifi_port *port);

void initialize_cxx_instance(nifi_instance *instance);

void enable_async_cxx_c2(nifi_instance *instance, C2_Server *server, c2_stop_callback *c1, c2_start_callback *c2, c2_update_callback *c3);
