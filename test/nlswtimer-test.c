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
 *      This file implements a unit test suite,
 *      testing nl_swtimer functions
 *
 */

// This is for unit tests only.  We can't simulate this unit test
#ifdef BUILD_FEATURE_UNIT_TEST

#ifdef BUILD_FEATURE_SW_TIMER_USES_RTOS_TICK

#include <nlmacros.h>
#include <nlplatform.h>
#include <nlplatform/nlswtimer.h>
#include <nlplatform/nlwatchdog.h>
#include "nltest.h"
#include <nlplatform/nlswtimer-test.h>
#include <nlertask.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include <stdlib.h>

extern bool g_swtimer_prevent_sleep;

static bool s_test_with_tick_count_near_wrap = false;

typedef struct {
    nlTestSuite *test_suite;
    uint32_t   num_repeats; // number of times timer function should restart itself
    uint32_t   count; // count of times the timer function was invoked
    uint32_t   repeat_delay; // delay for the timer function restart
    TickType_t expectedRunTimeMin;
    TickType_t expectedRunTimeMax;
} timer_test_info_t;

typedef struct timer_test_info2_s {
    nlTestSuite *test_suite;
    uint32_t   count; // count of times the timer function was invoked
    TickType_t expectedRunTimeMin;
    TickType_t expectedRunTimeMax;
    uint32_t   num_timers; // number of timers in the list
    nl_swtimer_t **timers;
    struct timer_test_info2_s **timer_infos;
    uint32_t   *timer_delays;
    TaskHandle_t notifyTaskHandle;
} timer_test_info2_t;

static TaskHandle_t sTaskHandle;
static bool s_test_with_tick_count_near_wrap;

#define TIMER_TEST_DELAY_1_MS     1
#define TIMER_TEST_DELAY_10_MS    10
#define TIMER_TEST_DELAY_50_MS    50
#define TIMER_TEST_DELAY_100_MS   100
#define TIMER_TEST_DELAY_200_MS   200
#define TIMER_TEST_DELAY_500_MS   500
#define TIMER_TEST_DELAY_1000_MS  1000
#define TIMER_TEST_DELAY_2000_MS  2000
#define TIMER_TEST_DELAY_4000_MS  4000
#define TIMER_TEST_DELAY_5000_MS  5000
#define TIMER_TEST_DELAY_10000_MS 10000


// timer functions should be invoked at the expected time or at most one or two
// ticks later (never early).  late is due to delta added by the implementation
// (1 tick usually) to guarantee we're not early in case a tick interrupt was
// just about to fire, and 1 more in case there was delay associated with the
// time we recorded the expected time and another tick arriving, or other added latency.
#define TIMING_ERROR_TOLERANCE_TICKS 2

// when sleep is enabled, there's a lot more latency/inaccuracy involved with coming
// out of sleep.  we give a higher tolerance, but this might vary by implementation.
#define TIMING_ERROR_TOLERANCE_WITH_SLEEP_TICKS 10

static uint32_t should_not_run_func(nl_swtimer_t *timer, void *arg)
{
    timer_test_info_t *test_info = (timer_test_info_t*)arg;
    NL_TEST_ASSERT(test_info->test_suite, 1 == 0);
    return 0;
}

static uint32_t one_shot_timer_test(nl_swtimer_t *timer, void *arg)
{
    BaseType_t yield = pdFALSE;
    timer_test_info_t *test_info = (timer_test_info_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();

    test_info->count++;
    // ticks later (never early).  late is due to delta added
    // by the implementation (1 tick usually) to guarantee
    // we're not early, and 1 more in case there was delay
    // associated with the time we recorded the expected time
    // and another tick arriving, or other added latency.
    if ((current_tick_count < test_info->expectedRunTimeMin) ||
        (current_tick_count > test_info->expectedRunTimeMax))
    {
        printf("%s: failure: current_tick_count = %u, expectedRunTimeMin = %u, expectedRunTimeMax = %u\n",
               __func__, current_tick_count, test_info->expectedRunTimeMin,
               test_info->expectedRunTimeMax);
    }
    NL_TEST_ASSERT(test_info->test_suite,
                   (current_tick_count >= test_info->expectedRunTimeMin) &&
                   (current_tick_count <= test_info->expectedRunTimeMax));
    vTaskNotifyGiveFromISR(sTaskHandle, &yield);
    portEND_SWITCHING_ISR(yield);
    return 0;
}

static uint32_t repeat_timer_test(nl_swtimer_t *timer, void *arg)
{
    timer_test_info_t *test_info = (timer_test_info_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();
    uint32_t repeat_delay_ms;

    test_info->count++;
    NL_TEST_ASSERT(test_info->test_suite,
                   (current_tick_count >= test_info->expectedRunTimeMin) &&
                   (current_tick_count <= test_info->expectedRunTimeMax));
    if (test_info->count <= test_info->num_repeats)
    {
        test_info->expectedRunTimeMin = current_tick_count + nl_time_ms_to_delay_time_native(test_info->repeat_delay) - 1;
        test_info->expectedRunTimeMax = test_info->expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
        repeat_delay_ms = test_info->repeat_delay;
    }
    else
    {
        BaseType_t yield = pdFALSE;
        vTaskNotifyGiveFromISR(sTaskHandle, &yield);
        portEND_SWITCHING_ISR(yield);
        repeat_delay_ms = 0;
    }
    return repeat_delay_ms;
}

static uint32_t cancel_timer_test(nl_swtimer_t *timer, void *arg)
{
    timer_test_info_t *test_info = (timer_test_info_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();
    nl_swtimer_t *timer_to_cancel = (nl_swtimer_t*)test_info->num_repeats;
    bool cancel_result;

    test_info->count++;
    NL_TEST_ASSERT(test_info->test_suite,
                   (current_tick_count >= test_info->expectedRunTimeMin) &&
                   (current_tick_count <= test_info->expectedRunTimeMax));

    cancel_result = nl_swtimer_cancel(timer_to_cancel);
    NL_TEST_ASSERT(test_info->test_suite, cancel_result == true);
    return 0;
}

// adjust ticks so that the count is close to wrap
static void AdjustTickCount(uint32_t ticks_before_wrap)
{
    TickType_t ticks_to_jump;
    bool old_prevent_value = g_swtimer_prevent_sleep;
    TickType_t before_sleep_tick_count;
    uint32_t ticks_to_sleep = 0;

    // delay 1 tick to make sure xNextTaskUnblockTime is cleared in FreeRTOS
    vTaskDelay(1);
    ticks_to_jump = 0 - ticks_before_wrap - xTaskGetTickCount() - 1;
    vTaskStepTick(ticks_to_jump);
    // call function to force nlswtimer to sync it's internal tick counter
    // to match FreeRTOS's
    g_swtimer_prevent_sleep = true;
    nl_swtimer_pre_sleep(&before_sleep_tick_count, &ticks_to_sleep);
    g_swtimer_prevent_sleep = old_prevent_value;
    // wait for 1 tick for FreeRTOS to settle after the big jump
    vTaskDelay(1);
}

static void Test_one_shot(nlTestSuite *inSuite, void *inContext)
{
    BaseType_t wait_result;
    timer_test_info_t test_info1;
    nl_swtimer_t timer1;
    TickType_t delay_ticks;

    if (s_test_with_tick_count_near_wrap)
    {
        AdjustTickCount(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_50_MS));
    }

    // test a simple one shot timer
    printf("%s: start\n", __func__);
    memset(&test_info1, 0, sizeof(test_info1));
    ulTaskNotifyTake(pdTRUE, 0); // clear any old notifications
    nl_swtimer_init(&timer1, one_shot_timer_test, &test_info1);
    delay_ticks = nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_100_MS);
    test_info1.test_suite = inSuite;
    test_info1.expectedRunTimeMin = xTaskGetTickCount() + delay_ticks;
    test_info1.expectedRunTimeMax = test_info1.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer1, TIMER_TEST_DELAY_100_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1));
    wait_result = ulTaskNotifyTake(pdTRUE, delay_ticks + TIMING_ERROR_TOLERANCE_TICKS);
    NL_TEST_ASSERT(inSuite, wait_result != 0);
    NL_TEST_ASSERT(inSuite, test_info1.count == test_info1.num_repeats + 1);

    // cleanup just in case of failure before we run next test, else
    // our stack timer structure will corrupt the nl_swtimer implementation
    (void)nl_swtimer_cancel(&timer1);
}

static void Test_single_repeat(nlTestSuite *inSuite, void *inContext)
{
    BaseType_t wait_result;
    timer_test_info_t test_info1;
    nl_swtimer_t timer1;
    TickType_t delay_ticks;

    if (s_test_with_tick_count_near_wrap)
    {
        AdjustTickCount(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_50_MS));
    }

    // test a timer that restarts itself once from within it's function
    printf("%s: start\n", __func__);
    memset(&test_info1, 0, sizeof(test_info1));
    ulTaskNotifyTake(pdTRUE, 0); // clear any old notifications
    delay_ticks = nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_100_MS);
    nl_swtimer_init(&timer1, repeat_timer_test, &test_info1);
    test_info1.test_suite = inSuite;
    test_info1.num_repeats = 1;
    test_info1.repeat_delay = TIMER_TEST_DELAY_100_MS;
    test_info1.expectedRunTimeMin = xTaskGetTickCount() + delay_ticks;
    test_info1.expectedRunTimeMax = test_info1.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer1, TIMER_TEST_DELAY_100_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1));
    wait_result = ulTaskNotifyTake(pdTRUE, (delay_ticks + TIMING_ERROR_TOLERANCE_TICKS)*2);
    NL_TEST_ASSERT(inSuite, wait_result != 0);
    NL_TEST_ASSERT(inSuite, test_info1.count == test_info1.num_repeats + 1);

    // cleanup just in case of failure before we run next test, else
    // our stack timer structure will corrupt the nl_swtimer implementation
    (void)nl_swtimer_cancel(&timer1);
}

static void Test_one_shot_cancel(nlTestSuite *inSuite, void *inContext)
{
    BaseType_t wait_result;
    timer_test_info_t test_info1;
    nl_swtimer_t timer1;
    TickType_t delay_ticks;
    bool cancel_result;

    if (s_test_with_tick_count_near_wrap)
    {
        AdjustTickCount(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_50_MS));
    }

    // test a timer that has a long delay and we cancel
    // it before it runs
    printf("%s: start\n", __func__);
    memset(&test_info1, 0, sizeof(test_info1));
    ulTaskNotifyTake(pdTRUE, 0); // clear any old notifications
    delay_ticks = nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_1000_MS);
    nl_swtimer_init(&timer1, should_not_run_func, &test_info1);
    nl_swtimer_start(&timer1, TIMER_TEST_DELAY_1000_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1));
    vTaskDelay(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_100_MS));
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1));
    cancel_result = nl_swtimer_cancel(&timer1);
    NL_TEST_ASSERT(inSuite, cancel_result == true);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1) == false);
    wait_result = ulTaskNotifyTake(pdTRUE, delay_ticks);
    NL_TEST_ASSERT(inSuite, wait_result == 0); // check for timeout
    NL_TEST_ASSERT(inSuite, test_info1.count == 0);

    // cleanup just in case of failure before we run next test, else
    // our stack timer structure will corrupt the nl_swtimer implementation
    (void)nl_swtimer_cancel(&timer1);
}

static void Test_one_shot_cancel_restart(nlTestSuite *inSuite, void *inContext)
{
    BaseType_t wait_result;
    timer_test_info_t test_info1;
    nl_swtimer_t timer1;
    TickType_t delay_ticks;
    bool cancel_result;

    if (s_test_with_tick_count_near_wrap)
    {
        AdjustTickCount(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_50_MS));
    }

    // test a timer that has a long delay and we
    // cancel it and change the time
    printf("%s: start\n", __func__);
    memset(&test_info1, 0, sizeof(test_info1));
    test_info1.test_suite = inSuite;
    ulTaskNotifyTake(pdTRUE, 0); // clear any old notifications
    delay_ticks = nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_100_MS);
    nl_swtimer_init(&timer1, should_not_run_func, &test_info1);
    nl_swtimer_start(&timer1, TIMER_TEST_DELAY_1000_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1));
    vTaskDelay(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_100_MS));
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1));
    cancel_result = nl_swtimer_cancel(&timer1);
    NL_TEST_ASSERT(inSuite, cancel_result == true);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1) == false);
    NL_TEST_ASSERT(inSuite, test_info1.count == 0);
    nl_swtimer_init(&timer1, one_shot_timer_test, &test_info1);
    test_info1.expectedRunTimeMin = xTaskGetTickCount() + delay_ticks;
    test_info1.expectedRunTimeMax = test_info1.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer1, TIMER_TEST_DELAY_100_MS);
    wait_result = ulTaskNotifyTake(pdTRUE, delay_ticks + TIMING_ERROR_TOLERANCE_TICKS);
    NL_TEST_ASSERT(inSuite, wait_result != 0);
    NL_TEST_ASSERT(inSuite, test_info1.count == test_info1.num_repeats + 1);

    // cleanup just in case of failure before we run next test, else
    // our stack timer structure will corrupt the nl_swtimer implementation
    (void)nl_swtimer_cancel(&timer1);
}

static void Test_repeat_100(nlTestSuite *inSuite, void *inContext)
{
    BaseType_t wait_result;
    timer_test_info_t test_info1;
    nl_swtimer_t timer1;
    TickType_t delay_ticks;

    if (s_test_with_tick_count_near_wrap)
    {
        AdjustTickCount(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_50_MS));
    }

    // test a timer that repeats itself 100 times (pseudo
    // indefinitely) at 100ms period
    printf("%s: start. test takes about 10 seconds...\n", __func__);
    memset(&test_info1, 0, sizeof(test_info1));
    ulTaskNotifyTake(pdTRUE, 0); // clear any old notifications
    delay_ticks = nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_100_MS);
    nl_swtimer_init(&timer1, repeat_timer_test, &test_info1);
    test_info1.test_suite = inSuite;
    test_info1.num_repeats = 100;
    test_info1.repeat_delay = TIMER_TEST_DELAY_100_MS;
    test_info1.expectedRunTimeMin = xTaskGetTickCount() + delay_ticks;
    test_info1.expectedRunTimeMax = test_info1.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer1, TIMER_TEST_DELAY_100_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1));
    wait_result = ulTaskNotifyTake(pdTRUE,
                                   (delay_ticks + TIMING_ERROR_TOLERANCE_TICKS) * test_info1.num_repeats);
    NL_TEST_ASSERT(inSuite, wait_result != 0);
    NL_TEST_ASSERT(inSuite, test_info1.count == test_info1.num_repeats + 1);

    // cleanup just in case of failure before we run next test, else
    // our stack timer structure will corrupt the nl_swtimer implementation
    (void)nl_swtimer_cancel(&timer1);
}

static void Test_timers_with_sleep_enabled(nlTestSuite *inSuite, void *inContext)
{
    BaseType_t wait_result;
    timer_test_info_t test_info1;
    nl_swtimer_t timer1;
    TickType_t delay_ticks;

    if (s_test_with_tick_count_near_wrap)
    {
        AdjustTickCount(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_50_MS));
    }

    // test a timer that has a long delay to make sure
    // it wakes up even when sleep occurs
    printf("%s: start\n", __func__);
    memset(&test_info1, 0, sizeof(test_info1));
    ulTaskNotifyTake(pdTRUE, 0); // clear any old notifications
    delay_ticks = nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_1000_MS);
    nl_swtimer_init(&timer1, one_shot_timer_test, &test_info1);
    test_info1.test_suite = inSuite;
    // xTaskGetCount() can be slow when a tick occurs with the scheduler suspended
    // in the FreeRTOS Idle function, so allow a min which is one tick early.
    test_info1.expectedRunTimeMin = xTaskGetTickCount() + delay_ticks - 1;
    test_info1.expectedRunTimeMax = test_info1.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_WITH_SLEEP_TICKS;
    nl_swtimer_start(&timer1, TIMER_TEST_DELAY_1000_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1));
    wait_result = ulTaskNotifyTake(pdTRUE, delay_ticks + TIMING_ERROR_TOLERANCE_WITH_SLEEP_TICKS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1) == false);
    NL_TEST_ASSERT(inSuite, wait_result != 0);
    NL_TEST_ASSERT(inSuite, test_info1.count == test_info1.num_repeats + 1);

    // test a timer that has a short delay, but long enough that
    // we should sleep.  make sure timer wakes up on time.
    memset(&test_info1, 0, sizeof(test_info1));
    delay_ticks = nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_10_MS);
    nl_swtimer_init(&timer1, one_shot_timer_test, &test_info1);
    test_info1.test_suite = inSuite;
    // xTaskGetCount() can be slow when a tick occurs with the scheduler suspended
    // in the FreeRTOS Idle function, so allow a min which is one tick early.
    test_info1.expectedRunTimeMin = xTaskGetTickCount() + delay_ticks - 1;
    test_info1.expectedRunTimeMax = test_info1.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_WITH_SLEEP_TICKS;
    nl_swtimer_start(&timer1, TIMER_TEST_DELAY_10_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1));
    wait_result = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1) == false);
    NL_TEST_ASSERT(inSuite, wait_result != 0);
    NL_TEST_ASSERT(inSuite, test_info1.count == test_info1.num_repeats + 1);

    // cleanup just in case of failure before we run next test, else
    // our stack timer structure will corrupt the nl_swtimer implementation
    (void)nl_swtimer_cancel(&timer1);
}

static void Test_five_timers(nlTestSuite *inSuite, void *inContext)
{
    timer_test_info_t test_info1;
    timer_test_info_t test_info2;
    timer_test_info_t test_info3;
    timer_test_info_t test_info4;
    timer_test_info_t test_info5;
    nl_swtimer_t timer1;
    nl_swtimer_t timer2;
    nl_swtimer_t timer3;
    nl_swtimer_t timer4;
    nl_swtimer_t timer5;

    if (s_test_with_tick_count_near_wrap)
    {
        AdjustTickCount(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_50_MS));
    }

    // test five repeating timers running at different delays.
    // total runtime of 10 seconds.
    // timer1 has a period of 100ms, repeats 99 times
    // timer2 has a period of 500ms, repeats 19 times
    // timer3 has a period of 1000ms, repeats 9 times
    // timer4 has a period of 2000ms, repeats 4 times
    // timer5 has a period of 5000ms, repeats once
    printf("%s: start. test takes about 10 seconds...\n", __func__);
    memset(&test_info1, 0, sizeof(test_info1));
    memset(&test_info2, 0, sizeof(test_info2));
    memset(&test_info3, 0, sizeof(test_info3));
    memset(&test_info4, 0, sizeof(test_info4));
    memset(&test_info5, 0, sizeof(test_info5));
    nl_swtimer_init(&timer1, repeat_timer_test, &test_info1);
    nl_swtimer_init(&timer2, repeat_timer_test, &test_info2);
    nl_swtimer_init(&timer3, repeat_timer_test, &test_info3);
    nl_swtimer_init(&timer4, repeat_timer_test, &test_info4);
    nl_swtimer_init(&timer5, repeat_timer_test, &test_info5);
    test_info1.test_suite = inSuite;
    test_info1.repeat_delay = TIMER_TEST_DELAY_100_MS;
    test_info1.num_repeats = TIMER_TEST_DELAY_10000_MS / TIMER_TEST_DELAY_100_MS - 1;
    test_info2.test_suite = inSuite;
    test_info2.repeat_delay = TIMER_TEST_DELAY_500_MS;
    test_info2.num_repeats = TIMER_TEST_DELAY_10000_MS / TIMER_TEST_DELAY_500_MS - 1;
    test_info3.test_suite = inSuite;
    test_info3.repeat_delay = TIMER_TEST_DELAY_1000_MS;
    test_info3.num_repeats = TIMER_TEST_DELAY_10000_MS / TIMER_TEST_DELAY_1000_MS - 1;
    test_info4.test_suite = inSuite;
    test_info4.repeat_delay = TIMER_TEST_DELAY_2000_MS;
    test_info4.num_repeats = TIMER_TEST_DELAY_10000_MS / TIMER_TEST_DELAY_2000_MS - 1;
    test_info5.test_suite = inSuite;
    test_info5.repeat_delay = TIMER_TEST_DELAY_5000_MS;
    test_info5.num_repeats = TIMER_TEST_DELAY_10000_MS / TIMER_TEST_DELAY_5000_MS - 1;
    test_info1.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_100_MS);
    test_info1.expectedRunTimeMax = test_info1.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer1, TIMER_TEST_DELAY_100_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1));
    test_info2.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_500_MS);
    test_info2.expectedRunTimeMax = test_info2.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer2, TIMER_TEST_DELAY_500_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer2));
    test_info3.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_1000_MS);
    test_info3.expectedRunTimeMax = test_info3.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer3, TIMER_TEST_DELAY_1000_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer3));
    test_info4.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_2000_MS);
    test_info4.expectedRunTimeMax = test_info4.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer4, TIMER_TEST_DELAY_2000_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer4));
    test_info5.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_5000_MS);
    test_info5.expectedRunTimeMax = test_info5.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer5, TIMER_TEST_DELAY_5000_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer5));
    // delay until we expect everything to have been run.  since
    // restarting a timer from within itself isn't quite the same
    // as true periodic, because of the extra tick added to the
    // each start, we have to wait a bit longer than 10 seconds
    vTaskDelay((nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_5000_MS)*2 + (test_info1.num_repeats * (TIMING_ERROR_TOLERANCE_TICKS + 1))));
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer2) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer3) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer4) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer5) == false);
    NL_TEST_ASSERT(inSuite, test_info1.count == test_info1.num_repeats + 1);
    NL_TEST_ASSERT(inSuite, test_info2.count == test_info2.num_repeats + 1);
    NL_TEST_ASSERT(inSuite, test_info3.count == test_info3.num_repeats + 1);
    NL_TEST_ASSERT(inSuite, test_info4.count == test_info4.num_repeats + 1);
    NL_TEST_ASSERT(inSuite, test_info5.count == test_info5.num_repeats + 1);

    // cleanup just in case of failure before we run next test, else
    // our stack timer structure will corrupt the nl_swtimer implementation
    (void)nl_swtimer_cancel(&timer1);
    (void)nl_swtimer_cancel(&timer2);
    (void)nl_swtimer_cancel(&timer3);
    (void)nl_swtimer_cancel(&timer4);
    (void)nl_swtimer_cancel(&timer5);
}

static void Test_immediate_expiration(nlTestSuite *inSuite, void *inContext)
{
    BaseType_t wait_result;
    timer_test_info_t test_info1;
    nl_swtimer_t timer1;
    TickType_t delay_ticks;

    if (s_test_with_tick_count_near_wrap)
    {
        AdjustTickCount(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_50_MS));
    }

    // test a simple one shot timer with immediate expiry
    printf("%s: start\n", __func__);
    memset(&test_info1, 0, sizeof(test_info1));
    ulTaskNotifyTake(pdTRUE, 0); // clear any old notifications
    nl_swtimer_init(&timer1, one_shot_timer_test, &test_info1);
    delay_ticks = 1;
    test_info1.test_suite = inSuite;
    test_info1.expectedRunTimeMin = xTaskGetTickCount() + delay_ticks;
    test_info1.expectedRunTimeMax = test_info1.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer1, 0);
    wait_result = ulTaskNotifyTake(pdTRUE, delay_ticks + TIMING_ERROR_TOLERANCE_TICKS);
    NL_TEST_ASSERT(inSuite, wait_result != 0);
    NL_TEST_ASSERT(inSuite, test_info1.count == test_info1.num_repeats + 1);

    // cleanup just in case of failure before we run next test, else
    // our stack timer structure will corrupt the nl_swtimer implementation
    (void)nl_swtimer_cancel(&timer1);
}

static void Test_five_timers_mixed(nlTestSuite *inSuite, void *inContext)
{
    timer_test_info_t test_info1;
    timer_test_info_t test_info2;
    timer_test_info_t test_info3;
    timer_test_info_t test_info4;
    timer_test_info_t test_info5;
    nl_swtimer_t timer1;
    nl_swtimer_t timer2;
    nl_swtimer_t timer3;
    nl_swtimer_t timer4;
    nl_swtimer_t timer5;

    if (s_test_with_tick_count_near_wrap)
    {
        AdjustTickCount(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_50_MS));
    }

    // test five repeating timers running at different delays.
    // total runtime of 10 seconds.  mixup the order of
    // the timer inserts.
    // timer1 has a period of 100ms, repeats 99 times
    // timer2 has a period of 500ms, repeats 19 times
    // timer3 has a period of 1000ms, repeats 9 times
    // timer4 has a period of 2000ms, repeats 4 times
    // timer5 has a period of 5000ms, repeats once
    printf("%s: start. test takes about 10 seconds...\n", __func__);
    memset(&test_info1, 0, sizeof(test_info1));
    memset(&test_info2, 0, sizeof(test_info2));
    memset(&test_info3, 0, sizeof(test_info3));
    memset(&test_info4, 0, sizeof(test_info4));
    memset(&test_info5, 0, sizeof(test_info5));
    nl_swtimer_init(&timer1, repeat_timer_test, &test_info1);
    nl_swtimer_init(&timer2, repeat_timer_test, &test_info2);
    nl_swtimer_init(&timer3, repeat_timer_test, &test_info3);
    nl_swtimer_init(&timer4, repeat_timer_test, &test_info4);
    nl_swtimer_init(&timer5, repeat_timer_test, &test_info5);
    test_info1.test_suite = inSuite;
    test_info1.repeat_delay = TIMER_TEST_DELAY_100_MS;
    test_info1.num_repeats = TIMER_TEST_DELAY_10000_MS / TIMER_TEST_DELAY_100_MS - 1;
    test_info2.test_suite = inSuite;
    test_info2.repeat_delay = TIMER_TEST_DELAY_500_MS;
    test_info2.num_repeats = TIMER_TEST_DELAY_10000_MS / TIMER_TEST_DELAY_500_MS - 1;
    test_info3.test_suite = inSuite;
    test_info3.repeat_delay = TIMER_TEST_DELAY_1000_MS;
    test_info3.num_repeats = TIMER_TEST_DELAY_10000_MS / TIMER_TEST_DELAY_1000_MS - 1;
    test_info4.test_suite = inSuite;
    test_info4.repeat_delay = TIMER_TEST_DELAY_2000_MS;
    test_info4.num_repeats = TIMER_TEST_DELAY_10000_MS / TIMER_TEST_DELAY_2000_MS - 1;
    test_info5.test_suite = inSuite;
    test_info5.repeat_delay = TIMER_TEST_DELAY_5000_MS;
    test_info5.num_repeats = TIMER_TEST_DELAY_10000_MS / TIMER_TEST_DELAY_5000_MS - 1;

    test_info4.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_2000_MS);
    test_info4.expectedRunTimeMax = test_info4.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer4, TIMER_TEST_DELAY_2000_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer4));

    test_info1.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_100_MS);
    test_info1.expectedRunTimeMax = test_info1.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer1, TIMER_TEST_DELAY_100_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1));

    test_info5.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_5000_MS);
    test_info5.expectedRunTimeMax = test_info5.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer5, TIMER_TEST_DELAY_5000_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer5));

    test_info2.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_500_MS);
    test_info2.expectedRunTimeMax = test_info2.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer2, TIMER_TEST_DELAY_500_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer2));

    test_info3.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_1000_MS);
    test_info3.expectedRunTimeMax = test_info3.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer3, TIMER_TEST_DELAY_1000_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer3));

    // delay until we expect everything to have been run.  since
    // restarting a timer from within itself isn't quite the same
    // as true periodic, because of the extra tick added to the
    // each start, we have to wait a bit longer than 10 seconds
    vTaskDelay((nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_5000_MS)*2 + (test_info1.num_repeats * (TIMING_ERROR_TOLERANCE_TICKS + 1))));
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer2) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer3) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer4) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer5) == false);
    NL_TEST_ASSERT(inSuite, test_info1.count == test_info1.num_repeats + 1);
    NL_TEST_ASSERT(inSuite, test_info2.count == test_info2.num_repeats + 1);
    NL_TEST_ASSERT(inSuite, test_info3.count == test_info3.num_repeats + 1);
    NL_TEST_ASSERT(inSuite, test_info4.count == test_info4.num_repeats + 1);
    NL_TEST_ASSERT(inSuite, test_info5.count == test_info5.num_repeats + 1);

    // cleanup just in case of failure before we run next test, else
    // our stack timer structure will corrupt the nl_swtimer implementation
    (void)nl_swtimer_cancel(&timer1);
    (void)nl_swtimer_cancel(&timer2);
    (void)nl_swtimer_cancel(&timer3);
    (void)nl_swtimer_cancel(&timer4);
    (void)nl_swtimer_cancel(&timer5);
}

static uint32_t cascade_timer_test(nl_swtimer_t *timer, void *arg)
{
    timer_test_info2_t *test_info = (timer_test_info2_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();
    unsigned i;

    test_info->count++;
    // we should run at the expected time or at most one or two
    // ticks later (never early).  late is due to delta added
    // by the implementation (1 tick usually) to guarantee
    // we're not early, and 1 more in case there was delay
    // associated with the time we recorded the expected time
    // and another tick arriving, or other added latency.
    NL_TEST_ASSERT(test_info->test_suite,
                   (current_tick_count >= test_info->expectedRunTimeMin) &&
                   (current_tick_count <= test_info->expectedRunTimeMax));
    for (i = 0; i < test_info->num_timers; i++)
    {
        test_info->timer_infos[i]->expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(test_info->timer_delays[i]);
        test_info->timer_infos[i]->expectedRunTimeMax = test_info->timer_infos[i]->expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
        nl_swtimer_start(test_info->timers[i], test_info->timer_delays[i]);
    }
    if (test_info->notifyTaskHandle)
    {
        BaseType_t yield = pdFALSE;
        vTaskNotifyGiveFromISR(test_info->notifyTaskHandle, &yield);
        portEND_SWITCHING_ISR(yield);
    }
    return 0;
}

static void Test_cascade_five_timers(nlTestSuite *inSuite, void *inContext)
{
    BaseType_t wait_result;
    timer_test_info2_t test_info1;
    timer_test_info2_t test_info2;
    timer_test_info2_t test_info3;
    timer_test_info2_t test_info4;
    timer_test_info2_t test_info5;
    nl_swtimer_t timer1;
    nl_swtimer_t timer2;
    nl_swtimer_t timer3;
    nl_swtimer_t timer4;
    nl_swtimer_t timer5;
    nl_swtimer_t *timer_list1[1];
    uint32_t timer_delays1[1];
    timer_test_info2_t *timer_infos1[1];
    nl_swtimer_t *timer_list2[3];
    uint32_t timer_delays2[3];
    timer_test_info2_t *timer_infos2[3];

    if (s_test_with_tick_count_near_wrap)
    {
        AdjustTickCount(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_50_MS));
    }

    // test five one shot timers where timer1 starts timer2 from
    // it's timer function, and timer2 starts timer3, timer4, and
    // timer5 from it's timer function.
    // timer1 has a timeout of 100MS
    // timer2 has a timeout of 500MS
    // timer3 has a timeout of 1000MS
    // timer4 has a timeout of 1000MS
    // timer5 has a timeout of 2000MS
    printf("%s: start. test takes about 3 seconds...\n", __func__);
    ulTaskNotifyTake(pdTRUE, 0); // clear any old notifications
    memset(&test_info1, 0, sizeof(test_info1));
    memset(&test_info2, 0, sizeof(test_info2));
    memset(&test_info3, 0, sizeof(test_info3));
    memset(&test_info4, 0, sizeof(test_info4));
    memset(&test_info5, 0, sizeof(test_info5));
    nl_swtimer_init(&timer1, cascade_timer_test, &test_info1);
    nl_swtimer_init(&timer2, cascade_timer_test, &test_info2);
    nl_swtimer_init(&timer3, cascade_timer_test, &test_info3);
    nl_swtimer_init(&timer4, cascade_timer_test, &test_info4);
    nl_swtimer_init(&timer5, cascade_timer_test, &test_info5);
    test_info1.test_suite = inSuite;
    test_info1.num_timers = 1;
    timer_list1[0] = &timer2;
    timer_delays1[0] = TIMER_TEST_DELAY_500_MS;
    timer_infos1[0] = &test_info2;
    test_info1.timers = timer_list1;
    test_info1.timer_infos = timer_infos1;
    test_info1.timer_delays = timer_delays1;

    test_info2.test_suite = inSuite;
    test_info2.num_timers = 3;
    timer_list2[0] = &timer3;
    timer_list2[1] = &timer4;
    timer_list2[2] = &timer5;
    timer_delays2[0] = TIMER_TEST_DELAY_1000_MS;
    timer_delays2[1] = TIMER_TEST_DELAY_1000_MS;
    timer_delays2[2] = TIMER_TEST_DELAY_2000_MS;
    timer_infos2[0] = &test_info3;
    timer_infos2[1] = &test_info4;
    timer_infos2[2] = &test_info5;
    test_info2.timers = timer_list2;
    test_info2.timer_infos = timer_infos2;
    test_info2.timer_delays = timer_delays2;

    test_info3.test_suite = inSuite;
    test_info4.test_suite = inSuite;
    test_info5.test_suite = inSuite;
    test_info5.notifyTaskHandle = xTaskGetCurrentTaskHandle();

    test_info1.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_100_MS);
    test_info1.expectedRunTimeMax = test_info1.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer1, TIMER_TEST_DELAY_100_MS);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1));
    // delay until we expect everything to have been run.  since
    // restarting a timer from within itself isn't quite the same
    // as true periodic, because of the extra tick added to the
    // each start, we have to wait a bit longer than 10 seconds
    wait_result = ulTaskNotifyTake(pdTRUE, nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_5000_MS));
    NL_TEST_ASSERT(inSuite, wait_result != 0); // assert did not timeout
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer2) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer3) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer4) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer5) == false);
    NL_TEST_ASSERT(inSuite, test_info1.count == 1);
    NL_TEST_ASSERT(inSuite, test_info2.count == 1);
    NL_TEST_ASSERT(inSuite, test_info3.count == 1);
    NL_TEST_ASSERT(inSuite, test_info4.count == 1);
    NL_TEST_ASSERT(inSuite, test_info5.count == 1);

    // cleanup just in case of failure before we run next test, else
    // our stack timer structure will corrupt the nl_swtimer implementation
    (void)nl_swtimer_cancel(&timer1);
    (void)nl_swtimer_cancel(&timer2);
    (void)nl_swtimer_cancel(&timer3);
    (void)nl_swtimer_cancel(&timer4);
    (void)nl_swtimer_cancel(&timer5);
}

static void Test_cancel_from_timer_func(nlTestSuite *inSuite, void *inContext)
{
    timer_test_info_t test_info1;
    timer_test_info_t test_info2;
    timer_test_info_t test_info3;
    nl_swtimer_t timer1;
    nl_swtimer_t timer2;
    nl_swtimer_t timer3;

    if (s_test_with_tick_count_near_wrap)
    {
        AdjustTickCount(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_50_MS));
    }

    // Start timer 1 as a repeating timer with 100ms period, repeating 9 times
    // Start timer 2 as a repeating timer with 100ms period, repeating 9 times
    // Start timer 3 as a one shot timer with 550ms delay, that cancels timer 1
    printf("%s: start\n", __func__);
    memset(&test_info1, 0, sizeof(test_info1));
    memset(&test_info2, 0, sizeof(test_info2));
    memset(&test_info3, 0, sizeof(test_info3));
    test_info1.test_suite = inSuite;
    test_info1.num_repeats = 9;
    test_info1.repeat_delay = TIMER_TEST_DELAY_100_MS;
    test_info2.test_suite = inSuite;
    test_info2.num_repeats = 9;
    test_info2.repeat_delay = TIMER_TEST_DELAY_100_MS;
    test_info3.test_suite = inSuite;
    // overload num_repeats to pass a ptr to the timer we want to cancel
    test_info3.num_repeats = (uint32_t)&timer1;
    nl_swtimer_init(&timer1, repeat_timer_test, &test_info1);
    nl_swtimer_init(&timer2, repeat_timer_test, &test_info2);
    nl_swtimer_init(&timer3, cancel_timer_test, &test_info3);
    test_info1.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_100_MS);
    test_info1.expectedRunTimeMax = test_info1.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer1, TIMER_TEST_DELAY_100_MS);
    test_info2.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_100_MS);
    test_info2.expectedRunTimeMax = test_info2.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer2, TIMER_TEST_DELAY_100_MS);
    test_info3.expectedRunTimeMin = xTaskGetTickCount() + nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_500_MS + TIMER_TEST_DELAY_50_MS);
    test_info3.expectedRunTimeMax = test_info3.expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(&timer3, (TIMER_TEST_DELAY_500_MS + TIMER_TEST_DELAY_50_MS));
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1));
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer2));
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer3));
    vTaskDelay(nl_time_ms_to_delay_time_native(TIMER_TEST_DELAY_2000_MS));
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer1) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer2) == false);
    NL_TEST_ASSERT(inSuite, nl_swtimer_is_active(&timer3) == false);
    NL_TEST_ASSERT(inSuite, test_info1.count == 5);
    NL_TEST_ASSERT(inSuite, test_info2.count == 10);
    NL_TEST_ASSERT(inSuite, test_info3.count == 1);

    // cleanup just in case of failure before we run next test, else
    // our stack timer structure will corrupt the nl_swtimer implementation
    (void)nl_swtimer_cancel(&timer1);
    (void)nl_swtimer_cancel(&timer2);
    (void)nl_swtimer_cancel(&timer3);
}

static const nlTest sTests[] = {
    NL_TEST_DEF("one shot timer test", Test_one_shot),
    NL_TEST_DEF("single repeat timer test", Test_single_repeat),
    NL_TEST_DEF("one shot cancelled timer test", Test_one_shot_cancel),
    NL_TEST_DEF("one shot cancel and restart timer test", Test_one_shot_cancel_restart),
    NL_TEST_DEF("cascade five timer test", Test_cascade_five_timers),
    NL_TEST_DEF("cancel from timer func test", Test_cancel_from_timer_func),
    NL_TEST_DEF("repeat 100 timer test", Test_repeat_100),
    NL_TEST_DEF("five timer test", Test_five_timers),
    NL_TEST_DEF("five timer test mixed", Test_five_timers_mixed),
    NL_TEST_DEF("immediate expiration", Test_immediate_expiration),
    NL_TEST_SENTINEL()
};

static const nlTest sSleepTests[] = {
    NL_TEST_DEF("timers with sleep enabled test", Test_timers_with_sleep_enabled),
    NL_TEST_SENTINEL()
};

#define DO_STRESS_TESTS 0

#if DO_STRESS_TESTS

#define SHORT_ITERATIONS 1

#if SHORT_ITERATIONS
/* Controls how long to run each iteration of the stress test.  With
 * timer1 set at 100ms repeating rate, this should cause each iteration
 * of the test to take about 120 seconds (2 minutes)
 */
#define MAX_STRESS_TEST_COUNT 1200

/* Controls what we reset the tick count to be before each iteration is
 * run in order to make sure we're testing the tick counter wrapping case.
 * We try to have the wrap occur roughly halfway through the test,
 * which is about 60 seconds
 */
#define TICK_COUNT_TARGET_AT_START_OF_STRESS_TEST (nl_time_ms_to_delay_time_native(60000))
#else /* SHORT_ITERATIONS */

/* Controls how long to run each iteration of the stress test.  With
 * timer1 set at 100ms repeating rate, this should cause each iteration
 * of the test to take about 120000 seconds (200 minutes or 3.33hrs)
 */
#define MAX_STRESS_TEST_COUNT 120000

/* Controls what we reset the tick count to be before each iteration is
 * run in order to make sure we're testing the tick counter wrapping case.
 * We try to have the wrap occur roughly halfway through the test,
 * which is about 6000 seconds or 100 minutes
 */
#define TICK_COUNT_TARGET_AT_START_OF_STRESS_TEST (nl_time_ms_to_delay_time_native(6000000))
#endif /* SHORT_ITERATIONS */

/* Controls how many iterations we run.  Currently set to run almost
 * forever.
 */
#define MAX_STRESS_TEST_ITERATIONS 0xffffffff

typedef struct {
    uint32_t   start_count;  // number of times the timer was started
    uint32_t   cancel_count; // number of times the timer was cancelled
    uint32_t   run_count;    // number of times the timer function ran
    nlTestSuite *test_suite;
    uint32_t   repeat_delay_min;
    uint32_t   repeat_delay_max;
    TickType_t expectedRunTimeMin;
    TickType_t expectedRunTimeMax;
} timer_test_info_stress_t;

static nl_swtimer_t stress_timer0;
static nl_swtimer_t stress_timer1;
static nl_swtimer_t stress_timer2;
static nl_swtimer_t stress_timer3;
static nl_swtimer_t stress_timer4;
static nl_swtimer_t stress_timer5;
static nl_swtimer_t stress_timer6;
static nl_swtimer_t stress_timer7;
static nl_swtimer_t stress_timer8;
static nl_swtimer_t stress_timer9;
static timer_test_info_stress_t stress_test_info0;
static timer_test_info_stress_t stress_test_info1;
static timer_test_info_stress_t stress_test_info2;
static timer_test_info_stress_t stress_test_info3;
static timer_test_info_stress_t stress_test_info4;
static timer_test_info_stress_t stress_test_info5;
static timer_test_info_stress_t stress_test_info6;
static timer_test_info_stress_t stress_test_info7;
static timer_test_info_stress_t stress_test_info8;
static timer_test_info_stress_t stress_test_info9;

static void cancel_stress_timer(nl_swtimer_t *timer, timer_test_info_stress_t *test_info)
{
    if (nl_swtimer_cancel(timer))
    {
        test_info->cancel_count++;
    }
}

static void start_stress_timer(nl_swtimer_t *timer, timer_test_info_stress_t *test_info, uint32_t delay_ms, TickType_t current_tick_count)
{
    test_info->expectedRunTimeMin = current_tick_count + nl_time_ms_to_delay_time_native(delay_ms);
    test_info->expectedRunTimeMax = test_info->expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    nl_swtimer_start(timer, delay_ms);
    test_info->start_count++;
}

static void repeat_stress_timer(nl_swtimer_t *timer, timer_test_info_stress_t *test_info, uint32_t delay_ms, TickType_t current_tick_count)
{
    // when using the repeat return argument feature of timer functions, the
    // extra tick isn't added to the delay internally so subtract one that was
    // added by the macro nl_time_ms_to_delay_time_native
    test_info->expectedRunTimeMin = current_tick_count + nl_time_ms_to_delay_time_native(delay_ms) - 1;
    test_info->expectedRunTimeMax = test_info->expectedRunTimeMin + TIMING_ERROR_TOLERANCE_TICKS;
    test_info->start_count++;
}

// we should run at the expected time or at most one or two
// ticks later (never early).  late is due to delta added
// by the implementation (1 tick usually) to guarantee
// we're not early, and 1 more in case there was delay
// associated with the time we recorded the expected time
// and another tick arriving, or other added latency.
static void check_timer_run_time(const char *func, timer_test_info_stress_t *test_info, TickType_t current_tick_count)
{
    test_info->run_count++;
    // check unwrapped and wrapped cases
    if (((test_info->expectedRunTimeMin <= test_info->expectedRunTimeMax) &&
         (((current_tick_count < test_info->expectedRunTimeMin) ||
           (current_tick_count > test_info->expectedRunTimeMax)))) ||
        ((test_info->expectedRunTimeMin > test_info->expectedRunTimeMax) &&
         (((current_tick_count < test_info->expectedRunTimeMin) &&
           (current_tick_count > test_info->expectedRunTimeMax)))))
    {
        printf("%s: current_ticks = %u, expected_min = %u, expected_max = %u\n",
               func, current_tick_count, test_info->expectedRunTimeMin, test_info->expectedRunTimeMax);
        assert(0);
    }
}

static void check_timer_counts(unsigned timer_num, timer_test_info_stress_t *test_info)
{
    printf("timer %u\n", timer_num);
    printf("\tstart_count = %lu\n", test_info->start_count);
    printf("\tcancel_count = %lu\n", test_info->cancel_count);
    printf("\trun_count = %lu\n", test_info->run_count);
    if (test_info->run_count != test_info->start_count - test_info->cancel_count)
    {
        printf("\tERROR: run_count != start_count - cancel_count\n");
        assert(0);
    }
}

static uint32_t stress_timer0_func(nl_swtimer_t *timer, void *arg)
{
    timer_test_info_stress_t *test_info = (timer_test_info_stress_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();
    uint32_t repeat_delay_ms;

    check_timer_run_time(__func__, test_info, current_tick_count);

    if (rand() > 0x80000000)
    {
        cancel_stress_timer(&stress_timer4, &stress_test_info4);
        cancel_stress_timer(&stress_timer5, &stress_test_info5);
        start_stress_timer(&stress_timer4, &stress_test_info4, TIMER_TEST_DELAY_500_MS, current_tick_count);
        start_stress_timer(&stress_timer5, &stress_test_info5, TIMER_TEST_DELAY_500_MS, current_tick_count);

        /* use return value to schedule repeat */
        repeat_delay_ms = test_info->repeat_delay_min;
        repeat_stress_timer(timer, test_info, test_info->repeat_delay_min, current_tick_count);
    }
    else
    {
        /* use function to schedule repeat */
        repeat_delay_ms = 0;
        start_stress_timer(timer, test_info, test_info->repeat_delay_min, current_tick_count);
    }
    return repeat_delay_ms;
}

static uint32_t stress_timer1_func(nl_swtimer_t *timer, void *arg)
{
    timer_test_info_stress_t *test_info = (timer_test_info_stress_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();
    uint32_t repeat_delay_ms;

    check_timer_run_time(__func__, test_info, current_tick_count);

    if (rand() > 0x80000000)
    {
        cancel_stress_timer(&stress_timer6, &stress_test_info6);
        start_stress_timer(&stress_timer6, &stress_test_info6, TIMER_TEST_DELAY_500_MS, current_tick_count);
        cancel_stress_timer(&stress_timer7, &stress_test_info7);
        start_stress_timer(&stress_timer7, &stress_test_info7, TIMER_TEST_DELAY_500_MS, current_tick_count);

        /* use return value to schedule repeat */
        repeat_delay_ms = test_info->repeat_delay_min;
        repeat_stress_timer(timer, test_info, test_info->repeat_delay_min, current_tick_count);
    }
    else
    {
        /* use start function to schedule repeat */
        repeat_delay_ms = 0;
        start_stress_timer(timer, test_info, test_info->repeat_delay_min, current_tick_count);
    }
    return repeat_delay_ms;
}

static uint32_t stress_timer2_func(nl_swtimer_t *timer, void *arg)
{
    timer_test_info_stress_t *test_info = (timer_test_info_stress_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();
    uint32_t repeat_delay_ms;

    check_timer_run_time(__func__, test_info, current_tick_count);

    if (rand() > 0x80000000)
    {
        cancel_stress_timer(&stress_timer8, &stress_test_info8);
        cancel_stress_timer(&stress_timer9, &stress_test_info9);
        start_stress_timer(&stress_timer8, &stress_test_info8, TIMER_TEST_DELAY_500_MS, current_tick_count);
        start_stress_timer(&stress_timer9, &stress_test_info9, TIMER_TEST_DELAY_500_MS, current_tick_count);

        /* use start function to schedule repeat */
        repeat_delay_ms = 0;
        start_stress_timer(timer, test_info, test_info->repeat_delay_min, current_tick_count);
    }
    else
    {
        /* use return value to schedule repeat */
        repeat_delay_ms = test_info->repeat_delay_min;
        repeat_stress_timer(timer, test_info, test_info->repeat_delay_min, current_tick_count);
    }
    return repeat_delay_ms;
}

static uint32_t stress_timer3_func(nl_swtimer_t *timer, void *arg)
{
    timer_test_info_stress_t *test_info = (timer_test_info_stress_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();
    uint32_t stress_number;

    check_timer_run_time(__func__, test_info, current_tick_count);

    stress_number = rand();
    if (stress_number > 0xB0000000)
    {
        cancel_stress_timer(&stress_timer4, &stress_test_info4);
        start_stress_timer(&stress_timer4, &stress_test_info4, TIMER_TEST_DELAY_500_MS, current_tick_count);
    }
    stress_number = rand();
    if (stress_number > 0xB0000000)
    {
        cancel_stress_timer(&stress_timer6, &stress_test_info6);
        start_stress_timer(&stress_timer6, &stress_test_info6, TIMER_TEST_DELAY_500_MS, current_tick_count);
    }
    stress_number = rand();
    if (stress_number > 0xB0000000)
    {
        cancel_stress_timer(&stress_timer8, &stress_test_info8);
        start_stress_timer(&stress_timer8, &stress_test_info8, TIMER_TEST_DELAY_500_MS, current_tick_count);
    }
    start_stress_timer(timer, test_info, test_info->repeat_delay_min, current_tick_count);
    return 0;
}

/* The code for timer functions 4-9 are the same, but we keep them separate
 * so that the trace messages are distinct when a problem is encountered.
 */
static uint32_t stress_timer4_func(nl_swtimer_t *timer, void *arg)
{
    timer_test_info_stress_t *test_info = (timer_test_info_stress_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();
    TickType_t repeat_delay;
    uint32_t stress_number = rand();

    check_timer_run_time(__func__, test_info, current_tick_count);

    repeat_delay = (stress_number % (test_info->repeat_delay_max - test_info->repeat_delay_min)) + test_info->repeat_delay_min;

    repeat_stress_timer(timer, test_info, repeat_delay, current_tick_count);
    return repeat_delay;
}

static uint32_t stress_timer5_func(nl_swtimer_t *timer, void *arg)
{
    timer_test_info_stress_t *test_info = (timer_test_info_stress_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();
    TickType_t repeat_delay;
    uint32_t stress_number = rand();

    check_timer_run_time(__func__, test_info, current_tick_count);

    repeat_delay = (stress_number % (test_info->repeat_delay_max - test_info->repeat_delay_min)) + test_info->repeat_delay_min;

    repeat_stress_timer(timer, test_info, repeat_delay, current_tick_count);
    return repeat_delay;
}

static uint32_t stress_timer6_func(nl_swtimer_t *timer, void *arg)
{
    timer_test_info_stress_t *test_info = (timer_test_info_stress_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();
    TickType_t repeat_delay;
    uint32_t stress_number = rand();

    check_timer_run_time(__func__, test_info, current_tick_count);

    repeat_delay = (stress_number % (test_info->repeat_delay_max - test_info->repeat_delay_min)) + test_info->repeat_delay_min;

    repeat_stress_timer(timer, test_info, repeat_delay, current_tick_count);
    return repeat_delay;
}

static uint32_t stress_timer7_func(nl_swtimer_t *timer, void *arg)
{
    timer_test_info_stress_t *test_info = (timer_test_info_stress_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();
    TickType_t repeat_delay;
    uint32_t stress_number = rand();

    check_timer_run_time(__func__, test_info, current_tick_count);

    repeat_delay = (stress_number % (test_info->repeat_delay_max - test_info->repeat_delay_min)) + test_info->repeat_delay_min;

    repeat_stress_timer(timer, test_info, repeat_delay, current_tick_count);
    return repeat_delay;
}

static uint32_t stress_timer8_func(nl_swtimer_t *timer, void *arg)
{
    timer_test_info_stress_t *test_info = (timer_test_info_stress_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();
    TickType_t repeat_delay;
    uint32_t stress_number = rand();

    check_timer_run_time(__func__, test_info, current_tick_count);

    repeat_delay = (stress_number % (test_info->repeat_delay_max - test_info->repeat_delay_min)) + test_info->repeat_delay_min;

    repeat_stress_timer(timer, test_info, repeat_delay, current_tick_count);
    return repeat_delay;
}

static uint32_t stress_timer9_func(nl_swtimer_t *timer, void *arg)
{
    timer_test_info_stress_t *test_info = (timer_test_info_stress_t*)arg;
    TickType_t current_tick_count = xTaskGetTickCount();
    TickType_t repeat_delay;
    uint32_t stress_number = rand();

    check_timer_run_time(__func__, test_info, current_tick_count);

    repeat_delay = (stress_number % (test_info->repeat_delay_max - test_info->repeat_delay_min)) + test_info->repeat_delay_min;

    repeat_stress_timer(timer, test_info, repeat_delay, current_tick_count);
    return repeat_delay;
}

static void Test_stress_timers(nlTestSuite *inSuite, void *inContext)
{
    /* Create 10 timers that do a bunch of stress tests until
     * one of our counters wrap and then exit.
     *
     * timer0: repeats every 100ms, stressly cancels and starts timer4 and timer5
     * timer1: repeats every 100ms, stressly cancels and starts timer6 and timer7
     * timer2: repeats every 100ms, stressly cancels and starts timer8 and timer9
     * timer3: repeats every 500ms, stressly cancels timer4, timer6, and timer8
     * timer4: starts with initial delay of 500ms, self repeats stressly from 1ms to 200ms,
     * timer5: starts with initial delay of 500ms, self repeats stressly from 1ms to 200ms,
     * timer6: starts with initial delay of 500ms, self repeats stressly from 100ms to 500ms,
     * timer7: starts with initial delay of 500ms, self repeats stressly from 100ms to 2000ms,
     * timer8: starts with initial delay of 500ms, self repeats stressly from 500ms to 1000ms,
     * timer9: starts with initial delay of 500ms, self repeats stressly from 50ms to 1000ms,
     * TestThread: vTaskDelay() for stress times between 1ms and 5000ms,
     *             cancels timer5 and timer7, checks
     *             counts and stops test when counts > limit.
     */
    printf("%s: start. test takes a long time!\n", __func__);
    memset(&stress_test_info0, 0, sizeof(stress_test_info0));
    memset(&stress_test_info1, 0, sizeof(stress_test_info1));
    memset(&stress_test_info2, 0, sizeof(stress_test_info2));
    memset(&stress_test_info3, 0, sizeof(stress_test_info3));
    memset(&stress_test_info4, 0, sizeof(stress_test_info4));
    memset(&stress_test_info5, 0, sizeof(stress_test_info5));
    memset(&stress_test_info6, 0, sizeof(stress_test_info6));
    memset(&stress_test_info7, 0, sizeof(stress_test_info7));
    memset(&stress_test_info8, 0, sizeof(stress_test_info8));
    memset(&stress_test_info9, 0, sizeof(stress_test_info9));

    stress_test_info0.test_suite = inSuite;
    stress_test_info0.repeat_delay_min = TIMER_TEST_DELAY_100_MS;
    stress_test_info0.repeat_delay_max = TIMER_TEST_DELAY_100_MS;
    stress_test_info1.test_suite = inSuite;
    stress_test_info1.repeat_delay_min = TIMER_TEST_DELAY_100_MS;
    stress_test_info1.repeat_delay_max = TIMER_TEST_DELAY_100_MS;
    stress_test_info2.test_suite = inSuite;
    stress_test_info2.repeat_delay_min = TIMER_TEST_DELAY_100_MS;
    stress_test_info2.repeat_delay_max = TIMER_TEST_DELAY_100_MS;
    stress_test_info3.test_suite = inSuite;
    stress_test_info3.repeat_delay_min = TIMER_TEST_DELAY_500_MS;
    stress_test_info3.repeat_delay_max = TIMER_TEST_DELAY_500_MS;
    stress_test_info4.test_suite = inSuite;
    stress_test_info4.repeat_delay_min = TIMER_TEST_DELAY_1_MS;
    stress_test_info4.repeat_delay_max = TIMER_TEST_DELAY_200_MS;
    stress_test_info5.test_suite = inSuite;
    stress_test_info5.repeat_delay_min = TIMER_TEST_DELAY_1_MS;
    stress_test_info5.repeat_delay_max = TIMER_TEST_DELAY_200_MS;
    stress_test_info6.test_suite = inSuite;
    stress_test_info6.repeat_delay_min = TIMER_TEST_DELAY_100_MS;
    stress_test_info6.repeat_delay_max = TIMER_TEST_DELAY_500_MS;
    stress_test_info7.test_suite = inSuite;
    stress_test_info7.repeat_delay_min = TIMER_TEST_DELAY_100_MS;
    stress_test_info7.repeat_delay_max = TIMER_TEST_DELAY_2000_MS;
    stress_test_info8.test_suite = inSuite;
    stress_test_info8.repeat_delay_min = TIMER_TEST_DELAY_500_MS;
    stress_test_info8.repeat_delay_max = TIMER_TEST_DELAY_1000_MS;
    stress_test_info9.test_suite = inSuite;
    stress_test_info9.repeat_delay_min = TIMER_TEST_DELAY_50_MS;
    stress_test_info9.repeat_delay_max = TIMER_TEST_DELAY_1000_MS;

    nl_swtimer_init(&stress_timer0, stress_timer0_func, &stress_test_info0);
    nl_swtimer_init(&stress_timer1, stress_timer1_func, &stress_test_info1);
    nl_swtimer_init(&stress_timer2, stress_timer2_func, &stress_test_info2);
    nl_swtimer_init(&stress_timer3, stress_timer3_func, &stress_test_info3);
    nl_swtimer_init(&stress_timer4, stress_timer4_func, &stress_test_info4);
    nl_swtimer_init(&stress_timer5, stress_timer5_func, &stress_test_info5);
    nl_swtimer_init(&stress_timer6, stress_timer6_func, &stress_test_info6);
    nl_swtimer_init(&stress_timer7, stress_timer7_func, &stress_test_info7);
    nl_swtimer_init(&stress_timer8, stress_timer8_func, &stress_test_info8);
    nl_swtimer_init(&stress_timer9, stress_timer9_func, &stress_test_info9);

    nlplatform_interrupt_disable();
    TickType_t current_tick_count = xTaskGetTickCount();
    start_stress_timer(&stress_timer0, &stress_test_info0, TIMER_TEST_DELAY_100_MS, current_tick_count);
    start_stress_timer(&stress_timer1, &stress_test_info1, TIMER_TEST_DELAY_100_MS, current_tick_count);
    start_stress_timer(&stress_timer2, &stress_test_info2, TIMER_TEST_DELAY_100_MS, current_tick_count);
    start_stress_timer(&stress_timer3, &stress_test_info3, TIMER_TEST_DELAY_500_MS, current_tick_count);
    start_stress_timer(&stress_timer4, &stress_test_info4, TIMER_TEST_DELAY_500_MS, current_tick_count);
    start_stress_timer(&stress_timer5, &stress_test_info5, TIMER_TEST_DELAY_500_MS, current_tick_count);
    start_stress_timer(&stress_timer6, &stress_test_info6, TIMER_TEST_DELAY_500_MS, current_tick_count);
    start_stress_timer(&stress_timer7, &stress_test_info7, TIMER_TEST_DELAY_500_MS, current_tick_count);
    start_stress_timer(&stress_timer8, &stress_test_info8, TIMER_TEST_DELAY_500_MS, current_tick_count);
    start_stress_timer(&stress_timer9, &stress_test_info9, TIMER_TEST_DELAY_500_MS, current_tick_count);
    nlplatform_interrupt_enable();

    while (1)
    {
        TickType_t delay = nl_time_ms_to_delay_time_native(rand() % 5000);
        vTaskDelay(delay);
        nlplatform_interrupt_disable();
        current_tick_count = xTaskGetTickCount();
        cancel_stress_timer(&stress_timer5, &stress_test_info5);
        cancel_stress_timer(&stress_timer7, &stress_test_info7);
        printf("\n%s: curent_tick_count = %u\n", __func__, current_tick_count);
        printf("stress_test_info0.run_count = %lu\n", stress_test_info0.run_count);
        printf("stress_test_info1.run_count = %lu\n", stress_test_info1.run_count);
        printf("stress_test_info2.run_count = %lu\n", stress_test_info2.run_count);
        printf("stress_test_info3.run_count = %lu\n", stress_test_info3.run_count);
        printf("stress_test_info4.run_count = %lu\n", stress_test_info4.run_count);
        printf("stress_test_info5.run_count = %lu\n", stress_test_info5.run_count);
        printf("stress_test_info6.run_count = %lu\n", stress_test_info6.run_count);
        printf("stress_test_info7.run_count = %lu\n", stress_test_info7.run_count);
        printf("stress_test_info8.run_count = %lu\n", stress_test_info8.run_count);
        printf("stress_test_info9.run_count = %lu\n", stress_test_info9.run_count);
        nlplatform_interrupt_enable();
        if ((stress_test_info0.run_count > MAX_STRESS_TEST_COUNT) ||
            (stress_test_info1.run_count > MAX_STRESS_TEST_COUNT) ||
            (stress_test_info2.run_count > MAX_STRESS_TEST_COUNT) ||
            (stress_test_info3.run_count > MAX_STRESS_TEST_COUNT) ||
            (stress_test_info4.run_count > MAX_STRESS_TEST_COUNT) ||
            (stress_test_info5.run_count > MAX_STRESS_TEST_COUNT) ||
            (stress_test_info6.run_count > MAX_STRESS_TEST_COUNT) ||
            (stress_test_info7.run_count > MAX_STRESS_TEST_COUNT) ||
            (stress_test_info8.run_count > MAX_STRESS_TEST_COUNT) ||
            (stress_test_info9.run_count > MAX_STRESS_TEST_COUNT))
        {
            break;
        }
    }
    // disable all timers in critical section to make sure none restart others
    nlplatform_interrupt_disable();
    cancel_stress_timer(&stress_timer0, &stress_test_info0);
    cancel_stress_timer(&stress_timer1, &stress_test_info1);
    cancel_stress_timer(&stress_timer2, &stress_test_info2);
    cancel_stress_timer(&stress_timer3, &stress_test_info3);
    cancel_stress_timer(&stress_timer4, &stress_test_info4);
    cancel_stress_timer(&stress_timer5, &stress_test_info5);
    cancel_stress_timer(&stress_timer6, &stress_test_info6);
    cancel_stress_timer(&stress_timer7, &stress_test_info7);
    cancel_stress_timer(&stress_timer8, &stress_test_info8);
    cancel_stress_timer(&stress_timer9, &stress_test_info9);
    nlplatform_interrupt_enable();
    check_timer_counts(0, &stress_test_info0);
    check_timer_counts(1, &stress_test_info1);
    check_timer_counts(2, &stress_test_info2);
    check_timer_counts(3, &stress_test_info3);
    check_timer_counts(4, &stress_test_info4);
    check_timer_counts(5, &stress_test_info5);
    check_timer_counts(6, &stress_test_info6);
    check_timer_counts(7, &stress_test_info7);
    check_timer_counts(8, &stress_test_info8);
    check_timer_counts(9, &stress_test_info9);
}

static const nlTest sStressTests[] = {
    NL_TEST_DEF("long running stress timer test", Test_stress_timers),
    NL_TEST_SENTINEL()
};

#endif /* DO_STRESS_TESTS */

static void dummy_task(void *arg)
{
    volatile int *end_flag = (int*)arg;
    printf("dummy_task start\n");
    while (*end_flag == 0)
    {
        nlwatchdog_refresh();
    }
    printf("dummy_task end\n");
    vTaskSuspend(NULL);
}

int nl_swtimer_test(void)
{
    nltask_t task;
    int end_dummy_task = 0;
    uint8_t dummy_stack[512];

    sTaskHandle = xTaskGetCurrentTaskHandle();

    /* Align the dummy_stack_ptr to required alignment */
    uint8_t *dummy_stack_ptr = ALIGN_POINTER(dummy_stack, NLER_REQUIRED_STACK_ALIGNMENT);

    nlTestSuite theSuite = {
        "nl_swtimer",
        &sTests[0],
    };

    nlTestSuite theSleepSuite = {
        "nl_swtimer",
        &sSleepTests[0],
    };

#if DO_STRESS_TESTS
    nlTestSuite theStressSuite = {
        "nl_swtimer",
        &sStressTests[0],
    };
#endif

    /* Run the sleep tests first.  The rest of the tests
     * run with sleep disabled in order to do accuracy
     * tests which would get messed up by sleep.
     */
    nlTestRunner(&theSleepSuite, NULL);
    nlTestRunnerStats(&theSleepSuite);
    printf("\nRerunning sleep timer tests with tick count near wrap\n\n");
    s_test_with_tick_count_near_wrap = true;
    nlTestRunner(&theSleepSuite, NULL);
    nlTestRunnerStats(&theSleepSuite);
    s_test_with_tick_count_near_wrap = false;

    /* To allow our timer functions to test timer accuracy,
     * we spawn a thread that just spins at low (but not lowest
     * priority).  The FreeRTOS idle thread function disables
     * task scheduling in it's main loop, and if the tick
     * interrupt happens during scheduler suspend, the tick
     * function is called, but the tick count reported by
     * xTaskGetTickCount() won't be right, so any tests of
     * accuracy from within timer functions can fail.
     */
    nltask_create(dummy_task, "dum", dummy_stack_ptr, sizeof(dummy_stack) - (dummy_stack_ptr - dummy_stack),
                  kIdleTaskPrio + 1, &end_dummy_task, &task);

    /* block sleep since many of our tests check for timer accuracy.
     * will be released by the last test
     */
    g_swtimer_prevent_sleep = true;

    nlTestRunner(&theSuite, NULL);

    printf("\nRerunning timer tests with tick count near wrap\n\n");
    g_swtimer_prevent_sleep = true;
    s_test_with_tick_count_near_wrap = true;
    nlTestRunner(&theSuite, NULL);

#if DO_STRESS_TESTS
    for (unsigned i = 0; i < MAX_STRESS_TEST_ITERATIONS; i++)
    {
        printf("\nRunning stress timer test, iteration %u\n\n", i);

        // set tick count close to wrap so we test wrap handling
        AdjustTickCount(TICK_COUNT_TARGET_AT_START_OF_STRESS_TEST);
        // block sleep since many of our tests check for timer accuracy.
        // will be released by the last test
        g_swtimer_prevent_sleep = true;
        nlTestRunner(&theStressSuite, NULL);
        g_swtimer_prevent_sleep = false;
    }
#endif

    end_dummy_task = 1;
    // yield to give chance for dummy task to exit since it's using our stack for its own
    vTaskDelay(10);

    return nlTestRunnerStats(&theSuite);
}
#endif // BUILD_FEATURE_SW_TIMER_USES_RTOS_TICK
#endif // BUILD_FEATURE_UNIT_TEST
