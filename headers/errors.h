#ifndef ERRORS_H

#define ERRORS_H

enum Errors {
    NULL_ARGUMENT,
    
    ALREADY_ALLOCATED,
    ALREADY_DEALLOCATED,
    FAILED_TO_ALLOCATE,
    FAILED_TO_COPY,

    TYPE_IS_WRONG,
    NON_DATA_TRUNCATED,
    CRC_VALIDATION_FAILED,
    PAYLOAD_VALIDATION_FAILED,

    INVALID_LENGTH,
    CLL_FULL, // try to write to a cll node when there are none left
    CLL_EMPTY,

    UNKNOWN

};

#endif