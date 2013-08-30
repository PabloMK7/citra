#ifndef COMMON_CRC_H_
#define COMMON_CRC_H_

#include "types.h"
#include "platform.h"

#define CRC_ROTL(crc) crc32_table[3][((crc) & 0xFF)] ^ crc32_table[2][((crc >> 8) & 0xFF)] ^ \
        crc32_table[1][((crc >> 16) & 0xFF)] ^ crc32_table[0][((crc >> 24))]

// Some definitions for using the X86 CRC32 instruction on different platforms. Keep in mind, you 
// should check for X86/X64 architecture support before using these, as well as for SSE 4.2 (see the
// x86_utils module).

#if defined(EMU_ARCHITECTURE_X86) || defined(EMU_ARCHITECTURE_X64)

#if EMU_PLATFORM == PLATFORM_WINDOWS

#include <nmmintrin.h>

#ifdef EMU_ARCHITECTURE_X64
static inline u64 InlineCrc32_U64(u64 crc, u64 value) {
    return _mm_crc32_u64(crc, value);
}
#endif
static inline u32 InlineCrc32_U32(u32 crc, u64 value) {
    return _mm_crc32_u32(crc, static_cast<u32>(value));
}

static inline u32 InlineCrc32_U8(u32 crc, u8 value) {
    return _mm_crc32_u8(crc, value);
}

#elif GCC_VERSION_AVAILABLE(4, 5) && defined(__SSE4_2__)

extern inline unsigned int __attribute__((
    __gnu_inline__, __always_inline__, __artificial__))
InlineCrc32_U8(unsigned int __C, unsigned char __V) {
    return __builtin_ia32_crc32qi(__C, __V);
}
#ifdef EMU_ARCHITECTURE_X64
extern inline unsigned long long __attribute__((
    __gnu_inline__, __always_inline__, __artificial__))
InlineCrc32_U64(unsigned long long __C, unsigned long long __V) {
    return __builtin_ia32_crc32di(__C, __V);
}
#else
extern inline unsigned int __attribute__((
    __gnu_inline__, __always_inline__, __artificial__))
InlineCrc32_U32(unsigned int __C, unsigned int __V) {
    return __builtin_ia32_crc32si (__C, __V);
}
#endif  // EMU_ARCHITECTURE_X64

#else

// GCC 4.4.x and earlier: use inline asm, or msse4.2 flag not set

static inline u64 InlineCrc32_U64(u64 crc, u64 value) {
    asm("crc32q %[value], %[crc]\n" : [crc] "+r" (crc) : [value] "rm" (value));
    return crc;
}

static inline u32 InlineCrc32_U32(u32 crc, u64 value) {
    asm("crc32l %[value], %[crc]\n" : [crc] "+r" (crc) : [value] "rm" (value));
    return crc;
}

static inline u32 InlineCrc32_U8(u32 crc, u8 value) {
    asm("crc32b %[value], %[crc]\n" : [crc] "+r" (crc) : [value] "rm" (value));
    return crc;
}
#endif

#endif  // EMU_ARCHITECTURE_X86 or EMU_ARCHITECTURE_X64

extern u32 crc32_table[4][256];

void Init_CRC32_Table();
u32 GenerateCRC(u8 *StartAddr, u32 Len);

#endif // COMMON_CRC_H_
