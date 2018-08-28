/*
 *
 *    Copyright (c) 2015-2018 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/*
 *    Description:
 *      This file implments functions used by Clang during C++ static
 *      initialization; specifically, locks to guard that initialization. These
 *      are necessary when linking against a "reduced" standard library.
 *
 */

#include <stdint.h>

// forward declarations
void __cxa_pure_virtual(void);
int __cxa_guard_acquire(uint64_t* guard_object);
void __cxa_guard_release(uint64_t* guard_object);
void __cxa_guard_abort(uint64_t* guard_object);

void _ZdlPv(void*);

// a global
void *__dso_handle;

// Defines to squelch warnings
#define abort_message(s)

void __cxa_pure_virtual(void)
{ 
    while (1);
}

// void operator delete(void* p)
void _ZdlPv(void* unused)
{
    while(1);
}


#if 0
static pthread_mutex_t __guard_mutex;
static pthread_once_t __once_control = PTHREAD_ONCE_INIT;


static void makeRecusiveMutex()
{
    pthread_mutexattr_t recursiveMutexAttr;
    pthread_mutexattr_init(&recursiveMutexAttr);
    pthread_mutexattr_settype(&recursiveMutexAttr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&__guard_mutex, &recursiveMutexAttr);
}

__attribute__((noinline))
static pthread_mutex_t* guard_mutex()
{
    pthread_once(&__once_control, &makeRecusiveMutex);
    return &__guard_mutex;
}
#endif

// helper functions for getting/setting flags in guard_object
static uint8_t initializerHasRun(uint64_t* guard_object)
{
    return ( *((uint8_t*)guard_object) != 0 );
}

static void setInitializerHasRun(uint64_t* guard_object)
{
    *((uint8_t*)guard_object)  = 1;
}

static uint8_t inUse(uint64_t* guard_object)
{
    return ( ((uint8_t*)guard_object)[1] != 0 );
}

static void setInUse(uint64_t* guard_object)
{
    ((uint8_t*)guard_object)[1] = 1;
}

static void setNotInUse(uint64_t* guard_object)
{
    ((uint8_t*)guard_object)[1] = 0;
}


//
// Returns 1 if the caller needs to run the initializer and then either
// call __cxa_guard_release() or __cxa_guard_abort().  If zero is returned,
// then the initializer has already been run.  This function blocks
// if another thread is currently running the initializer.  This function
// aborts if called again on the same guard object without an intervening
// call to __cxa_guard_release() or __cxa_guard_abort().
//
int __cxa_guard_acquire(uint64_t* guard_object)
{
    // Double check that the initializer has not already been run
    if ( initializerHasRun(guard_object) )
        return 0;

    // We now need to acquire a lock that allows only one thread
    // to run the initializer.  If a different thread calls
    // __cxa_guard_acquire() with the same guard object, we want 
    // that thread to block until this thread is done running the 
    // initializer and calls __cxa_guard_release().  But if the same
    // thread calls __cxa_guard_acquire() with the same guard object,
    // we want to abort.  
    // To implement this we have one global pthread recursive mutex 
    // shared by all guard objects, but only one at a time.  

#if 0
    int result = ::pthread_mutex_lock(guard_mutex());
    if ( result != 0 ) {
        abort_message("__cxa_guard_acquire(): pthread_mutex_lock "
                      "failed with %d\n", result);
    }
#endif

    // At this point all other threads will block in __cxa_guard_acquire()
    
    // Check if another thread has completed initializer run
    if ( initializerHasRun(guard_object) ) {
#if 0
        int result = ::pthread_mutex_unlock(guard_mutex());
        if ( result != 0 ) {
            abort_message("__cxa_guard_acquire(): pthread_mutex_unlock "
                          "failed with %d\n", result);
        }
#endif        
        return 0;
    }
    
    // The pthread mutex is recursive to allow other lazy initialized
    // function locals to be evaluated during evaluation of this one.
    // But if the same thread can call __cxa_guard_acquire() on the 
    // *same* guard object again, we call abort();
    if ( inUse(guard_object) ) {
        abort_message("__cxa_guard_acquire(): initializer for function "
                      "local static variable called enclosing function\n");
    }
    
    // mark this guard object as being in use
    setInUse(guard_object);

    // return non-zero to tell caller to run initializer
    return 1;
}



//
// Sets the first byte of the guard_object to a non-zero value.
// Releases any locks acquired by __cxa_guard_acquire().
//
void __cxa_guard_release(uint64_t* guard_object)
{
    // first mark initalizer as having been run, so 
    // other threads won't try to re-run it.
    setInitializerHasRun(guard_object);

#if 0
    // release global mutex    
    int result = ::pthread_mutex_unlock(guard_mutex());
    if ( result != 0 ) {
        abort_message("__cxa_guard_acquire(): pthread_mutex_unlock "
                      "failed with %d\n", result);
    }
#endif    
}



//
// Releases any locks acquired by __cxa_guard_acquire().
//
void __cxa_guard_abort(uint64_t* guard_object)
{
#if 0
    int result = ::pthread_mutex_unlock(guard_mutex());
    if ( result != 0 ) {
        abort_message("__cxa_guard_abort(): pthread_mutex_unlock "
                      "failed with %d\n", result);
    }
#endif

	// now reset state, so possible to try to initialize again
	setNotInUse(guard_object);
}
