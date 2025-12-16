#include <stdio.h>
#include <stdlib.h>
#include "kv.h"
#include "io.h"

int main(void) {
    printf("Opening database...\n");
    if (db_open("test.db") != 0) { 
        perror("Failed to open database"); 
        return 1; 
    }
    printf("Database opened successfully\n");

    printf("Putting key 'foo'...\n");
    if (db_put("foo", "hello") != 0) { 
        printf("put 'foo' failed\n"); 
        return 1;
    }
    printf("Put 'foo' successful\n");

    printf("Putting key 'bar'...\n");
    if (db_put("bar", "world") != 0) { 
        printf("put 'bar' failed\n"); 
        return 1;
    }
    printf("Put 'bar' successful\n");

    printf("Getting key 'foo'...\n");
    char *v = db_get("foo");
    if (v) { 
        printf("foo -> %s\n", v); 
        free(v); 
    } else {
        printf("foo not found\n");
    }

    printf("Getting key 'bar'...\n");
    v = db_get("bar");
    if (v) { 
        printf("bar -> %s\n", v); 
        free(v); 
    } else {
        printf("bar not found\n");
    }

    printf("Closing database...\n");
    db_close();
    printf("Done!\n");
    return 0;
}