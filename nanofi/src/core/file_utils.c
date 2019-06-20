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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "core/string_utils.h"
#include "core/file_utils.h"

int is_directory(const char * path) {
    struct stat dir_stat;
    if (stat(path, &dir_stat) < 0) {
        return 0;
    }
    return S_ISDIR(dir_stat.st_mode);
}

char * get_separator(int force_posix)
{
    char * sep = malloc(2 * sizeof(char));
    strncpy(sep, "/", 1);
    sep[1] = '\0';
#ifdef WIN32
    if (force_posix) {
        return sep;
    } else {
        char * sep = malloc(3 * sizeof(char));
        strncpy(sep, "\\", 2);
        sep[2] = '\0';
        return sep;
    }
#else
    return sep;
#endif
}

char * concat_path(const char * parent, const char * child) {
    char * path = (char *)malloc((strlen(parent) + strlen(child) + 2) * sizeof(char));
    strcpy(path, parent);
    strcat(path, get_separator(1));
    strcat(path, child);
    return path;
}

void remove_directory(const char * dir_path) {

    if (!is_directory(dir_path)) {
        if (unlink(dir_path) == -1) {
            printf("Could not remove file %s\n", dir_path);
        }
        return;
    }

    uint64_t path_len = strlen(dir_path);
    struct dirent * dir;
    DIR * d = opendir(dir_path);

    while ((dir = readdir(d)) != NULL) {
        char * entry_name = dir->d_name;
        if (!strcmp(entry_name, ".") || !strcmp(entry_name, "..")) {
            continue;
        }
        char * path = concat_path(dir_path, entry_name);
        remove_directory(path);
        free(path);
    }

    rmdir(dir_path);
    closedir(d);
}

long int file_size(const char * path) {
    if (!path) return -1;

    struct stat fstat;
    if (stat(path, &fstat) < 0) {
        return -1;
    }
    return fstat.st_size;
}

int make_dir(const char * path) {
    if (!path) return 0;

    errno = 0;
    int ret = mkdir(path, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
    if (ret == 0) {
        return 0;
    }

    switch (errno) {
    case ENOENT: {
        char * found = strrchr(path, '/');
        if (!found) {
            return -1;
        }
        int len = found - path;
        char * dir = calloc(len + 1, sizeof(char));
        strncpy(dir, path, len);
        dir[len] = '\0';
        int res = make_dir(dir);
        if (res < 0) {
            free(dir);
            return -1;
        }
        free(dir);
        return mkdir(path, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
    }
    case EEXIST: {
        if (is_directory(path)) {
            return 0;
        }
        return -1;
    }
    default:
        return -1;
    }
}
