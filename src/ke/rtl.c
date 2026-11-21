#include "../include/rtl.h"

void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void* dest, int c, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    for (size_t i = 0; i < n; i++) {
        d[i] = (uint8_t)c;
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return (int)p1[i] - (int)p2[i];
        }
    }
    return 0;
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) {
        len++;
    }
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i] || s1[i] == '\0') {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
    }
    return 0;
}

int toupper(int c) {
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && (toupper((unsigned char)*s1) == toupper((unsigned char)*s2))) {
        s1++;
        s2++;
    }
    return toupper((unsigned char)*s1) - toupper((unsigned char)*s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int c1 = toupper((unsigned char)s1[i]);
        int c2 = toupper((unsigned char)s2[i]);
        if (c1 != c2 || s1[i] == '\0') {
            return c1 - c2;
        }
    }
    return 0;
}

char* strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == '\0') ? (char*)s : NULL;
}

int abs(int x) {
    return (x < 0) ? -x : x;
}

long labs(long x) {
    return (x < 0) ? -x : x;
}

int atoi(const char* s) {
    int res = 0;
    int sign = 1;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res * sign;
}

static void put_char(char* buf, size_t size, size_t* pos, char c) {
    if (*pos + 1 < size) {
        buf[*pos] = c;
    }
    (*pos)++;
}

static void put_str(char* buf, size_t size, size_t* pos, const char* s) {
    if (!s) s = "(null)";
    while (*s) {
        put_char(buf, size, pos, *s++);
    }
}

static void put_uint(char* buf, size_t size, size_t* pos, uint64_t val, int base, bool upper, int width, char pad) {
    char tmp[32];
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0) {
            tmp[i++] = digits[val % base];
            val /= base;
        }
    }
    while (i < width) {
        put_char(buf, size, pos, pad);
        width--;
    }
    while (i > 0) {
        put_char(buf, size, pos, tmp[--i]);
    }
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list args) {
    size_t pos = 0;
    while (*fmt) {
        if (*fmt != '%') {
            put_char(buf, size, &pos, *fmt++);
            continue;
        }
        fmt++;
        char pad = ' ';
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        bool is_long = false;
        if (*fmt == 'l') {
            is_long = true;
            fmt++;
            if (*fmt == 'l') fmt++;
        }
        switch (*fmt) {
            case 'c': {
                char c = (char)va_arg(args, int);
                put_char(buf, size, &pos, c);
                break;
            }
            case 's': {
                const char* s = va_arg(args, const char*);
                put_str(buf, size, &pos, s);
                break;
            }
            case 'd':
            case 'i': {
                int64_t v = is_long ? va_arg(args, int64_t) : (int64_t)va_arg(args, int);
                if (v < 0) {
                    put_char(buf, size, &pos, '-');
                    v = -v;
                }
                put_uint(buf, size, &pos, (uint64_t)v, 10, false, width, pad);
                break;
            }
            case 'u': {
                uint64_t v = is_long ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int);
                put_uint(buf, size, &pos, v, 10, false, width, pad);
                break;
            }
            case 'x': {
                uint64_t v = is_long ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int);
                put_uint(buf, size, &pos, v, 16, false, width, pad);
                break;
            }
            case 'X': {
                uint64_t v = is_long ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int);
                put_uint(buf, size, &pos, v, 16, true, width, pad);
                break;
            }
            case 'p': {
                uint64_t v = (uint64_t)va_arg(args, void*);
                put_str(buf, size, &pos, "0x");
                put_uint(buf, size, &pos, v, 16, false, 16, '0');
                break;
            }
            case '%': {
                put_char(buf, size, &pos, '%');
                break;
            }
            default:
                put_char(buf, size, &pos, '%');
                put_char(buf, size, &pos, *fmt);
                break;
        }
        if (*fmt) fmt++;
    }
    if (size > 0) {
        buf[(pos < size) ? pos : (size - 1)] = '\0';
    }
    return (int)pos;
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return ret;
}
