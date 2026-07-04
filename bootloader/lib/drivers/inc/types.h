/*
 * types.h — common types
 */

#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>

typedef enum {
    STATUS_OK       = 0x00U,
    STATUS_ERROR    = 0x01U,
    STATUS_BUSY     = 0x02U,
    STATUS_TIMEOUT  = 0x03U
} status_t;

#endif /* TYPES_H_ */
