#include "../headers/buffer.h"
#include "../headers/errors.h"
#include <pthread.h>
#include <errno.h>

/*
 * Refer to headers/buffer.h
 */
int create_buffer(buf_t *buf) {
    buf = (buf_t *) calloc((size_t) 1, sizeof(buf_t));
    if(buf == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }
    buf->last_read = 0;
    buf->last_written = 0;

    for(int i = 0; i < MAX_WINDOW_SIZE; i++) {
        if(pthread_mutex_init(&(buf->nodes[i].mut), NULL) != 0) {
            deallocate_buffer(buf);
            //errno
            return -1;
        }
        if(alloc_packet(buf->nodes[i].packet) != 0) {
            deallocate_buffer(buf);
            return -1;
        }
    }
    return 0;
}

/*
 * Refer to headers/buffer.h
 */
void deallocate_buffer(buf_t *buf) {
    if(buf == NULL) {
        errno = NULL_ARGUMENT;
        return;
    }
}

/*
 * Refer to headers/buffer.h
 */
uint8_t hash(uint8_t number) {
    return number & 0b00011111;
}