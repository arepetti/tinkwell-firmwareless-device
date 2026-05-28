/*
 * pal_flash_posix.c -- Flash storage via regular files.
 *
 * Each "partition" is a file under $HOME/.tw-device/<label>.bin.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_flash.h"
#include "pal_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define TAG "flash"
#define MAX_LABEL 32

static char base_dir[512];

static void flash_path(const char *label, char *out, size_t out_sz)
{
    snprintf(out, out_sz, "%s/%s.bin", base_dir, label);
}

tw_err_t pal_flash_init(const char *label)
{
    TW_UNUSED(label);
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(base_dir, sizeof(base_dir), "%s/.tw-device", home);
    mkdir(base_dir, 0755);
    return TW_OK;
}

tw_err_t pal_flash_erase(const char *label)
{
    char path[600];
    flash_path(label, path, sizeof(path));
    remove(path);
    return TW_OK;
}

tw_err_t pal_flash_write(const char *label, size_t offset,
                         const void *data, size_t len)
{
    char path[600];
    flash_path(label, path, sizeof(path));

    FILE *f = fopen(path, "r+b");
    if (!f) f = fopen(path, "wb");
    if (!f) return TW_ERR_IO;

    fseek(f, (long)offset, SEEK_SET);
    size_t n = fwrite(data, 1, len, f);
    fclose(f);
    return n == len ? TW_OK : TW_ERR_IO;
}

tw_err_t pal_flash_read(const char *label, size_t offset,
                        void *buf, size_t len)
{
    char path[600];
    flash_path(label, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return TW_ERR_NOT_FOUND;

    fseek(f, (long)offset, SEEK_SET);
    size_t n = fread(buf, 1, len, f);
    fclose(f);
    return n == len ? TW_OK : TW_ERR_IO;
}

size_t pal_flash_size(const char *label)
{
    char path[600];
    flash_path(label, path, sizeof(path));

    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (size_t)st.st_size;
}

static struct {
    char   label[MAX_LABEL];
    void  *addr;
    size_t len;
    int    fd;
} mmap_state;

const void *pal_flash_mmap(const char *label, size_t *out_len)
{
    if (mmap_state.addr) {
        munmap(mmap_state.addr, mmap_state.len);
        close(mmap_state.fd);
        memset(&mmap_state, 0, sizeof(mmap_state));
    }

    char path[600];
    flash_path(label, path, sizeof(path));

    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size == 0) { close(fd); return NULL; }

    void *addr = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) { close(fd); return NULL; }

    snprintf(mmap_state.label, sizeof(mmap_state.label), "%s", label);
    mmap_state.addr = addr;
    mmap_state.len  = (size_t)st.st_size;
    mmap_state.fd   = fd;

    if (out_len) *out_len = mmap_state.len;
    return addr;
}

void pal_flash_munmap(const char *label)
{
    if (strcmp(mmap_state.label, label) == 0 && mmap_state.addr) {
        munmap(mmap_state.addr, mmap_state.len);
        close(mmap_state.fd);
        memset(&mmap_state, 0, sizeof(mmap_state));
    }
}
