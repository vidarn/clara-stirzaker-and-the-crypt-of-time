#include <stdint.h>
#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint32_t

#define s8  int8_t
#define s16 int16_t
#define s32 int32_t
#define s64 int32_t

#define MIN_MAX(type) \
static inline \
type __attribute__((overloadable)) min(type const x, type const y) \
{ return y < x ? y : x;} \
static inline \
type __attribute__((overloadable)) max(type const x, type const y) \
{ return y > x ? y : x;}

MIN_MAX(int)
MIN_MAX(unsigned)
MIN_MAX(float)
MIN_MAX(double)

#undef MIN_MAX

