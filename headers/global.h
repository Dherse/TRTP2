#ifndef GLOBAL_H

#define GLOBAL_H

#define true 1
#define false 0
typedef int bool;

#define MAX_PACKET_SIZE 528;
#define MAX_PAYLOAD_SIZE 512;

#ifndef GETSET

#define GETSET(owner, type, var) \
    // Gets ##var from type \
    void set_##var(owner *self, type val);\
    // Sets ##var from type \
    type get_##var(owner *self);

#define GETSET_IMPL(owner, type, var) \
    void set_##var(owner *self, type val) {\
        self->var = val;\
    }\
    type get_##var(owner *self) {\
        return self->var;\
    }

#endif

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#include <zlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <regex.h>

#include <pthread.h>

#include "errors.h"
#endif