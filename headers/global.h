#ifndef GLOBAL_H

#define GLOBAL_H

#define true 1
#define false 0
typedef int bool;

#define MAX_PACKET_SIZE 528;
#define MAX_PAYLOAD_SIZE 512;

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
#include <arpa/inet.h>
#include <netdb.h>
#include <regex.h>

#include "errors.h"
#include "packet.h"
#include "cli.h"
#include "buffer.h"

#endif