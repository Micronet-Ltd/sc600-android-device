/*
 * Copyright (C) 2016 Micronet Inc
 * <vladimir.zatulovsky@micronet-inc.com>
 * 
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cutils/klog.h>


#include "utils.h"
#include "debug.h"

unsigned int debug_level = INFO;

struct fastboot_cmd {
    struct fastboot_cmd *next;
    const char *prefix;
    unsigned prefix_len;
    void (* handler)(int src_fd, void *data, int64_t len, const char *arg);
};

static struct fastboot_cmd *cmdlist;

void fastboot_register(const char *prefix, void (* handler)(int src_fd, void *data, int64_t len, const char *arg))
{
    struct fastboot_cmd *cmd;
    cmd = (struct fastboot_cmd *)malloc(sizeof(*cmd));
    if (cmd) {
        cmd->prefix = prefix;
        cmd->prefix_len = strlen(prefix);
        cmd->handler = handler;
        cmd->next = cmdlist;
        cmdlist = cmd;
    }
}

enum fb_buffer_type {
    FB_BUFFER,
    FB_BUFFER_SPARSE,
};

struct fastboot_buffer {
    enum fb_buffer_type type;
    void *data;
    int64_t sz;
};

static void *load_fd(int fd, int64_t *max_size)
{
    int err;
    int64_t sz;
    char *data;

    data = 0;

    sz = get_file_size(fd);
    if (sz < 0) {
        return 0;
    }

    if (max_size && *max_size < sz) {
        sz = *max_size;
    }

    data = (char *)malloc(sz);
    if(!data) {
        return 0;
    }

    err = read(fd, data, sz);

    if(err != sz) {
        free(data);
        return 0;
    }

    if(max_size)
        *max_size = sz;

    return data;
}

#define MAX_SPARSE_RAM 256*1024*1024
static void fastboot_load_file(int src_fd, struct fastboot_buffer *fb_buf)
{
    int err;

    if (!fb_buf) {
        return;
    }

    lseek(src_fd, 0, SEEK_SET);

    fb_buf->sz = MAX_SPARSE_RAM;
    fb_buf->data = load_fd(src_fd, &fb_buf->sz);
    if (!fb_buf->data)
        return;

    if (fb_buf->sz < MAX_SPARSE_RAM) {
        fb_buf->type = FB_BUFFER_SPARSE;
    } else {
        fb_buf->type = FB_BUFFER;
    }
}

void fastboot_command(char *cmd, char *part, char *arg)
{
    int src_fd;
    struct fastboot_cmd *fb_cmd;
    struct fastboot_buffer fb_buf;

    D(DEBUG,"%s %s %s\n", cmd, part, arg);

    for (fb_cmd = cmdlist; fb_cmd; fb_cmd = fb_cmd->next) {
        if (memcmp(cmd, fb_cmd->prefix, fb_cmd->prefix_len)) {
            D(DEBUG,"mismatch %s %s", cmd, fb_cmd->prefix);
            continue;
        }

        D(DEBUG,"match found %s %s", cmd, fb_cmd->prefix);
        if (arg) {
            src_fd = open(arg, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        } else {
            src_fd = -1;
        }

        if (-1 != src_fd) {
            fastboot_load_file(src_fd, &fb_buf);
        } else {
            fb_buf.data = 0;
            fb_buf.sz = 0;
        }

        fb_cmd->handler(src_fd, fb_buf.data, fb_buf.sz, part);
        if (fb_buf.data) {
            free(fb_buf.data); 
        }

        if (-1 != src_fd) {
            close(src_fd);
        }
        D(DEBUG,"%s done\n", fb_cmd->prefix);
        return; 
    }
}

extern void commands_init(void);

int fastboot_init(void)
{
    commands_init();

    //parse update rule

    return 0;
}
