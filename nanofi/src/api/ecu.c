#include "api/ecu.h"
#include "api/nanofi.h"
#include "core/string_utils.h"
#include "core/cstructs.h"
#include "core/file_utils.h"
#include "core/flowfiles.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/stat.h>

processor_params * procparams = NULL;
volatile sig_atomic_t stopped = 0;

void free_proc_params(const char * uuid) {

    struct processor_params * pp = NULL;
    HASH_FIND_STR(procparams, uuid, pp);
    if (pp) {
        free_flow_file_list(&pp->ff_list);
        free(pp->properties->file_path);
        free(pp->properties);
        HASH_DEL(procparams, pp);
        free(pp);
    }
}

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        stopped = 1;
    }
}

void init_common_input(tailfile_input_params * input_params, char ** args) {
    if (args && *args) {
        input_params->file = args[1];
        input_params->interval = args[2];
        input_params->instance = args[4];
        input_params->tcp_port = args[5];
        input_params->nifi_port_uuid = args[6];
    }
}

tailfile_input_params init_logaggregate_input(char ** args) {
    tailfile_input_params input_params;
    memset(&input_params, 0, sizeof(input_params));
    init_common_input(&input_params, args);
    input_params.delimiter = args[3];
    return input_params;
}

tailfile_input_params init_tailfile_chunk_input(char ** args) {
    tailfile_input_params input_params;
    memset(&input_params, 0, sizeof(input_params));
    init_common_input(&input_params, args);
    input_params.chunk_size = args[3];
    return input_params;
}

int validate_input_params(tailfile_input_params * params, uint64_t * intrvl, uint64_t * port_num) {
    if (access(params->file, F_OK) == -1) {
        printf("Error: %s doesn't exist!\n", params->file);
        return -1;
    }

    struct stat stats;
    int ret = stat(params->file, &stats);

    if (ret == -1) {
        printf("Error occurred while getting file status {file: %s, error: %s}\n", params->file, strerror(errno));
        return -1;
    }
    // Check for file existence
    if (S_ISDIR(stats.st_mode)){
        printf("Error: %s is a directory!\n", params->file);
        return -1;
    }

    errno = 0;
    *intrvl = (uint64_t)(strtoul(params->interval, NULL, 10));

    if (errno != 0) {
        printf("Invalid interval value specified\n");
        return -1;
    }

    errno = 0;
    *port_num = (uint64_t)(strtoul(params->tcp_port, NULL, 10));
    if (errno != 0) {
        printf("Cannot convert tcp port to numeric value\n");
        return -1;
    }
    return 0;
}

void setup_signal_action() {
    struct sigaction action;
    memset(&action, 0, sizeof(sigaction));
    action.sa_handler = signal_handler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
}

nifi_proc_params setup_nifi_processor(tailfile_input_params * input_params, const char * processor_name, void(*callback)(processor_session *, processor_context *)) {
    nifi_proc_params params;
    nifi_port port;
    port.port_id = input_params->nifi_port_uuid;

    nifi_instance * instance = create_instance(input_params->instance, &port);
    add_custom_processor(processor_name, callback);
    standalone_processor * proc = create_processor(processor_name, instance);
    params.instance = instance;
    params.processor = proc;
    return params;
}

void add_to_hash_table(flow_file_record * ffr, uint64_t offset, const char * uuid) {
    struct processor_params * pp = NULL;
    HASH_FIND_STR(procparams, uuid, pp);
    if (pp == NULL) {
        pp = (struct processor_params*)malloc(sizeof(struct processor_params));
        memset(pp, 0, sizeof(struct processor_params));
        strcpy(pp->uuid_str, uuid);
        HASH_ADD_STR(procparams, uuid_str, pp);
    }

    add_flow_file_record(&pp->ff_list, ffr);
    pp->curr_offset = offset;
}

void delete_all_flow_files_from_proc(const char * uuid) {
    struct processor_params * pp = NULL;
    HASH_FIND_STR(procparams, uuid, pp);
    if (pp) {
        struct flow_file_list * head = pp->ff_list;
        while (head) {
            struct flow_file_list * tmp = head;
            free_flowfile(tmp->ff_record);
            head = head->next;
            free(tmp);
        }
        pp->ff_list = head;
    }
}

void delete_completed_flow_files_from_proc(const char * uuid) {
    struct processor_params * pp = NULL;
    HASH_FIND_STR(procparams, uuid, pp);
    if (pp) {
        struct flow_file_list * head = pp->ff_list;
        while (head) {
            struct flow_file_list * tmp = head;
            if (tmp->complete) {
                free_flowfile(tmp->ff_record);
                head = head->next;
                free(tmp);
            }
            else {
                break;
            }
        }
        pp->ff_list = head;
    }
}

uint64_t get_current_offset(const char * uuid) {
    struct processor_params * pp = NULL;
    HASH_FIND_STR(procparams, uuid, pp);
    if (pp) {
        return pp->curr_offset;
    }
    return 0;
}

processor_params * get_proc_params(const char * uuid) {
    struct processor_params * pp = NULL;
    HASH_FIND_STR(procparams, uuid, pp);
    return pp;
}

void update_proc_params(const char * uuid, uint64_t value, flow_file_list * ffl) {
    struct processor_params * pp = get_proc_params(uuid);
    if (!pp) {
        pp = (struct processor_params *)malloc(sizeof(struct processor_params));
        memset(pp, 0, sizeof(struct processor_params));
        pp->ff_list = ffl;
        pp->curr_offset = value;
        strcpy(pp->uuid_str, uuid);
        HASH_ADD_STR(procparams, uuid_str, pp);
        return;
    }
    delete_all_flow_files_from_proc(uuid);
    pp->curr_offset += value;
    pp->ff_list = ffl;
}

uint64_t update_curr_offset(const char * uuid, uint64_t value) {
    struct processor_params * pp = get_proc_params(uuid);
    if (pp) {
        pp->curr_offset += value;
        return pp->curr_offset;
    }

    pp = (struct processor_params *)malloc(sizeof(struct processor_params));
    memset(pp, 0, sizeof(struct processor_params));
    strcpy(pp->uuid_str, uuid);
    pp->curr_offset = value;
    HASH_ADD_STR(procparams, uuid_str, pp);
    return pp->curr_offset;
}

struct proc_properties * get_processor_properties(const char * uuid) {
    if (!uuid) {
        return NULL;
    }
    struct processor_params * pp = NULL;
    HASH_FIND_STR(procparams, uuid, pp);
    if (!pp) {
        return NULL;
    }
    return pp->properties;
}

void add_processor_properties(const char * uuid, struct proc_properties * const props) {
    struct processor_params * pp = get_proc_params(uuid);
    if (pp) {
        pp->properties = props;
        return;
    }

    pp = (struct processor_params *)malloc(sizeof(struct processor_params));
    memset(pp, 0, sizeof(struct processor_params));
    strcpy(pp->uuid_str, uuid);
    pp->properties = props;
    HASH_ADD_STR(procparams, uuid_str, pp);
}

void on_trigger_tailfilechunk(processor_session * ps, processor_context * ctx) {

    char uuid_str[37];
    get_proc_uuid_from_context(ctx, uuid_str);

    initialize_content_repo(ctx, uuid_str);

    struct proc_properties * props = get_processor_properties(uuid_str);
    if (!props) {
        char file_path[4096];
        char chunk_size[50];
        if (get_property(ctx, "file_path", file_path, sizeof(file_path)) != 0) {
            return;
        }

        if (get_property(ctx, "chunk_size", chunk_size, sizeof(chunk_size)) != 0) {
            return;
        }

        errno = 0;
        uint64_t chunk_size_value = strtoul(chunk_size, NULL, 10);

        if (errno != 0 || chunk_size_value == 0) {
            printf("Invalid chunk size specified\n");
            return;
        }

        props = (struct proc_properties *)malloc(sizeof(struct proc_properties));
        memset(props, 0, sizeof(struct proc_properties));
        int len = strlen(file_path);
        props->file_path = (char *)malloc((len + 1) * sizeof(char));
        strncpy(props->file_path, file_path, len);
        props->file_path[len] = '\0';
        props->chunk_size = chunk_size_value;
        add_processor_properties(uuid_str, props);
    }

    FILE * fp = fopen(props->file_path, "rb");

    if (!fp) {
        printf("Unable to open file. {file: %s, reason: %s}\n", props->file_path, strerror(errno));
        return;
    }

    char * buff = (char *)malloc((props->chunk_size +1 ) * sizeof(char));
    size_t bytes_read = 0;

    uint64_t curr_offset = get_current_offset(uuid_str);
    fseek(fp, curr_offset, SEEK_SET);
    while ((bytes_read = fread(buff, 1, props->chunk_size, fp)) > 0) {
        if (bytes_read < props->chunk_size) {
            break;
        }
        buff[props->chunk_size] = '\0';
        flow_file_record * ffr = write_to_flow(buff, strlen(buff), ctx);
        curr_offset = ftell(fp);
        add_attributes(ffr, props->file_path, curr_offset);
        add_to_hash_table(ffr, curr_offset, uuid_str);
    }
    free(buff);
    fclose(fp);
}

flow_file_info log_aggregate(const char * file_path, char delim, processor_context * ctx) {
    flow_file_info ff_info;
    memset(&ff_info, 0, sizeof(ff_info));

    if (!file_path) {
        return ff_info;
    }

    char uuid_str[37];
    get_proc_uuid_from_context(ctx, uuid_str);

    char buff[MAX_BYTES_READ + 1];
    errno = 0;
    FILE * fp = fopen(file_path, "rb");
    if (!fp) {
        printf("Cannot open file: {file: %s, reason: %s}\n", file_path, strerror(errno));
        return ff_info;
    }

    uint64_t curr_offset = get_current_offset(uuid_str);

    fseek(fp, curr_offset, SEEK_SET);

    flow_file_list * ffl = NULL;
    size_t bytes_read = 0;
    while ((bytes_read = fread(buff, 1, MAX_BYTES_READ, fp)) > 0) {
        buff[bytes_read] = '\0';
        struct token_list tokens = tokenize_string_tailfile(buff, delim);
        if (tokens.total_bytes > 0) {
            ff_info.total_bytes += tokens.total_bytes;
            curr_offset += tokens.total_bytes;
            fseek(fp, curr_offset, SEEK_SET);
        }

        token_node * head;
        for (head = tokens.head; head && head->data; head = head->next) {
            flow_file_record * ffr = write_to_flow(head->data, strlen(head->data), ctx);
            add_attributes(ffr, file_path, curr_offset);
            add_flow_file_record(&ffl, ffr);
        }
        free_all_tokens(&tokens);
    }
    fclose(fp);
    ff_info.ff_list = ffl;
    return ff_info;
}

struct proc_properties * get_properties(const char * uuid, processor_context * ctx) {
    struct proc_properties * props = get_processor_properties(uuid);
    if (props) {
        return props;
    }

    char file_path[4096];
    char delimiter[3];

    if (get_property(ctx, "file_path", file_path, sizeof(file_path)) != 0) {
        return props;
    }

    if (get_property(ctx, "delimiter", delimiter, sizeof(delimiter)) != 0) {
        printf("No delimiter found\n");
        return props;
    }

    if (strlen(delimiter) == 0) {
        printf("Delimiter not specified or it is empty\n");
        return props;
    }

    props = (struct proc_properties *)malloc(sizeof(struct proc_properties));
    memset(props, 0, sizeof(struct proc_properties));

    char delim = delimiter[0];

    if (delim == '\\') {
          if (strlen(delimiter) > 1) {
            switch (delimiter[1]) {
              case 'r':
                delim = '\r';
                break;
              case 't':
                delim = '\t';
                break;
              case 'n':
                delim = '\n';
                break;
              case '\\':
                delim = '\\';
                break;
              default:
                break;
            }
        }
    }

    int len = strlen(file_path);
    props->file_path = (char *)malloc((len + 1) * sizeof(char));
    strncpy(props->file_path, file_path, len);
    props->file_path[len] = '\0';
    props->delimiter = delim;

    add_processor_properties(uuid, props);
    return props;
}

void on_trigger_logaggregator(processor_session * ps, processor_context * ctx) {
    char uuid_str[37];
    get_proc_uuid_from_context(ctx, uuid_str);

    struct proc_properties * props = get_properties(uuid_str, ctx);

    if (!props || !props->file_path) return;

    char delim = props->delimiter;

    initialize_content_repo(ctx, uuid_str);
    flow_file_info ff_info = log_aggregate(props->file_path, delim, ctx);

    update_proc_params(uuid_str, ff_info.total_bytes, ff_info.ff_list);
}

void write_flow_file(flow_file_record * ffr, const char * buff, size_t count) {
    FILE * ffp = fopen(ffr->contentLocation, "ab");
    if (!ffp) return;
    if (fwrite(buff, 1, count, ffp) < count) {
        fclose(ffp);
        free_flowfile(ffr);
        return;
    }
    fclose(ffp);
}

flow_file_list * get_last_flow_file(const char * uuid) {
    struct processor_params * pp = NULL;
    HASH_FIND_STR(procparams, uuid, pp);
    if (!pp) {
        return NULL;
    }

    flow_file_list * ff_list = pp->ff_list;
    flow_file_list * el = NULL;
    LL_FOREACH(ff_list, el) {
        if (el && !el->next) {
            return el;
        }
    }
    return NULL;
}

flow_file_list * add_flow_file_to_proc_params(const char * uuid, flow_file_record * ffr) {
    struct processor_params * pp = NULL;
    HASH_FIND_STR(procparams, uuid, pp);
    if (!pp) {
        pp = (struct processor_params *)malloc(sizeof(struct processor_params));
        memset(pp, 0, sizeof(struct processor_params));
        strcpy(pp->uuid_str, uuid);
        HASH_ADD_STR(procparams, uuid_str, pp);
    }
    flow_file_list * ffl_node = add_flow_file_record(&pp->ff_list, ffr);
    ffl_node->complete = 0;
    return ffl_node;
}

void on_trigger_tailfiledelimited(processor_session * ps, processor_context * ctx) {
    char uuid_str[37];
    get_proc_uuid_from_context(ctx, uuid_str);

    initialize_content_repo(ctx, uuid_str);
    struct proc_properties * props = get_properties(uuid_str, ctx);

    if (!props || !props->file_path) return;

    char delim = props->delimiter;

    FILE * fp = fopen(props->file_path, "rb");

    if (!fp) {
        printf("Unable to open file. {file: %s, reason: %s}\n", props->file_path, strerror(errno));
        return;
    }

    char buff[MAX_BYTES_READ + 1];
    size_t bytes_read = 0;

    uint64_t curr_offset = get_current_offset(uuid_str);
    fseek(fp, curr_offset, SEEK_SET);

    flow_file_list * ffl_node = get_last_flow_file(uuid_str);
    while ((bytes_read = fread(buff, 1, MAX_BYTES_READ, fp)) > 0) {
        buff[bytes_read] = '\0';
        const char * begin = buff;
        const char * end = NULL;

        while ((end = strchr(begin, delim))) {
            uint64_t len = end - begin;
            if (len > 0) {
                if (!ffl_node || ffl_node->complete) {
                    ffl_node = add_flow_file_to_proc_params(uuid_str, generate_flow(ctx));
                }
                write_flow_file(ffl_node->ff_record, begin, len);
                update_curr_offset(uuid_str, (len + 1));
            }
            else {
                update_curr_offset(uuid_str, 1);
            }
            if (ffl_node) {
                ffl_node->complete = 1;
                update_attributes(ffl_node->ff_record, props->file_path, get_current_offset(uuid_str));
            }
            begin = (end + 1);
        }

        if (!end && *begin != '\0') {
            if (!ffl_node || ffl_node->complete) {
                ffl_node = add_flow_file_to_proc_params(uuid_str, generate_flow(ctx));
            }
            size_t count = strlen(begin);
            write_flow_file(ffl_node->ff_record, begin, count);
            update_curr_offset(uuid_str, count);
            update_attributes(ffl_node->ff_record, props->file_path, get_current_offset(uuid_str));
        }
    }
    fclose(fp);
}
