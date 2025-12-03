/*
 * storage.c
 *
 *  Created on: 28 thg 11, 2025
 *      Author: buiqu
 */
#include "storage.h"

#define STORAGE_SIZE 100
static SystemData_t mock_flash[STORAGE_SIZE];
static int write_idx = 0;
static int read_idx = 0;
static bool is_full = false;

void storage_init(void) {
    write_idx = 0; read_idx = 0; is_full = false;
}

bool storage_save(SystemData_t *data) {
    if (is_full) return false;
    mock_flash[write_idx] = *data;
    write_idx = (write_idx + 1) % STORAGE_SIZE;
    if (write_idx == read_idx) is_full = true;
    return true;
}

bool storage_read_oldest(SystemData_t *data) {
    if (write_idx == read_idx && !is_full) return false;
    *data = mock_flash[read_idx];
    return true;
}

void storage_mark_oldest_as_sent(void) {
    if (write_idx == read_idx && !is_full) return;
    read_idx = (read_idx + 1) % STORAGE_SIZE;
    is_full = false;
}

bool storage_is_empty(void) {
    return (write_idx == read_idx && !is_full);
}

