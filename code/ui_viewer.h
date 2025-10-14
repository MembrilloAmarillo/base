#ifndef _UI_VIEWER_H_
#define _UI_VIEWER_H_

typedef struct hk_list hk_list;
struct hk_list {
    U8_String String;
    U8_String ParamsEnabled;

    hk_list* Prev;
    hk_list* Next;
};

#if defined(__GNUC__) || defined(__clang__)
#define SWAP32(x) __builtin_bswap32(x)
#define SWAP64(x) __builtin_bswap64(x)
#else
uint32_t ByteSwap32(uint32_t val) {
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0x00FF00FF);
    return (val << 16) | (val >> 16);
}
#define SWAP32(x) ByteSwap32(x)

uint64_t ByteSwap64(uint64_t val) {
    val = ((val << 8) & 0xFF00FF00FF00FF00ULL) | ((val >> 8) & 0x00FF00FF00FF00FFULL);
    val = ((val << 16) & 0xFFFF0000FFFF0000ULL) | ((val >> 16) & 0x0000FFFF0000FFFFULL);
    val = (val << 32) | (val >> 32);
    return val;
}
#define SWAP64(x) ByteSwap64(x)

#endif

/**
 * Converts a 64-bit packed timestamp into a formatted string.
 * @param buffer The character buffer to write the formatted string into.
 * @param buffer_size The size of the character buffer.
 * @param packed_time The 64-bit value with seconds in the upper 32 bits
 * and nanoseconds in the lower 32 bits.
 */
void format_unix_nanotime(char* buffer, size_t buffer_size, uint64_t packed_time_be);

#endif

#ifdef UI_VIEWER_IMPL

void format_unix_nanotime(char* buffer, size_t buffer_size, uint64_t packed_time_be) {

    uint32_t nanoseconds_be = (uint32_t)(packed_time_be & 0xFFFFFFFF);
    uint32_t seconds_be = (uint32_t)(packed_time_be >> 32);

    time_t seconds_le = (time_t)seconds_be;
    uint32_t nanoseconds_le = nanoseconds_be;

    struct tm *time_info = gmtime(&seconds_le);
    if (time_info == NULL) {
        snprintf(buffer, buffer_size, "Invalid time value");
        return;
    }

    snprintf(buffer, buffer_size,
             "%04d-%02d-%02d %02d:%02d:%02d.%09u",
             time_info->tm_year + 1900,
             time_info->tm_mon + 1,
             time_info->tm_mday,
             time_info->tm_hour,
             time_info->tm_min,
             time_info->tm_sec,
             nanoseconds_le);
}

#endif
