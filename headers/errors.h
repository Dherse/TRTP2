#ifndef ERRORS_H

#define ERRORS_H

#include <errno.h>

typedef enum Errors {
    /** An argument was NULL */
    NULL_ARGUMENT = 0,
    
    /** An item was already allocated */
    ALREADY_ALLOCATED = 1,

    /** An item was already deallocated */
    ALREADY_DEALLOCATED = 2,

    /** Failed to allocate (malloc/calloc) */
    FAILED_TO_ALLOCATE = 3,

    /** Failed to copy (memcpy) */
    FAILED_TO_COPY = 4,

    /** Failed to open (fopen/socket) */
    FAILED_TO_OPEN = 5,

    /** The length of an incoming packet is too short to be decoded (corruption) */
    PACKET_TOO_SHORT = 6,

    /** The length of an incoming packet is too long (corruption) */
    PACKET_TOO_LONG = 7,

    /** The length encoded in a packet is higher than 512 */
    PACKET_INCORRECT_LENGTH = 8,

    /** The packet type is wrong (i.e packet->type = IGNORE) */
    TYPE_IS_WRONG = 9,

    /** Packet truncated of not of type dATA */
    NON_DATA_TRUNCATED = 10,

    /** CRC1 validation failed */
    CRC_VALIDATION_FAILED = 11,

    /** CRC2 validation failed */
    PAYLOAD_VALIDATION_FAILED = 12,

    /** Payload is longer than 512 bytes */
    PAYLOAD_TOO_LONG = 13,

    /** Failed to compile output file format REGEX */
    REGEX_COMPILATION_FAILED = 14,

    /** Missing an option value */
    CLI_O_VALUE_MISSING = 15,

    /** Unknown option */
    CLI_UNKNOWN_OPTION = 16,

    /** Missing the IP filter */
    CLI_IP_MISSING = 17,

    /** Missing the port */
    CLI_PORT_MISSING = 18,

    /** Too many arguments on the CLI */
    CLI_TOO_MANY_ARGUMENTS = 19,

    /** Output file format validation failed (did not pass regex) */
    CLI_FORMAT_VALIDATION_FAILED = 20,
    
    /** IP format invalid */
    CLI_IP_INVALID = 21,

    /** Maximum value invalid */
    CLI_MAX_INVALID = 22,

    /** Port invalid (invalid value/range) */
    CLI_PORT_INVALID = 23,

    /** Number of handlers/receivers invalid */
    CLI_HANDLE_INVALID = 24,

    /** Maximum window size invalid */
    CLI_WINDOW_INVALID = 25,

    /** Failed to convert form a string to an int */
    STR2INT_INCONVERTIBLE = 26,

    /** Failed to convert form a string to an int (overflow) */
    STR2INT_OVERFLOW = 27,

    /** Failed to convert form a string to an int (underflow) */
    STR2INT_UNDERFLOW = 28,

    /** Failed to convert form a string to an int (not the whole string) */
    STR2INT_NOT_END = 29,

    /** Failed to create a pthread_mutex_t */
    FAILED_TO_INIT_MUTEX = 30,

    /** Failed to create a pthread_cond_t */
    FAILED_TO_INIT_COND = 31,

    /** Failed to resize (hashtable) */
    FAILED_TO_RESIZE = 32,

    /** Unknown/internal error */
    UNKNOWN = 255

} errors_t;

#endif
