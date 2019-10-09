/**
 * This is an implementation of a "super-fast" hashing algorithm.
 * 
 * It solves the complex problem of hashing the IPv6 address and the
 * port of the client using a single hash function taking as parameter
 * the IP followed by the port in a single byte array.
 * 
 * The performance should be sufficient.
 * 
 * SOURCE : http://www.azillionmonkeys.com/qed/hash.html
 */
#include <stdlib.h>
#include <stdint.h>

#ifndef SF_H

#define SF_H

#undef get16bits

#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)

#define get16bits(d) (*((const uint16_t *) (d)))

#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

uint32_t SuperFastHash (const char * data, int len);

#endif