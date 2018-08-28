/*
 *
 *    Copyright (c) 2016-2018 Nest Labs, Inc.
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
 *      This file...
 *
 */

#include <nlplatform.h>
#include <nlplatform/nlswtimer.h>
#include <stdio.h>
#include <nlerassert.h>

#define DEBUG_TRACE 0

typedef struct nl_swtimer_entry_s
{
    nl_swtimer_func_t *func;
    void *arg;
    uint32_t delay;  /* unit depends on implementation */
    struct nl_swtimer_entry_s *next;
} nl_swtimer_entry_t;
static nl_swtimer_entry_t *s_timer_list;
_Static_assert(sizeof(nl_swtimer_entry_t) == sizeof(nl_swtimer_t), "sizeof(nl_swtimer_t) != sizeof(nl_swtimer_entry_t)");

#ifdef BUILD_FEATURE_SW_TIMER_USES_RTOS_TICK
#include <FreeRTOS.h>
#include <task.h>

// flag to prevent sleep if a unit test wants to test accuracy
// of times and sleep would mess that up
volatile bool g_swtimer_prevent_sleep = false;

/* Our own tick count.  FreeRTOS doesn't increment it's tick
 * counter when the scheduler is suspended, which happens all
 * the time in FreeRTOS's idle task.  The tick interrupt still
 * calls the tick hook function, but consecutive hook function
 * invocations might see the same value returned from
 * xTaskGetTickCount().  To keep time more accurately, we
 * have our own tick count, which always increments whenever
 * our hook function is called, and which we resync to the
 * FreeRTOS tick count after sleep because that's the only time
 * when it might jump by more than one tick.
 */
static TickType_t s_swtimer_tick_count;


#define NS_PER_TICK (1000000000 /*nanoseconds/second*/ / configTICK_RATE_HZ)

/* System time (time since boot) in nanoseconds.  This time is
 * incremented when system tick is incremented.  Unlike a tick
 * counter, this counter essentially does not wrap because it
 * would take hundreds of years.
 */
static uint64_t s_system_time_ns = 0;

/* List of timers whose delays are processed once the current
 * tick count has overflowed and wrapped back to 0.
 */
static nl_swtimer_entry_t *s_timer_overflow_list;

#if DEBUG_TRACE == 1
static void dump_swtimer_list(void)
{
    nl_swtimer_entry_t *timer_p = s_timer_list;
    while (timer_p)
    {
        printf("\ttimer %p, delay = %u, func = %p\n", timer_p, timer_p->delay, timer_p->func);
        timer_p = timer_p->next;
    }
    timer_p = s_timer_overflow_list;
    printf("overflow list:\n");
    while (timer_p)
    {
        printf("\ttimer %p, delay = %u, func = %p\n", timer_p, timer_p->delay, timer_p->func);
        timer_p = timer_p->next;
    }
}
#endif

static bool timer_in_list_locked(const nl_swtimer_entry_t *timer_list, const nl_swtimer_entry_t *timer_p)
{
    bool result = false;
    while (timer_list)
    {
        if (timer_list == timer_p)
        {
            result = true;
            break;
        }
        timer_list = timer_list->next;
    }
    return result;
}

static bool timer_is_active_locked(const nl_swtimer_entry_t *timer_p)
{
    return (timer_in_list_locked(s_timer_list, timer_p) ||
            timer_in_list_locked(s_timer_overflow_list, timer_p));
}

bool nl_swtimer_is_active(const nl_swtimer_t *timer_arg)
{
    bool result;
    const nl_swtimer_entry_t *timer_p = (const nl_swtimer_entry_t*)timer_arg;

    nlplatform_interrupt_disable();
    result = timer_is_active_locked(timer_p);
    nlplatform_interrupt_enable();
    return result;
}

bool nl_swtimer_pre_sleep(TickType_t *before_sleep_tick_count, uint32_t *xExpectedIdleTime)
{
    bool retval = false;
    /* Since this is called from the FreeRTOS idle thread with interrupts
     * still enabled, have to enter a critical section to examine
     * the s_timer_list
     */
    nlplatform_interrupt_disable();
#ifdef BUILD_FEATURE_UNIT_TEST
    if (g_swtimer_prevent_sleep)
    {
        /* in case unit-test set xTickCount for test reasons,
         * sync our counter with xTickCount
         */
        s_swtimer_tick_count = xTaskGetTickCount();
        s_system_time_ns = (uint64_t)s_swtimer_tick_count * NS_PER_TICK;
        goto done;
    }
#endif
    if (s_timer_list)
    {
        if (s_timer_list->delay > s_swtimer_tick_count)
        {
            TickType_t delay_in_ticks = s_timer_list->delay - s_swtimer_tick_count;
            if (delay_in_ticks < *xExpectedIdleTime)
            {
#if DEBUG_TRACE == 1
                printf("[%u] %s: xExpectedIdleTime = %u, s_timer_list->delay = %u, delay_in_ticks = %u\n",
                       s_swtimer_tick_count, __func__, *xExpectedIdleTime, s_timer_list->delay, delay_in_ticks);
#endif
                *xExpectedIdleTime = delay_in_ticks;
            }
        }
        else
        {
            /* Skip sleep if our timer was supposed to have run but hasn't.
             * This can sometimes happen if two consecutive calls to
             * sleep occurred without even one tick interrupt firing.
             * If the idle loop is very efficient relative to the tick
             * period, this could happen.
             */
            goto done;
        }
    }

    retval = true;

done:
    *before_sleep_tick_count = xTaskGetTickCount();

    /* our time can be a little different from FreeRTOS's tick count because
     * the latter doesn't advance when the scheduler is suspended but those
     * ticks are accounted for in uxPendedTicks counter and added to xTickCount
     * when the scheduler is resumed.  To check for drift that we're not
     * accounting for, make sure we're not too far off.
     */
    assert(s_swtimer_tick_count >= *before_sleep_tick_count);
    /* accuracy check, value is somewhat arbitrary.  mostly concerned
     * about a bug causing long term drift between our count and FreeRTOS's.
     */
    assert(s_swtimer_tick_count - *before_sleep_tick_count <= 3);

    nlplatform_interrupt_enable();
    return retval;
}

void nl_swtimer_post_sleep(TickType_t before_sleep_tick_count)
{
    TickType_t after_sleep_tick_count;

    nlplatform_interrupt_disable();

    after_sleep_tick_count = xTaskGetTickCount();

    /* add time slept.  we compute time sleep by computing the difference
     * of the FreeRTOS tick count before and after sleep.  We could have tried
     * to tie into the call to vTaskStepTick() instead but this keeps our
     * implementation more self-contained.  The sleep should never span
     * the wrap point of the tick counter.
     */
    assert(after_sleep_tick_count >= before_sleep_tick_count);
    TickType_t sleep_ticks = (after_sleep_tick_count - before_sleep_tick_count);
    s_swtimer_tick_count += sleep_ticks;
    s_system_time_ns += (uint64_t)sleep_ticks * NS_PER_TICK;
    
    nlplatform_interrupt_enable();
}

/*
 * Initialize the fields of our private timer structure
 */
void nl_swtimer_init(nl_swtimer_t *timer_arg, nl_swtimer_func_t *func, void *arg)
{
    nl_swtimer_entry_t *timer = (nl_swtimer_entry_t*)timer_arg;
    timer->func = func;
    timer->arg = arg;
    timer->next = NULL;
}

static void nl_swtimer_insert_locked(nl_swtimer_entry_t *timer, uint32_t delay_in_ticks)
{
    const TickType_t current_tick_count = s_swtimer_tick_count;
    nl_swtimer_entry_t **timer_pp;
    nl_swtimer_entry_t *timer_p;

    assert(timer_is_active_locked(timer) == false);

    // If an instantaneous timeout is being requested, expire on the next tick
    // boundary
    if (delay_in_ticks == 0)
    {
        delay_in_ticks = 1;
    }

    timer->delay = current_tick_count + delay_in_ticks;
    /* since we always add 1 tick, the only time we're equal
     * is when we've overflowed and completely wrapped
     * around again.
     */
    if (timer->delay > current_tick_count)
    {
        /* timer will be processed on the current timer list */
        timer_pp = &s_timer_list;
    }
    else
    {
        /* timer will be processed when the tick count overflows */
        timer_pp = &s_timer_overflow_list;
    }
    timer_p = *timer_pp;

#if DEBUG_TRACE == 1
    printf("\n\n[%u] %s: Starting timer %p, %u ticks\n",
           current_tick_count, __func__, timer, delay_in_ticks);
    printf("timer list before insert:\n");
    dump_swtimer_list();
#endif
    while (timer_p)
    {
        if (timer->delay < timer_p->delay)
        {
            break;
        }
        timer_pp = &timer_p->next;
        timer_p = timer_p->next;
    }
    // timer_p is a pointer to the timer we're
    // inserting the new timer before.
    // timer_pp is a pointer to the pointer to
    // timer_p, which we adjust to point to the new
    // timer.
    *timer_pp = timer;
    timer->next = timer_p;

#if DEBUG_TRACE == 1
    printf("timer list after insert:\n");
    dump_swtimer_list();
#endif
}

/* Add timer to our sorted timer list atomically.  The delay field
 * in the timer struct stores the target tick count for when the
 * timer should run and not a delta of ticks from the current tick
 * count.  This is needed for handling large jumps in ticks
 * that might happen coming out of sleep and for the situation
 * where the timer restarts itself from the timer function.
 * The list is sorted by delays to make the tick handler check for
 * expired timers faster, so it doesn't have to traverse the entire
 * list of timers at interrupt time.
 *
 * Have to use a critical section since our timer function runs at
 * interrupt time.
 *
 * Since our list is unbounded, this takes non-deterministic time, but
 * we don't expect a huge number of timers in our small devices.
 */
void nl_swtimer_start(nl_swtimer_t *timer_arg, uint32_t delay_ms)
{
    nl_swtimer_entry_t *timer = (nl_swtimer_entry_t*)timer_arg;
    TickType_t delay_in_ticks = nl_time_ms_to_delay_time_native(delay_ms);

    assert(timer->func);

    nlplatform_interrupt_disable();

    nl_swtimer_insert_locked(timer, delay_in_ticks);

    nlplatform_interrupt_enable();
}

static bool remove_from_list_locked(nl_swtimer_entry_t **timer_pp, nl_swtimer_entry_t *timer)
{
    bool result = false;
    while (*timer_pp)
    {
        if (*timer_pp == timer)
        {
            // Found the timer in the list.
            result = true;
#if DEBUG_TRACE == 1
            printf("Removing timer %p with delay %u, func %p\n",
                   timer, timer->delay, timer->func);
            printf("timer list before removal:\n");
            dump_swtimer_list();
#endif
            // Fixup the pointer to us to point
            // to our next
            *timer_pp = timer->next;
#if DEBUG_TRACE == 1
            printf("list after timer removal:\n");
            dump_swtimer_list();
#endif
            break;
        }
        timer_pp = &((*timer_pp)->next);
    }
    return result;
}

bool nl_swtimer_cancel(nl_swtimer_t *timer_arg)
{
    nl_swtimer_entry_t *timer = (nl_swtimer_entry_t*)timer_arg;
    bool result;

    nlplatform_interrupt_disable();

    result = remove_from_list_locked(&s_timer_list, timer);
    if (!result)
    {
        result = remove_from_list_locked(&s_timer_overflow_list, timer);
    }

    nlplatform_interrupt_enable();
    return result;
}

void nl_swtimer_rtos_tick_handler(void)
{
    /* increment our tick count and check for wrap. */
    const TickType_t last_tick_count = s_swtimer_tick_count++;
    /* compiler optimization, since this won't change in this function */
    const TickType_t current_tick_count = s_swtimer_tick_count;

    s_system_time_ns += NS_PER_TICK;

    if (current_tick_count < last_tick_count)
    {
        /* Tick count has wrapped.  All timers in the s_timer_list
         * can be run, and some timers in the s_timer_overflow_list
         * might be runnable too.  To handle restarts and cancels
         * calls from timer functions, we merge both lists at this time,
         * setting the delay of all timers in s_timer_list to 0,
         * then adding all the timers from s_timer_overflow_list to
         * the end of s_timer_list.
         */
        nl_swtimer_entry_t **timer_pp = &s_timer_list;
        nl_swtimer_entry_t *timer_p = *timer_pp;
#if DEBUG_TRACE == 1
        printf("Tick count overflowed, zeroing delay of s_timer_list timers and moving s_timer_overflow_list timers to end\n");
#endif
        while (timer_p)
        {
            timer_p->delay = 0;
            timer_pp = &timer_p->next;
            timer_p = timer_p->next;
        }
        *timer_pp = s_timer_overflow_list;
        s_timer_overflow_list = NULL;
#if DEBUG_TRACE == 1
        printf("timer list after merge:\n");
        dump_swtimer_list();
#endif
    }
    while (s_timer_list)
    {
        if (current_tick_count >= s_timer_list->delay)
        {
            nl_swtimer_entry_t *timer_p = s_timer_list;
            uint32_t new_delay;
            /* Remove from timer list so it can restart itself if desired */
            s_timer_list = timer_p->next;
#if DEBUG_TRACE == 1
            printf("[%u] Running timer %p, func = %p\n", current_tick_count, timer_p, timer_p->func);
#endif
            new_delay = (timer_p->func)((nl_swtimer_t*)timer_p, timer_p->arg);
            if (new_delay)
            {
                /* Since this delay is aligned on the tick interrupt,
                 * we don't want the extra tick like we do for
                 * the normal start case
                 */
                TickType_t delay_in_ticks = nl_time_ms_to_delay_time_native(new_delay) - 1;
                nl_swtimer_insert_locked(timer_p, delay_in_ticks);
            }
        }
        else
        {
            break;
        }
    }
}

uint64_t nl_swtimer_get_time_ns(void)
{
    uint64_t result;
    nlplatform_interrupt_disable();
    result = s_system_time_ns;
    nlplatform_interrupt_enable();
    return result;
}

#if defined(DEBUG) && defined(BUILD_FEATURE_UNIT_TEST)
#include <nlertime.h>

#define TIMER0_TICKS 25
#define TIMER1_TICKS 50
#define TIMER2_TICKS 100
#define TIMER3_TICKS 200
#define TIMER4_TICKS 500

#define NUM_TEST_TIMERS 5

static const uint32_t timer_delays[NUM_TEST_TIMERS] =
{
    TIMER0_TICKS,
    TIMER1_TICKS,
    TIMER2_TICKS,
    TIMER3_TICKS,
    TIMER4_TICKS
};
static nl_swtimer_t timers[NUM_TEST_TIMERS];

// checks if the list is ordered correctly for the 5 timers
// we create in different order.  If the removed_timer is
// not NULL, then assume this timer was removed and don't
// look for it.
static void verify_list1(nl_swtimer_t *removed_timer)
{
    nl_swtimer_entry_t *timer_p = s_timer_list;
    unsigned i;

    for (i = 0; i < NUM_TEST_TIMERS; i++) {
        if (removed_timer != &timers[i]) {
            assert(timer_p == (nl_swtimer_entry_t*)&timers[i]);
            assert(timer_p->delay == timer_delays[i]);
            timer_p = timer_p->next;
        }
    }
    assert(timer_p == NULL);
}

// checks if the list is ordered correctly for the 5 timers
// we create with the same delay
static void verify_list2(void)
{
    nl_swtimer_entry_t *timer_p;

    // timer1 check
    timer_p = s_timer_list;
    assert(timer_p == (nl_swtimer_entry_t*)&timers[0]);
    assert(timer_p->delay == TIMER0_TICKS);

    // timer2 check
    timer_p = timer_p->next;
    assert(timer_p == (nl_swtimer_entry_t*)&timers[1]);
    assert(timer_p->delay == TIMER0_TICKS);

    // timer3 check
    timer_p = timer_p->next;
    assert(timer_p == (nl_swtimer_entry_t*)&timers[2]);
    assert(timer_p->delay == TIMER0_TICKS);

    // timer4 check
    timer_p = timer_p->next;
    assert(timer_p == (nl_swtimer_entry_t*)&timers[3]);
    assert(timer_p->delay == TIMER0_TICKS);

    // timer5 check
    timer_p = timer_p->next;
    assert(timer_p == (nl_swtimer_entry_t*)&timers[4]);
    assert(timer_p->delay == TIMER0_TICKS);

    assert(timer_p->next == NULL);
}

// Call this in main, before threading has started, to
// do a sanity test of the list implementation
void nl_swtimer_sanity_test(void);
void nl_swtimer_sanity_test(void)
{
    nluart_force_sync(CONSOLE_UART_ID);
    printf("\n\n%s: start\n", __func__);

    nlplatform_interrupt_disable();

    // tests order of timer list after various combinations
    // of start/cancel.  the function isn't actually
    // ever used since we do this test with interrupts
    // disabled, so we just pass a dummy ptr in order
    // not to trip the NULL assert.  the actual runtime
    // testing is done in the unit test code.
    nl_swtimer_init(&timers[0], (nl_swtimer_func_t*)nl_swtimer_sanity_test, NULL);
    nl_swtimer_init(&timers[1], (nl_swtimer_func_t*)nl_swtimer_sanity_test, NULL);
    nl_swtimer_init(&timers[2], (nl_swtimer_func_t*)nl_swtimer_sanity_test, NULL);
    nl_swtimer_init(&timers[3], (nl_swtimer_func_t*)nl_swtimer_sanity_test, NULL);
    nl_swtimer_init(&timers[4], (nl_swtimer_func_t*)nl_swtimer_sanity_test, NULL);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    verify_list1(NULL);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    verify_list1(NULL);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    verify_list1(NULL);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    verify_list1(NULL);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    verify_list1(NULL);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    verify_list1(NULL);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    verify_list1(NULL);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    verify_list1(NULL);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    verify_list1(NULL);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    verify_list1(NULL);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    verify_list2();
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    // test list validity with removal from different places in list
    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    nl_swtimer_cancel(&timers[0]);
    verify_list1(&timers[0]);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    nl_swtimer_cancel(&timers[1]);
    verify_list1(&timers[1]);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    nl_swtimer_cancel(&timers[2]);
    verify_list1(&timers[2]);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    nl_swtimer_cancel(&timers[3]);
    verify_list1(&timers[3]);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    nl_swtimer_start(&timers[0], nl_time_native_to_time_ms(TIMER0_TICKS-1));
    nl_swtimer_start(&timers[1], nl_time_native_to_time_ms(TIMER1_TICKS-1));
    nl_swtimer_start(&timers[2], nl_time_native_to_time_ms(TIMER2_TICKS-1));
    nl_swtimer_start(&timers[3], nl_time_native_to_time_ms(TIMER3_TICKS-1));
    nl_swtimer_start(&timers[4], nl_time_native_to_time_ms(TIMER4_TICKS-1));
    nl_swtimer_cancel(&timers[4]);
    verify_list1(&timers[4]);
    nl_swtimer_cancel(&timers[0]);
    nl_swtimer_cancel(&timers[1]);
    nl_swtimer_cancel(&timers[2]);
    nl_swtimer_cancel(&timers[3]);
    nl_swtimer_cancel(&timers[4]);
    assert(s_timer_list == NULL);

    printf("%s: end: all tests passed\n\n", __func__);
    nlplatform_interrupt_enable();
}
#endif // DEBUG && BUILD_FEATURE_UNIT_TEST
#endif // BUILD_FEATURE_SW_TIMER_USES_RTOS_TICK
