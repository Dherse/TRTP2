#ifndef ERRORS_H

#define ERRORS_H

#include <errno.h>

typedef enum Errors {
    /** An argument was NULL */
    NULL_ARGUMENT,
    
    /** An item was already allocated */
    ALREADY_ALLOCATED,

    /** An item was already deallocated */
    ALREADY_DEALLOCATED,

    /** Failed to allocate (malloc/calloc) */
    FAILED_TO_ALLOCATE,

    /** Failed to copy (memcpy) */
    FAILED_TO_COPY,

    /** Failed to open (fopen/socket) */
    FAILED_TO_OPEN,

    /** The length of an incoming packet is too short to be decoded (corruption) */
    PACKET_TOO_SHORT,

    /** The length of an incoming packet is too long (corruption) */
    PACKET_TOO_LONG,

    /** The length encoded in a packet is higher than 512 */
    PACKET_INCORRECT_LENGTH,

    /** The packet type is wrong (i.e packet->type = IGNORE) */
    TYPE_IS_WRONG,

    /** Packet truncated of not of type dATA */
    NON_DATA_TRUNCATED,

    /** CRC1 validation failed */
    CRC_VALIDATION_FAILED,

    /** CRC2 validation failed */
    PAYLOAD_VALIDATION_FAILED,

    /** Payload is longer than 512 bytes */
    PAYLOAD_TOO_LONG,

    /** Failed to compile output file format REGEX */
    REGEX_COMPILATION_FAILED,

    /** Missing an option value */
    CLI_O_VALUE_MISSING,

    /** Unknown option */
    CLI_UNKNOWN_OPTION,

    /** Missing the IP filter */
    CLI_IP_MISSING,

    /** Missing the port */
    CLI_PORT_MISSING,

    /** Too many arguments on the CLI */
    CLI_TOO_MANY_ARGUMENTS,

    /** Output file format validation failed (did not pass regex) */
    CLI_FORMAT_VALIDATION_FAILED,
    
    /** IP format invalid */
    CLI_IP_INVALID,

    /** Maximum value invalid */
    CLI_MAX_INVALID,

    /** Port invalid (invalid value/range) */
    CLI_PORT_INVALID,

    /** Number of handlers/receivers invalid */
    CLI_HANDLE_INVALID,

    /** Maximum window size invalid */
    CLI_WINDOW_INVALID,

    /** Failed to convert form a string to an int */
    STR2INT_INCONVERTIBLE,

    /** Failed to convert form a string to an int (overflow) */
    STR2INT_OVERFLOW,

    /** Failed to convert form a string to an int (underflow) */
    STR2INT_UNDERFLOW,

    /** Failed to convert form a string to an int (not the whole string) */
    STR2INT_NOT_END,

    /** Failed to create a pthread_mutex_t */
    FAILED_TO_INIT_MUTEX,

    /** Failed to create a pthread_cond_t */
    FAILED_TO_INIT_COND,

    /** Failed to resize (hashtable) */
    FAILED_TO_RESIZE,

    /** Unknown/internal error */
    UNKNOWN

} errors_t;

#endif