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

#ifndef WIN32
#include <dirent.h>
#include <unistd.h>
#include <glob.h>
#else
#include <windows.h>
#include <fileapi.h>
#include <handleapi.h>
#include <direct.h>
#pragma comment(lib, "User32.lib")
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include <assert.h>

#include "core/string_utils.h"
#include "core/file_utils.h"
#include "core/log.h"
#include "utlist.h"

#ifdef _MSC_VER
#ifndef PATH_MAX
#define PATH_MAX 260
#endif
#endif

#ifdef WIN32
#define stat _stat
#define mkdir _mkdir
#endif

int is_directory(const char * path) {
  struct stat dir_stat;
  if (stat(path, &dir_stat) < 0) {
    return 0;
  }
#ifdef WIN32
  return dir_stat.st_mode & S_IFDIR;
#else
  return S_ISDIR(dir_stat.st_mode);
#endif
}

const char * get_separator(int force_posix) {
#ifdef WIN32
  if (!force_posix) {
    return "\\";
  }
#endif
  return "/";
}

char * concat_path(const char * parent, const char * child) {
  char * path = (char *) malloc((strlen(parent) + strlen(child) + 2) * sizeof(char));
  strcpy(path, parent);
  const char * sep = get_separator(0);
  strcat(path, sep);
  strcat(path, child);
  return path;
}

#ifndef WIN32
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
#else
void remove_directory(const char * dir_path) {
  HANDLE hFind;
  WIN32_FIND_DATA fd;

  hFind = FindFirstFile(dir_path, &fd);
  if (hFind == INVALID_HANDLE_VALUE) {
    return;
  }

  if (fd.dwFileAttributes != FILE_ATTRIBUTE_DIRECTORY) {
    DeleteFile(dir_path);
    FindClose(hFind);
    return;
  }

  char * path = concat_path(dir_path, "*");
  HANDLE hFind1;
  if ((hFind1 = FindFirstFile(path, &fd)) != INVALID_HANDLE_VALUE) {
    do {
      char * entry_name = fd.cFileName;
      if (!strcmp(entry_name, ".") || !strcmp(entry_name, "..")) continue;
      char * entry_path = concat_path(dir_path, entry_name);
      remove_directory(entry_path);
      free(entry_path);
    }while (FindNextFile(hFind1, &fd));
  }
  RemoveDirectory(dir_path);
  FindClose(hFind);
  FindClose(hFind1);
  free(path);
}
#endif

int make_dir(const char * path) {
  if (!path)
    return -1;

  errno = 0;
#ifdef WIN32
  int ret = mkdir(path);
  char path_sep = '\\';
#else
  int ret = mkdir(path,
      S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
  char path_sep = '/';
#endif
  if (ret == 0) {
    return 0;
  }

  switch (errno) {
  case ENOENT: {
    char * found = strrchr(path, path_sep);
    if (!found) {
      return -1;
    }
    int len = found - path;
    char * dir = calloc(len + 1, sizeof(char));
    strncpy(dir, path, len);
    dir[len] = '\0';
    int res = make_dir(dir);
    free(dir);
    if (res < 0) {
      return -1;
    }
#ifdef WIN32
    return mkdir(path);
#else
    return mkdir(path,
        S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif
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

char * get_current_working_directory() {
  char * cwd = (char *) malloc(PATH_MAX * sizeof(char));
  memset(cwd, 0, PATH_MAX);
#ifdef WIN32
  if (_getcwd(cwd, PATH_MAX) != NULL)
  return cwd;
#else
  if (getcwd(cwd, PATH_MAX) != NULL) {
    return cwd;
  }
#endif
  free(cwd);
  return NULL;
}

properties_t * read_properties_file(const char * file_path) {
  if (!file_path) {
    logc(err, "%s", "No file path provided");
    return NULL;
  }

  properties_t * params = NULL;
  FILE * fp = fopen(file_path, "r");
  char * line = NULL;
  size_t size = 0;
  if (!fp) {
    logc(err, "Could not open file %s", file_path);
    return NULL;
  }
#ifndef WIN32
  while (getline(&line, &size, fp) > 0) {
#else
    size = 1024;
    line = (char *)malloc(1024);
    while (fgets(line, size, fp) != NULL) {
#endif
    char ** tokens = parse_tokens(line, size, 2, " =\n");
    properties_t * el = (properties_t *) malloc(sizeof(properties_t));

    el->key = tokens[0];
    el->value = tokens[1];

    char ** tmp = tokens;
    free(tmp);

    if (el->key && el->value) {
      HASH_ADD_KEYPTR(hh, params, el->key, strlen(el->key), el);
    } else {
      free(el->key);
      free(el->value);
      free(el);
    }
  }
  free(line);
  fclose(fp);
  return params;
}

size_t get_file_size(FILE * fp) {
  assert(fp);
  off_t offset = ftell(fp);
  fseek(fp, 0, SEEK_END);
  off_t size = ftell(fp);
  fseek(fp, offset, SEEK_SET);
  return (size_t) size;
}

#ifndef WIN32
int is_file(const char * file_path) {
  if (!file_path) {
    return -1;
  }

  struct stat stats;
  int ret = stat(file_path, &stats);

  if (ret == -1) {
    logc(err,
        "Error occurred while getting file status {file: %s, error: %s}\n",
        file_path, strerror(errno));
    return -1;
  }

  if (S_ISDIR(stats.st_mode)) {
    logc(err, "Error: %s is a directory!\n", file_path);
    return -1;
  }
  return 0;
}
#else
int is_file(const char * file_path) {
  if (!file_path) {
    return -1;
  }
  HANDLE hFind;
  WIN32_FIND_DATA fd;

  hFind = FindFirstFile(file_path, &fd);
  if (hFind == INVALID_HANDLE_VALUE || fd.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {
    return -1;
  }

  return 0;
}
#endif

#ifndef WIN32
fileinfo * scan_matching_files(const char * path, int include_dirs) {
  if (!path) {
    return NULL;
  }
  glob_t gb;
  memset(&gb, 0, sizeof(gb));
  int ret = glob(path, GLOB_TILDE | GLOB_ERR, NULL, &gb);
  switch (ret) {
  case GLOB_ABORTED:
    logc(err, "path expansion aborted for path %s", path);
    return NULL;
  case GLOB_NOMATCH:
    logc(err, "no match for path %s found", path);
    return NULL;
  case GLOB_NOSPACE:
    logc(err, "no space error while expanding path %s", path);
    return NULL;
  }

  fileinfo * paths = NULL;
  if (gb.gl_pathc > 0) {
    char ** iter = NULL;
    for (iter = gb.gl_pathv; *iter != NULL; ++iter) {
      if (!include_dirs && is_file(*iter) < 0)
        continue;
      fileinfo * node = (fileinfo *)malloc(sizeof(fileinfo));
      memset(node, 0, sizeof(fileinfo));
      node->path = copystr(*iter);
      LL_APPEND(paths, node);
    }
  }
  globfree(&gb);
  return paths;
}
#else
fileinfo * scan_matching_files(const char * path, int include_dirs) {
  if (!path) return NULL;

  fileinfo * paths = NULL;
  size_t plen = strlen(path);
  if (plen > MAX_PATH) {
    return NULL;
  }

  HANDLE hfind;
  WIN32_FIND_DATAA fd;

  char buff[MAX_PATH] = {'\0'};
  const char * star = strchr(path, '*');
  if (!star) star = path + plen;
  const char * suffix = strchr(star, '\\');
  if (!suffix) suffix = path + plen;
  memcpy(buff, path, suffix - path);

  hfind = FindFirstFileA(buff, &fd);
  if (hfind != INVALID_HANDLE_VALUE) {
    do {
      char * entry_name = fd.cFileName;
      if (strcmp(entry_name, ".") == 0 || strcmp(entry_name, "..") == 0)
      continue;
      // walk backwards to the first \\ from star position
      while (star >= path && *star != '\\') star--;
      char new_path[MAX_PATH] = {'\0'};
      memcpy(new_path, path, star - path + 1);
      if (strlen(new_path) + strlen(entry_name) + strlen(suffix) > MAX_PATH - 1) {
        continue;
      }
      strcat(new_path, entry_name);
      strcat(new_path, suffix);
      if (strchr(new_path, '*')) {
        fileinfo * children = scan_matching_files(new_path, include_dirs);
        LL_CONCAT(paths, children);
      }
      else {
        if (!include_dirs && fd.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {
          continue;
        }
        fileinfo * node = (fileinfo *)malloc(sizeof(fileinfo));
        memset(node, 0, sizeof(fileinfo));
        node->path = copystr(new_path);
        LL_APPEND(paths, node);
      }
    } while (FindNextFileA(hfind, &fd));

    FindClose(hfind);
  }
  return paths;
}
#endif
