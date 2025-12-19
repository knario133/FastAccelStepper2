#ifndef POORMANFLOAT_H
#define POORMANFLOAT_H

#include <stdint.h>
#include "common.h"

#if defined(SUPPORT_ESP32)

// Use native float for ESP32
typedef float upm_float;

#define UPM_CONST_128E12 ((upm_float)128e12)
#define UPM_CONST_16E6 ((upm_float)16000000.0)
#define UPM_CONST_500 ((upm_float)500.0)
#define UPM_CONST_1000 ((upm_float)1000.0)
#define UPM_CONST_2000 ((upm_float)2000.0)
#define UPM_CONST_32000 ((upm_float)32000.0)
#define UPM_CONST_16E6_DIV_SQRT_OF_2 ((upm_float)(16000000.0 / 1.41421356))
#define UPM_CONST_21E6 ((upm_float)21000000.0)
#define UPM_CONST_42000 ((upm_float)42000.0)
#define UPM_CONST_21E6_DIV_SQRT_OF_2 ((upm_float)(21000000.0 / 1.41421356))
#define UPM_CONST_2205E11 ((upm_float)2.205e11)

static inline upm_float upm_from(uint8_t x) { return (float)x; }
static inline upm_float upm_from(uint16_t x) { return (float)x; }
static inline upm_float upm_from(uint32_t x) { return (float)x; }

static inline uint16_t upm_to_u16(upm_float x) { return (uint16_t)x; }
static inline uint32_t upm_to_u32(upm_float x) { return (uint32_t)x; }

static inline upm_float upm_shl(upm_float x, uint8_t n) { return x * (1 << n); }
static inline upm_float upm_shr(upm_float x, uint8_t n) { return x / (1 << n); }

static inline upm_float upm_multiply(upm_float x, upm_float y) { return x * y; }
static inline upm_float upm_reciprocal(upm_float x) { return 1.0f / x; }
static inline upm_float upm_square(upm_float x) { return x * x; }
static inline upm_float upm_rsquare(upm_float x) { return 1.0f / (x * x); }
static inline upm_float upm_rsqrt(upm_float x) { return 1.0f / sqrtf(x); }
static inline upm_float upm_divide(upm_float x, upm_float y) { return x / y; }

#else

// Original PoorManFloat implementation for AVR/SAM
typedef uint16_t upm_float;
#define UPM_CONST_128E12 ((upm_float)0xaed0)
#define UPM_CONST_16E6 ((upm_float)0x97e8)
#define UPM_CONST_500 ((upm_float)0x88f4)
#define UPM_CONST_1000 ((upm_float)0x89f4)
#define UPM_CONST_2000 ((upm_float)0x8af4)
#define UPM_CONST_32000 ((upm_float)0x8ef4)
#define UPM_CONST_16E6_DIV_SQRT_OF_2 ((upm_float)0x9759)
#define UPM_CONST_21E6 ((upm_float)0x9840)
#define UPM_CONST_42000 ((upm_float)0x8f48)
#define UPM_CONST_21E6_DIV_SQRT_OF_2 ((upm_float)0x97c4)
#define UPM_CONST_2205E11 ((upm_float)0xaf91)

upm_float upm_from(uint8_t x);
upm_float upm_from(uint16_t x);
upm_float upm_from(uint32_t x);

uint16_t upm_to_u16(upm_float x);
uint32_t upm_to_u32(upm_float x);

upm_float upm_shl(upm_float x, uint8_t n);
upm_float upm_shr(upm_float x, uint8_t n);

upm_float upm_multiply(upm_float x, upm_float y);  // TESTED
upm_float upm_reciprocal(upm_float x);             // TESTED
upm_float upm_square(upm_float x);                 // TESTED = x*x
upm_float upm_rsquare(upm_float x);  // TESTED Reciprocal square = 1/(x*x)
upm_float upm_rsqrt(upm_float x);    // TESTED. Reciprocal sqrt() = 1/sqrt(x)

// OLD
upm_float upm_divide(upm_float x, upm_float y);

#endif
#endif
