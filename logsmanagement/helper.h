/** @file helper.h
 *  @brief Includes helper functions for the Logs Project
 *
 *  @author Dimitris Pantazis
 */

#ifndef HELPER_H_
#define HELPER_H_

#include "../libnetdata/libnetdata.h"
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>

#define LOG_SEPARATOR "===============================\n"

typedef enum { LOGS_MANAG_ERROR,
               LOGS_MANAG_WARNING,
               LOGS_MANAG_INFO,
               LOGS_MANAG_DEBUG } Log_level;

#if LOGS_MANAG_DEBUG_LEV
#define fprintf_log(log_level, ...)                          \
    do {                                                     \
        if (1) fprintf_log_internal(log_level, __VA_ARGS__); \
    } while (0)
#else
#define fprintf_log(log_level, ...)                          \
    do {                                                     \
        if (0) fprintf_log_internal(log_level, __VA_ARGS__); \
    } while (0)
#endif  // LOGS_MANAG_DEBUG_LEV

#ifndef m_assert
#if LOGS_MANAG_DEBUG_LEV                                             // Disable m_assert if production release
#define m_assert(expr, msg) assert(((void)(msg), (expr))) /**< Custom assert function that prints out failure message */
#else
#define m_assert(expr, msg) \
    do {                    \
    } while (0)
#endif  // LOGS_MANAG_DEBUG_LEV
#endif  // m_assert

// Portable thread local, see https://stackoverflow.com/questions/18298280/how-to-declare-a-variable-as-thread-local-portably
#ifndef thread_local
# if __STDC_VERSION__ >= 201112 && !defined __STDC_NO_THREADS__
#  define thread_local _Thread_local
# elif defined _WIN32 && ( \
       defined _MSC_VER || \
       defined __ICL || \
       defined __DMC__ || \
       defined __BORLANDC__ )
#  define thread_local __declspec(thread) 
/* note that ICC (linux) and Clang are covered by __GNUC__ */
# elif defined __GNUC__ || \
       defined __SUNPRO_C || \
       defined __xlC__
#  define thread_local __thread
# else
#  error "Cannot define thread_local"
# endif
#endif

#ifndef COMPILE_TIME_ASSERT // https://stackoverflow.com/questions/3385515/static-assert-in-c
#define STATIC_ASSERT(COND,MSG) typedef char static_assertion_##MSG[(!!(COND))*2-1]
// token pasting madness:
#define COMPILE_TIME_ASSERT3(X,L) STATIC_ASSERT(X,static_assertion_at_line_##L)
#define COMPILE_TIME_ASSERT2(X,L) COMPILE_TIME_ASSERT3(X,L)
#define COMPILE_TIME_ASSERT(X)    COMPILE_TIME_ASSERT2(X,__LINE__)
#endif  // COMPILE_TIME_ASSERT

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
 * @brief Extract file_basename from full file path
 * @param path String containing the full path.
 * @return Pointer to the file_basename string
 */
static inline char *get_basename(char const *path) {
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
    if (log_level > LOGS_MANAG_DEBUG_LEV) 
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
        case LOGS_MANAG_WARNING:
            fprintf(stream, "LOGS_MANAG_WARNING: ");
            break;
        case LOGS_MANAG_INFO:
            fprintf(stream, "LOGS_MANAG_INFO: ");
            break;
        case LOGS_MANAG_DEBUG:
            fprintf(stream, "LOGS_MANAG_DEBUG: ");
            break;
        case LOGS_MANAG_ERROR:
            fprintf(stream, "LOGS_MANAG_ERROR: ");
            break;
        default:
            break;
    }

    va_list args;
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
}

#endif  // HELPER_H_
