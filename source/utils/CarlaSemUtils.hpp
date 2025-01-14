/*
 * Carla semaphore utils
 * Copyright (C) 2013-2015 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the doc/GPL.txt file.
 */

#ifndef CARLA_SEM_UTILS_HPP_INCLUDED
#define CARLA_SEM_UTILS_HPP_INCLUDED

#include "CarlaUtils.hpp"

#include <ctime>

#define CARLA_USE_FUTEXES

#ifdef CARLA_OS_WIN
struct carla_sem_t { HANDLE handle; };
#elif defined(CARLA_OS_MAC)
// TODO
struct carla_sem_t { char dummy; };
#elif defined(CARLA_USE_FUTEXES)
# include <cerrno>
# include <syscall.h>
# include <sys/time.h>
# include <linux/futex.h>
struct carla_sem_t { int count; };
#else
# include <cerrno>
# include <semaphore.h>
# include <sys/time.h>
# include <sys/types.h>
struct carla_sem_t { sem_t sem; };
#endif

/*
 * Create a new semaphore, pre-allocated version.
 */
static inline
bool carla_sem_create2(carla_sem_t& sem) noexcept
{
    carla_zeroStruct(sem);
#if defined(CARLA_OS_WIN)
    SECURITY_ATTRIBUTES sa;
    carla_zeroStruct(sa);
    sa.nLength        = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    sem.handle = ::CreateSemaphore(&sa, 0, 1, nullptr);

    return (sem.handle != INVALID_HANDLE_VALUE);
#elif defined(CARLA_OS_MAC)
    return false; // TODO
#elif defined(CARLA_USE_FUTEXES)
    sem.count = 0;
    return true;
#else
    return (::sem_init(&sem.sem, 1, 0) == 0);
#endif
}

/*
 * Create a new semaphore.
 */
static inline
carla_sem_t* carla_sem_create() noexcept
{
    if (carla_sem_t* const sem = (carla_sem_t*)std::malloc(sizeof(carla_sem_t)))
    {
        if (carla_sem_create2(*sem))
            return sem;

        std::free(sem);
    }

    return nullptr;
}

/*
 * Destroy a semaphore, pre-allocated version.
 */
static inline
void carla_sem_destroy2(carla_sem_t& sem) noexcept
{
#if defined(CARLA_OS_WIN)
    ::CloseHandle(sem.handle);
#elif defined(CARLA_OS_MAC)
    // TODO
#elif defined(CARLA_USE_FUTEXES)
    // nothing to do
    (void)sem;
#else
    ::sem_destroy(&sem.sem);
#endif
}

/*
 * Destroy a semaphore.
 */
static inline
void carla_sem_destroy(carla_sem_t* const sem) noexcept
{
    CARLA_SAFE_ASSERT_RETURN(sem != nullptr,);

    carla_sem_destroy2(*sem);
    std::free(sem);
}

/*
 * Post semaphore (unlock).
 */
static inline
void carla_sem_post(carla_sem_t& sem) noexcept
{
#ifdef CARLA_OS_WIN
    ::ReleaseSemaphore(sem.handle, 1, nullptr);
#elif defined(CARLA_OS_MAC)
    // TODO
#elif defined(CARLA_USE_FUTEXES)
    const bool unlocked = __sync_bool_compare_and_swap(&sem.count, 0, 1);
    CARLA_SAFE_ASSERT_RETURN(unlocked,);
    ::syscall(__NR_futex, &sem.count, FUTEX_WAKE, 1, nullptr, nullptr, 0);
#else
    ::sem_post(&sem.sem);
#endif
}

/*
 * Wait for a semaphore (lock).
 */
static inline
bool carla_sem_timedwait(carla_sem_t& sem, const uint msecs) noexcept
{
    CARLA_SAFE_ASSERT_RETURN(msecs > 0, false);

#if defined(CARLA_OS_WIN)
    return (::WaitForSingleObject(sem.handle, secs*1000) == WAIT_OBJECT_0);
#elif defined(CARLA_OS_MAC)
    return false; // TODO
#elif defined(CARLA_USE_FUTEXES)
    timespec timeout;
    timeout.tv_sec  = static_cast<time_t>(msecs / 1000);
    timeout.tv_nsec = static_cast<time_t>(msecs % 1000);

    for (; ! __sync_bool_compare_and_swap(&sem.count, 1, 0);)
    {
        if (::syscall(__NR_futex, &sem.count, FUTEX_WAIT, 0, &timeout, nullptr, 0) == 0)
        {
            __sync_fetch_and_sub(&sem.count, 1);
            return true;
        }

        //if (errno == EAGAIN)
        //   continue;

        return false;
    }

    return true;
#else
    if (::sem_trywait(&sem.sem) == 0)
        return true;

    timespec timeout;
    ::clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec  += static_cast<time_t>(msecs / 1000);
    timeout.tv_nsec += static_cast<time_t>(msecs % 1000);

    try {
        return (::sem_timedwait(&sem.sem, &timeout) == 0);
    } CARLA_SAFE_EXCEPTION_RETURN("sem_timedwait", false);
#endif
}

// -----------------------------------------------------------------------

#endif // CARLA_SEM_UTILS_HPP_INCLUDED
