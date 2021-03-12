/** @file helper.h
 *  @brief Includes helper functions for the Logs Project
 *
 *  @author Dimitris Pantazis
 */

#ifndef HELPER_H_
#define HELPER_H_

#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>

// branch prediction optimisation
#ifdef __GNUC__
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#define LOG_SEPARATOR "===============================\n"

typedef enum { ERROR,
               WARNING,
               INFO,
               DEBUG } Log_level;

#if DEBUG_LEV
#define fprintf_log(log_level, ...)                          \
    do {                                                     \
        if (1) fprintf_log_internal(log_level, __VA_ARGS__); \
    } while (0)
#else
#define fprintf_log(log_level, ...)                          \
    do {                                                     \
        if (0) fprintf_log_internal(log_level, __VA_ARGS__); \
    } while (0)
#endif  // DEBUG_LEV

#ifndef m_assert
#if DEBUG_LEV                                             // Disable m_assert if production release
#define m_assert(expr, msg) assert(((void)(msg), (expr))) /**< Custom assert function that prints out failure message */
#else
#define m_assert(expr, msg) \
    do {                    \
    } while (0)
#endif  // DEBUG_LEV
#endif  // m_assert

#ifndef fatal
#define fatal() assert(0); /**< Always-enabled fatal assert. Persists in producation releases. */
#endif                     // fatal

#ifndef s_assert
#define s_assert(X) ({ extern int __attribute__((error("assertion failure: '" #X "' not true"))) compile_time_check(); ((X)?0:compile_time_check()),0; })
#endif  // s_assert

#define BIT_SET(a, b) ((a) |= (1ULL << (b)))
#define BIT_CLEAR(a, b) ((a) &= ~(1ULL << (b)))
#define BIT_CHECK(a, b) (!!((a) & (1ULL << (b))))  // '!!' to make sure this returns 0 or 1

// Include for sleep_ms()
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif  // _WIN32

/** 
 * @brief Sleep for certaion milliseconds
 * @param ms Milliseconds to sleep for
 * @todo Replace with uv_sleep()
 */
static inline void sleep_ms(unsigned int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif  // _WIN32
}

/**
 * @brief Get unix time in milliseconds
 * @return Unix time in milliseconds
 * @todo Replace gettimeofday() with uv_gettimeofday()
 */
static inline uint64_t get_unix_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t s1 = (uint64_t)(tv.tv_sec) * 1000;
    uint64_t s2 = (uint64_t)(tv.tv_usec) / 1000;
    return s1 + s2;
}

/**
 * @brief Extract basename from full file path
 * @param path String containing the full path.
 * @return Pointer to the basename string
 */
static inline char *basename(char const *path) {
    char *s = strrchr(path, '/');
    if (!s)
        return strdup(path);
    else
        return strdup(s + 1);
}

/**
 * @brief Custom formatted print implementation
 */
static inline void fprintf_log_internal(Log_level log_level, FILE *stream, const char *format, ...) {
    if (log_level > DEBUG_LEV)
        return;

    struct timeval tv;
    time_t nowtime;
    struct tm *nowtm;
    char tmbuf[64];

    gettimeofday(&tv, NULL);
    nowtime = tv.tv_sec;
    nowtm = localtime(&nowtime);
    // strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);
    strftime(tmbuf, sizeof tmbuf, "%H:%M:%S", nowtm);
    fprintf(stream, "*ND-LGS %s.%06ld ", tmbuf, (long)tv.tv_usec);

    switch (log_level) {
        case WARNING:
            fprintf(stream, "WARNING: ");
            break;
        case INFO:
            fprintf(stream, "INFO: ");
            break;
        case DEBUG:
            fprintf(stream, "DEBUG: ");
            break;
        case ERROR:
            fprintf(stream, "ERROR: ");
            break;
        default:
            break;
    }

    va_list args;
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
}

#define m_malloc(size) m_malloc_int(__FILE__, __FUNCTION__, __LINE__, size)
/**
 * @brief Custom malloc() implementation
 * @details Same as malloc() but will #fatal() if it cannot allocate the memory 
 * @return Pointer to the allocated memory block
 */
static inline void *m_malloc_int(const char *file, const char *function, const unsigned long line, size_t size) {
    void *ptr = malloc(size);
    if (unlikely(!ptr)) {
        fprintf_log(ERROR, stderr, "%s:%d: `m_malloc' failed to allocate memory in function `%s'\n", file, line, function);
        fatal();
    }
    return ptr;
}

#define m_realloc(ptr, size) m_realloc_int(__FILE__, __FUNCTION__, __LINE__, ptr, size)
/**
 * @brief Custom realloc() implementation
 * @details Same as realloc() but will #fatal() if it cannot reallocate the memory 
 * @return Pointer to the reallocated memory block
 */
static inline void *m_realloc_int(const char *file, const char *function, int line, void *ptr, size_t size) {
    if (!ptr)
        return m_malloc(size);

    ptr = realloc(ptr, size);
    if (unlikely(!ptr)) {
        fprintf_log(ERROR, stderr, "%s:%d: `m_realloc' failed to reallocate memory in function `%s'\n", file, line, function);
        fatal();
    }
    return ptr;
}

#endif  // HELPER_H_
