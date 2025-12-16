#ifndef KV_H

#define KV_H

#include "io.h"

int db_put(const char *key, const char *value);

char *db_get(const char *key);

int db_delete(const char *key);

#define HEADER_LEN 11

#endif
