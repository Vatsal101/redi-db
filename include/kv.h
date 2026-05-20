#ifndef KV_H
#define KV_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "io.h"

// operations with index table
int db_put_table(const char *key, const char *value);
char *db_get_table(const char *key);
int db_delete_table(const char *key);

int db_put_table_internal(const char *key, const char *value);
int db_delete_table_internal(const char *key);

#endif
