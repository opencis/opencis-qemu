/*
 * Copyright (c) 2024 EEUM, Inc.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>

#define SYSFS_PATH_MAX 256
#define DEVICE_PATH_MAX 256

#define DEFAULT_STEP_SIZE 64
#define ALIGNMENT 2097152  // 2 * 1024 * 1024 (2 MiB)
#define DEFAULT_DEVICE_PATH "dax0.0"

void get_device_size(const char *device_path, off_t *device_size)
{
    char sysfs_path[SYSFS_PATH_MAX];
    FILE *sysfs_file;

    snprintf(sysfs_path, SYSFS_PATH_MAX, "/sys/bus/dax/devices/%s/size",
             device_path);

    sysfs_file = fopen(sysfs_path, "r");
    if (sysfs_file == NULL) {
        perror("Failed to open sysfs file");
        exit(1);
    }

    if (fscanf(sysfs_file, "%ld", device_size) != 1) {
        perror("Failed to read device size from sysfs");
        fclose(sysfs_file);
        exit(1);
    }

    fclose(sysfs_file);
}

int main(int argc, char *argv[])
{
    const char *target_device = (argc > 1) ? argv[1] : DEFAULT_DEVICE_PATH;
    char device_path[DEVICE_PATH_MAX];
    snprintf(device_path, DEVICE_PATH_MAX, "/dev/%s", target_device);
    printf("Device Path: %s\n", device_path);

    int fd;
    void *mmap_ptr;
    off_t pagesize = sysconf(_SC_PAGESIZE);
    printf("Page size: %ld bytes\n", pagesize);

    // Open the character device
    fd = open(device_path, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return 1;
    }

    // Get the device size
    off_t device_size;
    get_device_size(target_device, &device_size);
    printf("Device size: %ld bytes\n", device_size);

    // Memory map the device into user space with proper alignment
    const uint64_t capacity = device_size;
    mmap_ptr = mmap(NULL, capacity, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("MMAP at %p\n", mmap_ptr);

    if (mmap_ptr == MAP_FAILED) {
        perror("Memory mapping failed");
        close(fd);
        return 1;
    }

    uint64_t io_offset;
    char *memory_map = mmap_ptr;

    // Create test data, 64 bytes
    uint64_t *data = (uint64_t *) malloc(DEFAULT_STEP_SIZE);
    if (data == NULL)
    {
        printf("Failed to allocate test data.\n");
        close(fd);
        return 1;
    }
    data[0] = 0x1111111111111111;
    data[1] = 0x2222222222222222;
    data[2] = 0x3333333333333333;
    data[3] = 0x4444444444444444;
    data[4] = 0x5555555555555555;
    data[5] = 0x6666666666666666;
    data[6] = 0x7777777777777777;
    data[7] = 0x8888888888888888;

    // Write to mmap region, 64 bytes at a time
    for (io_offset = 0; io_offset < capacity / DEFAULT_STEP_SIZE; io_offset += DEFAULT_STEP_SIZE) {
        memcpy(&memory_map[io_offset], data, DEFAULT_STEP_SIZE);
        printf("Data written at offset 0x%"PRIx64"\n", io_offset);
    }

    // Unmap the memory-mapped region
    if (munmap(mmap_ptr, capacity) == -1) {
        printf("Unmapping failed\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
