/*
 * Copyright (C) 2016 Micronet Inc
 * <vladimir.zatulovsky@micronet-inc.com>
 * 
 * Copyright (c) 2009-2013, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Google, Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
//#include <malloc.h>
//#include <sys/types.h>
//#include <sys/mman.h>
#include <sys/stat.h>
//#include <unistd.h>
//#include <sys/reboot.h>
//#include <sys/ioctl.h>
#include <sys/mount.h>

#include "bootimg.h"
//#include "commands/boot.h"
#include "flash.h"
#include "partitions.h"
#include "virtual_partitions.h"
#include "debug.h"
//#include "protocol.h"
//#include "trigger.h"
#include "utils.h"
//#include <cutils/config_utils.h>


#define BUFFER_SIZE 1024 * 1024
#define MIN(a, b) (a > b ? b : a)

static int GPT_header_location (void) {
    printf("%s: using second sector\n", __func__);
    return 1;
}

int trigger_gpt_layout(struct GPT_content *table) {
    printf("%s: %p", __func__, table);
    return 0;
}

void cmd_gpt_layout (int src_fd, const char *blk_dev, const char *arg) {
    struct GPT_entry_table *old_table;
    struct GPT_content entry;

    if (!blk_dev || !strcmp(blk_dev, "")) {
        printf("%s: blockdev not defined in config file\n", __func__);
        return;
    }

    //TODO: add same verification as in cmd_flash
    if (src_fd < 0) {
        printf("%s: no layout file\n", __func__);
        return;
    }

    old_table = GPT_get_device(blk_dev, GPT_header_location());

    GPT_default_content(&entry, old_table);
    if (!old_table)
        printf("%s: Could not get old gpt table\n", __func__);
    else
        GPT_release_device(old_table);

    if (!GPT_parse_file(src_fd, &entry)) {
        printf("%s: Could not parse partition config file\n", __func__);
        return;
    }

    if (trigger_gpt_layout(&entry)) {
        printf("%s: Vendor forbids this opperation\n", __func__);
        GPT_release_content(&entry);
        return;
    }

    if (!GPT_write_content(blk_dev, &entry)) {
        printf("%s: Unable to write gpt file\n", __func__);
        GPT_release_content(&entry);
        return;
    }

    GPT_release_content(&entry);
    printf("%s: done\n", __func__);
}

struct block_device {
    const char *part_name;
    const char *blk_dev_name;
    const char *fs_type;
    const char *mount_point;
};

struct block_device avail_block_devices[] = {
    {"/system", "/dev/block/bootdevice/by-name/system", "ext4", "/system"},
    {"/cache", "/dev/block/bootdevice/by-name/cache", "ext4", "/cache"},
    {"/userdata", "/dev/block/bootdevice/by-name/userdata", "ext4", "/data"},
    {"/boot", "/dev/block/bootdevice/by-name/boot", "emmc", 0},
    {"/recovery", "/dev/block/bootdevice/by-name/recovery", "emmc", 0},
    {"/misc", "/dev/block/bootdevice/by-name/misc", "raw", 0},
};

#define BLK_DEV_PREF "/dev/block/bootdevice/by-name"

int flash_mount(const char *part)
{
    int i;

    for (i = 0; i < (int)sizeof(avail_block_devices)/(int)sizeof(avail_block_devices[0]); i++) {
        if (avail_block_devices[i].mount_point && 0 == strcmp(part, avail_block_devices[i].part_name)) {
            mount(avail_block_devices[i].blk_dev_name, avail_block_devices[i].mount_point, avail_block_devices[i].fs_type,
                  MS_NOATIME | MS_NODEV | MS_NODIRATIME, 0);
        }
    }

    return 0;
}

int flash_find_entry(const char *blk_dev_name, char *out, size_t out_len)
{
    if (strlen(BLK_DEV_PREF) + strlen(blk_dev_name) + 1 > out_len) {
        printf("%s: not enough memory %s:%s [%d]", __func__, BLK_DEV_PREF, blk_dev_name, (int)out_len);
        return -1;
    }

    snprintf(out, out_len, "%s%s", BLK_DEV_PREF, blk_dev_name);

    if (access(out, F_OK ) == -1) {
        printf("%s: could not find partition file %s", __func__, blk_dev_name);
        return -1;
    }

    return 0;
}

int flash_erase(int fd)
{
    int64_t size;
    size = get_block_device_size(fd);
    D(DEBUG, "erase %" PRId64" data from %d\n", size, fd);

    return wipe_block_device(fd, size);
}

int flash_write(int partition_fd, int data_fd, ssize_t size, ssize_t skip)
{
    ssize_t written = 0;
    struct GPT_mapping input;
    struct GPT_mapping output;

    while (written < size) {
        int current_size = MIN(size - written, BUFFER_SIZE);

        if (gpt_mmap(&input, written + skip, current_size, data_fd)) {
            D(ERR, "Error in writing data, unable to map data file %zd at %zd size %d", size, skip, current_size);
            return -1;
        }
        if (gpt_mmap(&output, written, current_size, partition_fd)) {
            D(ERR, "Error in writing data, unable to map output partition");
            return -1;
        }

        memcpy(output.ptr, input.ptr, current_size);

        gpt_unmap(&input);
        gpt_unmap(&output);

        written += current_size;
    }

    return 0;
}

int flash_dump(int partition_fd, int data_fd, ssize_t part_len)
{
    ssize_t rd = 0;
    ssize_t total = 0;
    uint8_t buff[BUFFER_SIZE];

    while (total < part_len) {
        int len = MIN(part_len - total, BUFFER_SIZE);

        rd = read(partition_fd, buff, len);
        write(data_fd, buff, rd);
        total += rd;
    }

    return 0;
}

static void cmd_erase(int src_fd, void *data, int64_t len, const char *arg)
{
    int partition_fd;
    char path[PATH_MAX];

    printf("%s: %s\n", __func__, arg);

    if (flash_find_entry(arg, path, PATH_MAX)) {
        printf("%s: Couldn't find partition\n", __func__);
        printf("%s: partition table doesn't exist\n", __func__);
        return;
    }

    printf("%s: unmount %s\n", __func__, arg);
    if (!(0 == umount(arg) || errno == EINVAL || errno == ENOENT)) {
        printf("%s: failure unmount %s[%s]\n", __func__, arg, strerror(errno));
        return;
    }

    if (1) {
        int dbg_fd;
        int part_fd = flash_get_partiton(path);
        int64_t part_len = get_file_size64(part_fd);

        dbg_fd = open("/data/prev-dump.img", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        printf("%s: dump part %s[%lu]\n", __func__, path, part_len);
        flash_dump(part_fd, dbg_fd, part_len);
        sync();

        close(dbg_fd);
        flash_close(part_fd);
    }

    if (0) {
        partition_fd = flash_get_partiton(path); 
        if (partition_fd < 0) {
            printf("%s: partiton file does not exists\n", __func__);
        }

        if (flash_erase(partition_fd)) {
            printf("%s: failed to erase partition\n", __func__);
            flash_close(partition_fd);
            return;
        }

        if (flash_close(partition_fd) < 0) {
            printf("%s: could not close device %s\n", __func__, strerror(errno));
            printf("%s: failed to erase partition\n", __func__);
            return;
        }
    }
    printf("%s: Done\n", __func__);
}

static void cmd_flash(int src_fd, void *pdata, int64_t len, const char *arg)
{
    int64_t part_len;
    int i, part_fd;
    uint32_t total_blocks = 0, chunk, chunk_sz, chunk_blk_cnt, fill_val, *fill_buf = 0, *rd_buf;
    sparse_header_t *sparse_header;
    chunk_header_t chunk_header;
    char path[PATH_MAX];

    printf("%s: %s [%p, %lx]\n", __func__, arg, pdata, len);

    if (!(0 == umount(arg) || errno == EINVAL || errno == ENOENT)) {
        printf("%s: failure unmount %s[%s]\n", __func__, arg, strerror(errno));
        return;
    }

    if (try_handle_virtual_partition(0, BLK_DEV_PREF, arg)) {
        printf("%s: failure to handle partition table\n", __func__);
        return;
    }

    if (flash_find_entry(arg, path, PATH_MAX)) {
        printf("%s: partition doesn't exist\n", __func__);
        return;
    }

//    printf("%s: %s found\n", __func__, path);

    // TODO: Maybe its goot idea to check whether the partition is bootable
    if (!strcmp(arg, "boot") || !strcmp(arg, "recovery")) {
        if (memcmp((void *)pdata, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
            printf("%s: not a boot image\n", __func__);
            return;
        }
    }

    part_fd = flash_get_partiton(path);
    part_len = get_file_size64(part_fd);

    if (part_len < len) {
        flash_close(part_fd);
        printf("%s: the file is too large\n", __func__);
        return;
    }

    lseek64(src_fd, 0, SEEK_SET);

    // Read and skip over sparse image header
    sparse_header = (sparse_header_t *)pdata;
    if (sparse_header->magic == SPARSE_HEADER_MAGIC) {
        printf("%s: sparse file in %d chanks\n", __func__, sparse_header->total_chunks);
        rd_buf = (uint32_t *)((uint8_t *)pdata + sparse_header->file_hdr_sz);
        if (part_len < ((int64_t)sparse_header->total_blks * (int64_t)sparse_header->blk_sz)) {
            flash_close(part_fd);
            printf("%s: the sparse-file is too large\n", __func__);
            return;
        }
        lseek64(src_fd, (uint64_t)sparse_header->file_hdr_sz, SEEK_CUR);
        if (sizeof(sparse_header_t) < sparse_header->file_hdr_sz) {
            /* Skip the remaining bytes in a header that is longer than
             * we expected.
             */
            printf("%s: %d: seek src %d bytes\n", __func__, __LINE__, sparse_header->file_hdr_sz - (uint16_t)sizeof(sparse_header_t));
            lseek64(src_fd, (uint64_t)sparse_header->file_hdr_sz - sizeof(sparse_header_t), SEEK_CUR);
        }
        printf ("%s: === Sparse Image Header ===\n", __func__);
        printf ("%s:     magic: 0x%x\n", __func__, sparse_header->magic);
        printf ("%s:     major_version: 0x%x\n", __func__, sparse_header->major_version);
        printf ("%s:     minor_version: 0x%x\n", __func__, sparse_header->minor_version);
        printf ("%s:     file_hdr_sz: %d\n", __func__, sparse_header->file_hdr_sz);
        printf ("%s:     chunk_hdr_sz: %d\n", __func__, sparse_header->chunk_hdr_sz);
        printf ("%s:     blk_sz: %d\n", __func__, sparse_header->blk_sz);
        printf ("%s:     total_blks: %d\n", __func__, sparse_header->total_blks);
        printf ("%s:     total_chunks: %d\n", __func__, sparse_header->total_chunks);

        for (chunk = 0; chunk < sparse_header->total_chunks; chunk++) {
            /* Make sure the total image size does not exceed the partition size */
            if(((int64_t)total_blocks * (int64_t)sparse_header->blk_sz) >= part_len) {
                printf("%s: too more blocks\n", __func__);
                flash_close(part_fd);
                return;
            }
            /* Read and skip over chunk header */
            if (sizeof(chunk_header_t) != read(src_fd, &chunk_header, sizeof(chunk_header_t))) {
                printf("%s: %d: failure to read src\n", __func__, __LINE__);
                flash_close(part_fd);
                return;
            }

//            printf("%s: === Chunk Header ===\n", __func__);
//            printf("%s:     chunk_type: 0x%x\n", __func__, chunk_header.chunk_type);
//            printf("%s:     chunk_data_sz: %d\n", __func__, chunk_header.chunk_sz);
//            printf("%s:     total_size: %d\n", __func__, chunk_header.total_sz);

            if(sparse_header->chunk_hdr_sz > sizeof(chunk_header_t)) {
                /* Skip the remaining bytes in a header that is longer than
                 * we expected.
                 */
                printf("%s: %d: seek src %d bytes\n", __func__, __LINE__, sparse_header->chunk_hdr_sz - (uint16_t)sizeof(chunk_header_t));
                lseek64(src_fd, (uint64_t)sparse_header->chunk_hdr_sz - sizeof(chunk_header_t), SEEK_CUR);
            }

            chunk_sz = sparse_header->blk_sz * chunk_header.chunk_sz;

            /* Make sure multiplication does not overflow uint32 size */
            if (sparse_header->blk_sz && (chunk_header.chunk_sz != chunk_sz / sparse_header->blk_sz)) {
                printf("%s: invalid chunk header\n", __func__);
                flash_close(part_fd);
                return;
            }

            /* Make sure that the chunk size calculated from sparse image does not
             * exceed partition size
             */
            if ((int64_t)total_blocks * (int64_t)sparse_header->blk_sz + chunk_sz > part_len) {
                printf("%s: too large chunk\n", __func__);
                flash_close(part_fd);
                return;
            }

            switch (chunk_header.chunk_type) {
                case CHUNK_TYPE_RAW:
                    if(chunk_header.total_sz != (sparse_header->chunk_hdr_sz + chunk_sz)) {
                        printf("%s: invalid raw chunk header\n", __func__);
                        flash_close(part_fd);
                        return;
                    }

                    if (chunk_sz != read(src_fd, rd_buf, chunk_sz)) {
                        printf("%s: %d: failure to read src\n", __func__, __LINE__);
                        flash_close(part_fd);
                        return;
                    }
//                    printf("%s: %d: write raw [%d: %ld:%d]\n", __func__, __LINE__, chunk, (int64_t)total_blocks*sparse_header->blk_sz, chunk_sz);
                    lseek64(part_fd, (uint64_t)total_blocks*sparse_header->blk_sz, SEEK_SET);
                    if (chunk_sz != write(part_fd, rd_buf, chunk_sz)) {
                        printf("%s: %d: failure to write partition\n", __func__, __LINE__);
                        flash_close(part_fd);
                        return;
                    }
                    if (total_blocks > (UINT_MAX - chunk_header.chunk_sz)) {
                        printf("%s: invalid raw chunk size\n", __func__);
                        flash_close(part_fd);
                        return;
                    }
                    total_blocks += chunk_header.chunk_sz;
                    break;
                case CHUNK_TYPE_FILL:
                    if (chunk_header.total_sz != (sparse_header->chunk_hdr_sz + sizeof(uint32_t))) {
                        printf("%s: invalid fill chunk size\n", __func__);
                        flash_close(part_fd);
                        return;
                    }

                    fill_buf = (uint32_t *)memalign(CACHE_LINE, ROUND_UP(sparse_header->blk_sz, CACHE_LINE));
                    if (!fill_buf) {
                        printf("%s: not enough memory for fill chunk\n", __func__);
                        flash_close(part_fd);
                        return;
                    }

                    if (sizeof(uint32_t) != read(src_fd, rd_buf, sizeof(uint32_t))) {
                        printf("%s: %d: failure to read src\n", __func__, __LINE__);
                        flash_close(part_fd);
                        free(fill_buf);
                        return;
                    }

                    fill_val = *rd_buf;
                    //data = (char *) data + sizeof(uint32_t);
                    chunk_blk_cnt = chunk_sz / sparse_header->blk_sz;

                    for (i = 0; i < (int)(sparse_header->blk_sz / sizeof(fill_val)); i++) {
                        fill_buf[i] = fill_val;
                    }

                    for (i = 0; i < (int)chunk_blk_cnt; i++) {
                        /* Make sure that the data written to partition does not exceed partition size */
                        if ((int64_t)total_blocks * (int64_t)sparse_header->blk_sz + sparse_header->blk_sz > part_len) {
                            printf("%s: %d: fill chunk size exceeds partition size\n", __func__, __LINE__);
                            flash_close(part_fd);
                            free(fill_buf);
                            return;
                        }

//                        printf("%s: %d: write fill [%d: %d:%d]\n", __func__, __LINE__, chunk, total_blocks*sparse_header->blk_sz, sparse_header->blk_sz);
                        lseek64(part_fd, (uint64_t)total_blocks*sparse_header->blk_sz, SEEK_SET);
                        if (sparse_header->blk_sz != write(part_fd, fill_buf, sparse_header->blk_sz)) {
                            printf("%s: %d: failure to write partition\n", __func__, __LINE__);
                            flash_close(part_fd);
                            free(fill_buf);
                            return;
                        }

                        total_blocks++;
                    }

                    free(fill_buf);
                    break;
                case CHUNK_TYPE_DONT_CARE:
//                    printf("%s: %d: skip don't care [%d:%d] blocks\n", __func__, __LINE__, chunk, chunk_header.chunk_sz);
                    if (total_blocks > (UINT_MAX - chunk_header.chunk_sz)) {
                        printf("%s: invalid don't care chunk size\n", __func__);
                        flash_close(part_fd);
                        return;
                    }
                    total_blocks += chunk_header.chunk_sz;
                    break;
                case CHUNK_TYPE_CRC32:
                    if (chunk_header.total_sz != sparse_header->chunk_hdr_sz) {
                        printf("%s: invalid crc chunk\n", __func__);
                        flash_close(part_fd);
                        return;
                    }
                    if (total_blocks > (UINT_MAX - chunk_header.chunk_sz)) {
                        printf("%s: invalid crc chunk size\n", __func__);
                        flash_close(part_fd);
                        return;
                    }
//                    printf("%s: %d: skip CRC32 [%d:%d] blocks\n", __func__, __LINE__, chunk, chunk_header.chunk_sz);
                    total_blocks += chunk_header.chunk_sz;
                    lseek64(src_fd, (uint64_t)chunk_sz, SEEK_CUR);
                    break;
                default:
                    printf("%s: Unkown chunk: %x\n",__func__, chunk_header.chunk_type);
                    flash_close(part_fd);
                    return;
            }
        }

        printf("%s: Un-sparse summary: %u blocks of %u expected\n",__func__, total_blocks, sparse_header->total_blks);

        if(total_blocks != sparse_header->total_blks) {
            printf("%s: sparse image write failure\n", __func__);
            flash_close(part_fd);
            return;
        }

        printf("%s: un-sparse image done\n", __func__);
    } else {
        printf("%s: writing %" PRId64" bytes to '%s'\n", __func__, len, arg);

        if (flash_write(part_fd, src_fd, len, 0)) {
            printf("%s: flash write failure\n", __func__);
            flash_close(part_fd);
            return;
        }
    }

    sync();
    flash_close(part_fd);


    if (0) {
        int dbg_fd = open("/sdcard/dump.img", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        part_fd = flash_get_partiton(path);
        part_len = get_file_size64(part_fd);

        printf("%s: dump %s of %" PRId64" lenth\n", __func__, arg, part_len);

        flash_dump(part_fd, dbg_fd, part_len);
        sync();

        close(dbg_fd);
        flash_close(part_fd);
    }

    flash_mount(arg);

    printf("%s: '%s' updated\n", __func__, arg);
}

extern void fastboot_register(const char *prefix, void (* handler)(int src_fd, void *data, int64_t len, const char *arg));

void commands_init(void)
{
    virtual_partition_register("partition-table", cmd_gpt_layout);

    fastboot_register("erase:", cmd_erase);
    fastboot_register("flash:", cmd_flash);
}

