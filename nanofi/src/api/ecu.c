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

nifi_instance * instance = NULL;
standalone_processor * proc = NULL;
flow_file_list * ff_list = NULL;
uint64_t curr_offset = 0;
volatile sig_atomic_t stopped = 0;

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        stopped = 1;
    }
}

void set_offset(uint64_t offset) {
    curr_offset = offset;
}

uint64_t get_offset() {
    return curr_offset;
}

void on_trigger_tailfilechunk(processor_session * ps, processor_context * ctx) {
    char file_path[4096];
    char chunk_size[50];

    free_flow_file_list(&ff_list);

    if (get_property(ctx, "file_path", file_path, sizeof(file_path)) != 0) {
        return;
    }

    if (get_property(ctx, "chunk_size", chunk_size, sizeof(chunk_size)) != 0) {
        return;
    }

    errno = 0;
    unsigned long chunk_size_value = strtoul(chunk_size, NULL, 10);

    if (errno != 0) {
        printf("Invalid chunk size specified\n");
        return;
    }

    FILE * fp = fopen(file_path, "rb");

    if (!fp) {
        printf("Unable to open file. {file: %s, reason: %s}\n", file_path, strerror(errno));
        return;
    }

    char buff[chunk_size_value + 1];
    size_t bytes_read = 0;
    fseek(fp, curr_offset, SEEK_SET);
    while ((bytes_read = fread(buff, 1, chunk_size_value, fp)) > 0) {
        if (bytes_read < chunk_size_value) {
            break;
        }
        buff[chunk_size_value] = '\0';
        flow_file_record * ffr = write_to_flow_file(buff, instance, proc);
        char offset_str[21];
        snprintf(offset_str, sizeof(offset_str), "%llu", curr_offset);
        add_attribute(ffr, "current offset", offset_str, strlen(offset_str));
        add_flow_file_record(&ff_list, ffr);
        curr_offset = ftell(fp);
    }
    fclose(fp);
}

flow_file_info log_aggregate(const char * file_path, char delim, uint64_t curr_offset) {
    flow_file_info ff_info;
    memset(&ff_info, 0, sizeof(ff_info));

    if (!file_path) {
        return ff_info;
    }

    char buff[MAX_BYTES_READ + 1];
    errno = 0;
    FILE * fp = fopen(file_path, "rb");
    if (!fp) {
        printf("Cannot open file: {file: %s, reason: %s}\n", file_path, strerror(errno));
        return ff_info;
    }
    fseek(fp, curr_offset, SEEK_SET);

    flow_file_list * ffl = NULL;
    size_t bytes_read = 0;
    while ((bytes_read = fread(buff, 1, MAX_BYTES_READ, fp)) > 0) {
        buff[bytes_read] = '\0';
        struct token_list tokens = tokenize_string_tailfile(buff, delim);
        if (tokens.total_bytes > 0) {
            curr_offset += tokens.total_bytes;
            fseek(fp, curr_offset, SEEK_SET);
        }

        token_node * head;
        for (head = tokens.head; head && head->data; head = head->next) {
            flow_file_record * ffr = write_to_flow_file(head->data, instance, proc);
            char offset[21] = {0};
            snprintf(offset, sizeof(offset), "%llu", curr_offset);
            add_attribute(ffr, "file offset", offset, strlen(offset));
            add_flow_file_record(&ffl, ffr);
        }
        ff_info.total_bytes += tokens.total_bytes;
        free_all_tokens(&tokens);
    }
    fclose(fp);
    ff_info.ff_list = ffl;
    return ff_info;
}

void on_trigger_logaggregator(processor_session * ps, processor_context * ctx) {

    char file_path[4096];
    char delimiter[3];

    if (get_property(ctx, "file_path", file_path, sizeof(file_path)) != 0) {
        return;
    }

    if (get_property(ctx, "delimiter", delimiter, sizeof(delimiter)) != 0) {
        printf("No delimiter found\n");
        return;
    }

    if (strlen(delimiter) == 0) {
        printf("Delimiter not specified or it is empty\n");
        return;
    }
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

    flow_file_info ff_info = log_aggregate(file_path, delim, get_offset());
    set_offset(get_offset() + ff_info.total_bytes);
    ff_list = ff_info.ff_list;
}
