/*
 * storage.h
 *
 *  Created on: 28 thg 11, 2025
 *      Author: buiqu
 */

#ifndef STORAGE_H_
#define STORAGE_H_

#include "app_data.h"

void storage_init(void);
bool storage_save(SystemData_t *data);
bool storage_read_oldest(SystemData_t *data);
void storage_mark_oldest_as_sent(void);
bool storage_is_empty(void);

#endif /* STORAGE_H_ */
