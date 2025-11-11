// common_defs.h
#ifndef COMMON_DEFS_H
#define COMMON_DEFS_H

#include <stdio.h>
#include <stdlib.h> // For exit and EXIT_FAILURE
#include <pthread.h>
// ANSI color codes
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_RESET "\x1b[0m"

// Macros for printing colored text
#define printf_red(format, ...) printf(ANSI_COLOR_RED format ANSI_COLOR_RESET, ##__VA_ARGS__)
#define printf_green(format, ...) printf(ANSI_COLOR_GREEN format ANSI_COLOR_RESET, ##__VA_ARGS__)
#define printf_yellow(format, ...) printf(ANSI_COLOR_YELLOW format ANSI_COLOR_RESET, ##__VA_ARGS__)

// Custom assert macro with color
#define my_assert(expr)                                                                                  \
    do                                                                                                   \
    {                                                                                                    \
        if (!(expr))                                                                                     \
        {                                                                                                \
            printf_red("[FAIL] - Assertion failed: %s, file %s, line %d.\n", #expr, __FILE__, __LINE__); \
        }                                                                                                \
    } while (0)

// void my_assert_impl(int condition, const char *test_name, const char *file, int line, const char *assertion)
// {
//     if (!condition)
//     {
//         printf_red("[FAIL] - %s (Assertion '%s' failed at %s:%d.)\n", test_name, file, line);
//         exit(EXIT_FAILURE); // Optionally exit the program
//     }
//     else
//     {
//         printf_green("[PASS] - %s.\n", test_name);
//     }
// }

// #define my_assert(condition, test_name) my_assert_impl(condition, test_name, __FILE__, __LINE__, #condition)

typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;       // The current number of threads that have reached the barrier
    int num_threads; // The total number of threads expected at the barrier
} my_barrier_t;

// Initialize the custom barrier
int my_barrier_init(my_barrier_t *barrier, int num_threads)
{
    int result;
    barrier->count = 0;
    barrier->num_threads = num_threads;
    result = pthread_mutex_init(&barrier->mutex, NULL);
    if (result != 0)
        return result;
    result = pthread_cond_init(&barrier->cond, NULL);
    if (result != 0)
        return result;
    return 0;
}

// The barrier wait function
int my_barrier_wait(my_barrier_t *barrier)
{
    pthread_mutex_lock(&barrier->mutex);

    // Increase the count of threads that have reached the barrier
    barrier->count++;

    if (barrier->count == barrier->num_threads)
    {
        // Last thread to reach the barrier wakes up all others
        barrier->count = 0;                     // Reset for potential reuse
        pthread_cond_broadcast(&barrier->cond); // Wake up all threads
    }
    else
    {
        // Wait until all threads have reached the barrier
        pthread_cond_wait(&barrier->cond, &barrier->mutex);
    }

    pthread_mutex_unlock(&barrier->mutex);
    return 0;
}

// Destroy the custom barrier
int my_barrier_destroy(my_barrier_t *barrier)
{
    pthread_mutex_destroy(&barrier->mutex);
    pthread_cond_destroy(&barrier->cond);
    return 0;
}

#endif // COMMON_DEFS_H