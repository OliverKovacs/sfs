#include <stdint.h>

void _memcpy(void *dest, const void *src, size_t n) {
    for (size_t i = 0; i < n; i++) {
        ((uint8_t*)dest)[i] = ((uint8_t *)src)[i];
    }
}

void _memset(void *s, uint8_t c, size_t n) {
    for (size_t i = 0; i < n; i++) {
        ((uint8_t *)s)[i] = c;
    }
}

size_t _strcmp(const char *s1, const char *s2) {
    while (*s1 != '\0') {
        if (*s1 != *s2) return 1;
        s1++;
        s2++;
    }
    return *s2 != '\0';
}

size_t _strlen(const char *s) {
    size_t size = 0;
    while (*s != '\0') {
        size++;
        s++;
    }
    return size;
}
