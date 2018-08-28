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
#include <nlenv.h>

#include <FreeRTOS.h>
#include <task.h>

#ifdef BUILD_FEATURE_RESET_INFO

// g_reset_info needs to be in retained/persistent RAM that
// maintains it's values across a reboot.
//
// Currently, two implementations are supported. Either
// g_reset_info is in dedicated retained RAM, or it is
// in temporary/overlaid retained RAM that is only valid
// between a fault and early in the next boot.  The latter
// implementation saves RAM, but requires an external FLASH
// partition in order to save the reset/fault information
// so that it is available for later use by the application,
// Once reset info is written to, the system *MUST* continue
// with a reset soon after and task switching should be disabled.

void nlplatform_reset_info_init_done(void) LINKER_REPLACEABLE_FUNCTION(nlplatform_reset_info_init_done_default);

#ifdef BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM
// NL_RESETINFO_SYMBOL is defined when special section
// is used for RESETINFO.  It is assumed this is the case
// when the section is overlaid with another RAM section
// to reduce RAM usage, instead of just using a dedicated
// area in RETAIN section.
nl_reset_info_t g_reset_info NL_RESETINFO_SYMBOL __attribute__((used));
#ifdef DEBUG
// cache of reset reason for querying after boot since the
// overlaid RAM will be used by something else once boot is done.
static nl_reset_reason_t s_cached_reset_reason;
#endif // DEBUG

static int save_fault_to_flash(void)
{
    int retval;
    char fault_dirty_flag;
    size_t retlen = sizeof(fault_dirty_flag);

    retval = nl_env_get(kFaultDiagsDirtyKey, &fault_dirty_flag, &retlen);
    if (retval >= 0)
    {
        printf(UNIQUE_PRINTF_FORMAT_STRING("Fault info already dirty, not overwriting\n"));
    }
    else
    {
        g_reset_info.fault_info.reason = g_reset_info.reset_reason;
        fault_dirty_flag = '1';
        retval = nlflash_erase(NLFLASH_EXTERNAL, NL_FAULT_DIAGS_FLASH_LOCATION,
                               NL_FAULT_DIAGS_FLASH_SIZE, &retlen, NULL);
        if (retval < 0)
        {
            goto flash_ops_failed;
        }
        retval = nlflash_write(NLFLASH_EXTERNAL, NL_FAULT_DIAGS_FLASH_LOCATION,
                               sizeof(g_reset_info.fault_info),
                               &retlen, (uint8_t*)&g_reset_info.fault_info, NULL);
        if (retval < 0)
        {
            goto flash_ops_failed;
        }
        retval = nl_env_set(kFaultDiagsDirtyKey, (void*)&fault_dirty_flag, sizeof(fault_dirty_flag));
        if (retval < 0)
        {
            goto flash_ops_failed;
        }
        printf(UNIQUE_PRINTF_FORMAT_STRING("Saved reset+fault info to external flash\n"));
    }
flash_ops_failed:
    if (retval < 0)
    {
        printf(UNIQUE_PRINTF_FORMAT_STRING("Saving reset+fault info to external flash failed\n"));
    }
    return retval;
}

void nl_reset_info_init(void)
{
    if (g_reset_info.magic != NL_RESET_INFO_MAGIC)
    {
        // make sure g_reset_info.reset_reason and
        // g_reset_info.fault_info.reason are initialized
        g_reset_info.reset_reason = NL_RESET_REASON_UNKNOWN;
        g_reset_info.fault_info.reason = NL_RESET_REASON_UNKNOWN;
    }
    // check the status of the reset_reason to see
    // if the we have reset from a previous app run due to a fault.
    // if so, save a copy of the fault info flash so we can
    // later send it to the service.  g_reset_info should be
    // preserved at this early point in boot.
    // Only write fault info out if it is a fault and not another
    // type of reset, and only if the flash block isn't already dirty
    // (dirty is indicated by existence of dirty env)
    if ((g_reset_info.magic == NL_RESET_INFO_MAGIC) &&
        IS_VALID_FAULT_REASON(g_reset_info.reset_reason))
    {
        save_fault_to_flash();
    }

#ifdef DEBUG
    // save last reset reason so we can print it later if desired
    if ((g_reset_info.magic == NL_RESET_INFO_MAGIC) &&
        (IS_VALID_RESET_REASON(g_reset_info.reset_reason) ||
         IS_VALID_FAULT_REASON(g_reset_info.reset_reason)))
    {
        s_cached_reset_reason = g_reset_info.reset_reason;
    }
    else
    {
        s_cached_reset_reason = NL_RESET_REASON_UNKNOWN;
    }
    nl_reset_info_print();
    nl_reset_info_print_saved_fault();
#endif // DEBUG

    // clear magic to indicate we've processed previous reset reason
    // and are ready for a new reset
    g_reset_info.magic = 0;
    // allow platforms to do things after we're done with
    // the g_reset_info, in case it is overlaid with some
    // other section
    nlplatform_reset_info_init_done();
}

static void nl_reset_info_set(nl_reset_reason_t reset_reason, const char *fault_description)
{
    g_reset_info.magic = NL_RESET_INFO_MAGIC;
    g_reset_info.reset_reason = reset_reason;
    if (IS_VALID_FAULT_REASON(reset_reason))
    {
        // g_reset_info.fault_info is just a cache, the persistent
        // information is in external FLASH so we always reset the
        // RAM copy here
        g_reset_info.fault_info.reason = NL_RESET_REASON_UNKNOWN;
        if (fault_description != NULL)
        {
            strncpy((void*)&g_reset_info.fault_info.description, fault_description,
                    sizeof(nl_fault_description_t));
        }
    }
}

void nl_reset_info_prepare_reset(nl_reset_reason_t reset_reason, const char *fault_description)
{
    // disable scheduler since we don't want anyone modifying
    // .resetinfo if it's in an overlaid section
    vTaskSuspendAll();

    // sometimes there are nested faults.  if we've already been set, do
    // nothing since we want to retain the first reason.
    if (g_reset_info.magic != NL_RESET_INFO_MAGIC)
    {
        nl_reset_info_set(reset_reason, fault_description);
    }
}

void nl_reset_info_prepare_reset_bootloader(nl_reset_reason_t reset_reason, const char *fault_description)
{
    // If the reset_reason is a fault, we always want to record this
    // in the bootloader case (unlike nl_reset_info_prepare_reset(),
    // which doesn't want to overwrite faults until it's been able
    // to save it off to flash).  Otherwise, a preexisting reset reason
    // like normal SW reset would prevent the bootloader from recording
    // a fault.
    if ((g_reset_info.magic != NL_RESET_INFO_MAGIC) ||
        IS_VALID_FAULT_REASON(reset_reason))
    {
        nl_reset_info_set(reset_reason, fault_description);
    }
}

int nl_reset_info_get_saved_fault(nl_fault_info_t *saved_fault_info)
{
    int retval = -1;

    char fault_dirty_flag;
    size_t retlen = sizeof(fault_dirty_flag);
    if (nl_env_get(kFaultDiagsDirtyKey, &fault_dirty_flag, &retlen) >= 0)
    {
        // read into the provided buffer
        retval = nlflash_read(NLFLASH_EXTERNAL, NL_FAULT_DIAGS_FLASH_LOCATION,
                              sizeof(*saved_fault_info), &retlen,
                              (uint8_t*)saved_fault_info, NULL);
    }

    // sanity check the previous fault info.  especially if there was a
    // change in reset_reason codes, we could get weird info.
    if (retval == 0)
    {
        if (!IS_VALID_FAULT_REASON(saved_fault_info->reason))
        {
            // not valid previous fault, clear it
            nl_reset_info_clear_saved_fault();
            retval = -1;
        }
    }
    return retval;
}

void nl_reset_info_clear_saved_fault(void)
{
    nl_env_set(kFaultDiagsDirtyKey, NULL, 0);
}

#else  // BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM

nl_reset_info_t g_reset_info NL_RETAIN_SYMBOL __attribute__((used));

void nl_reset_info_init(void)
{
    if (g_reset_info.magic != NL_RESET_INFO_MAGIC)
    {
        // make sure g_reset_info.reset_reason and
        // g_reset_info.fault_info.reason are initialized
        g_reset_info.reset_reason = NL_RESET_REASON_UNKNOWN;
        nl_reset_info_clear_saved_fault();
    }

#ifdef DEBUG
    nl_reset_info_print();
    nl_reset_info_print_saved_fault();
#endif // DEBUG

    // clear magic to indicate we've processed previous reset reason
    // and are ready for a new reset
    g_reset_info.magic = 0;
    // allow platforms to do things after we're done with
    // the g_reset_info, in case it is overlaid with some
    // other section
    nlplatform_reset_info_init_done();
}

static void nl_reset_info_set(nl_reset_reason_t reset_reason, const char *fault_description,
                              bool override_fault_reason)
{
    g_reset_info.magic = NL_RESET_INFO_MAGIC;
    g_reset_info.reset_reason = reset_reason;
    if (IS_VALID_FAULT_REASON(reset_reason))
    {
        // don't overwrite last fault info if it's not been cleared by app,
        // to preserve that info for eventual app processing
        if ((g_reset_info.fault_info.reason == NL_RESET_REASON_UNKNOWN) ||
            (override_fault_reason == true))
        {
            g_reset_info.fault_info.reason = reset_reason;
            if (fault_description != NULL)
            {
                strncpy((void*)&g_reset_info.fault_info.description, fault_description,
                        sizeof(nl_fault_description_t));
            }
        }
    }
}

void nl_reset_info_prepare_reset(nl_reset_reason_t reset_reason, const char *fault_description)
{
    // sometimes there are nested faults.  if we've already been set, do
    // nothing since we want to retain the first reason.
    if (g_reset_info.magic != NL_RESET_INFO_MAGIC)
    {
        nl_reset_info_set(reset_reason, fault_description, false);
    }
}

void nl_reset_info_prepare_reset_bootloader(nl_reset_reason_t reset_reason, const char *fault_description)
{
    // If the reset_reason is a fault, we always want to record this
    // in the bootloader case (unlike nl_reset_info_prepare_reset(),
    // which doesn't want to overwrite faults until it's been able
    // to save it off to flash).  Otherwise, a preexisting reset reason
    // like normal SW reset would prevent the bootloader from recording
    // a fault.
    if ((g_reset_info.magic != NL_RESET_INFO_MAGIC) ||
        IS_VALID_FAULT_REASON(reset_reason))
    {
        nl_reset_info_set(reset_reason, fault_description, true);
    }
}


int nl_reset_info_get_saved_fault(nl_fault_info_t *saved_fault_info)
{
    int retval = -1;

    // our RAM is not shared so our fault info (if any) is valid
    if (IS_VALID_FAULT_REASON(g_reset_info.fault_info.reason))
    {
        memcpy(saved_fault_info, &g_reset_info.fault_info, sizeof(g_reset_info.fault_info));
        retval = 0;
    }

    // sanity check the previous fault info.  especially if there was a
    // change in reset_reason codes, we could get weird info.
    if (retval == 0)
    {
        if (!IS_VALID_FAULT_REASON(saved_fault_info->reason))
        {
            // not valid previous fault, clear it
            nl_reset_info_clear_saved_fault();
            retval = -1;
        }
    }
    return retval;
}

void nl_reset_info_clear_saved_fault(void)
{
    memset(&g_reset_info.fault_info, 0, sizeof(g_reset_info.fault_info));
    g_reset_info.fault_info.reason = NL_RESET_REASON_UNKNOWN;
}

#endif // BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM

nl_reset_reason_t nl_reset_info_get_reset_reason(void)
{
    nl_reset_reason_t reset_reason = NL_RESET_REASON_UNKNOWN;

    if ((g_reset_info.magic == NL_RESET_INFO_MAGIC) &&
        (IS_VALID_RESET_REASON(g_reset_info.reset_reason) ||
         IS_VALID_FAULT_REASON(g_reset_info.reset_reason)))
    {
        reset_reason = g_reset_info.reset_reason;
    }

    return reset_reason;
}

#ifdef DEBUG
/* The entries in s_reset_reason_strings and
 * s_fault_reset_reason_strings should match those in
 * nl_reset_reason_t enum under nlreset_info.h
 */
const char *s_reset_reason_strings[NL_RESET_REASON_COUNT] =
{
    [NL_RESET_REASON_UNSPECIFIED] = "unspecified",
    [NL_RESET_REASON_UNKNOWN] = "unknown",
    [NL_RESET_REASON_SW_REQUESTED] = "sw requested",
    [NL_RESET_REASON_SW_UPDATE] = "sw update",
    [NL_RESET_REASON_FACTORY_RESET] = "factory reset"
};

const char *s_fault_reset_reason_strings[NL_RESET_REASON_FAULT_COUNT] =
{
    [NL_RESET_REASON_HARD_FAULT - NL_RESET_REASON_FIRST_FAULT] = "hard fault",
    [NL_RESET_REASON_ASSERT - NL_RESET_REASON_FIRST_FAULT] = "assert",
    [NL_RESET_REASON_WATCHDOG - NL_RESET_REASON_FIRST_FAULT] = "watchdog",
    [NL_RESET_REASON_STACK_OVERFLOW - NL_RESET_REASON_FIRST_FAULT] = "stack overflow"
};

void nl_reset_info_print(void)
{
    nl_reset_reason_t reset_reason;
#ifdef BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM
    reset_reason = s_cached_reset_reason;
#else
    reset_reason = g_reset_info.reset_reason;
#endif
    if (IS_VALID_RESET_REASON(reset_reason))
    {
        printf(UNIQUE_PRINTF_FORMAT_STRING("Last reset reason: %s\n"), s_reset_reason_strings[reset_reason]);
    }
    else if (IS_VALID_FAULT_REASON(reset_reason))
    {
        printf(UNIQUE_PRINTF_FORMAT_STRING("Last fault reset reason: %s\n"), s_fault_reset_reason_strings[reset_reason - NL_RESET_REASON_FIRST_FAULT]);
    }
    else
    {
        printf(UNIQUE_PRINTF_FORMAT_STRING("Invalid last reset reason %u\n"), (unsigned)reset_reason);
    }
}

static const char * const reg_names[] = { " sp", " lr", " pc", "psr" };

void nl_reset_info_print_saved_fault(void)
{
    nl_fault_info_t fault_info;
    nl_fault_task_info_t *task_info;
    unsigned i,j;
    if (nl_reset_info_get_saved_fault(&fault_info) == 0)
    {
        // Found a previous fault_info.  Print out all associated data
        printf(UNIQUE_PRINTF_FORMAT_STRING("Previous fault info found! Printing post-mortem info:\n"));
        printf(UNIQUE_PRINTF_FORMAT_STRING("Fault reason: %s\n"), s_fault_reset_reason_strings[fault_info.reason - NL_RESET_REASON_FIRST_FAULT]);
        printf(UNIQUE_PRINTF_FORMAT_STRING("Fault Registers:\n"));
        for (i = 0; i < 13; i++)
        {
            printf(UNIQUE_PRINTF_FORMAT_STRING("r%d\t0x%08lx\n"), i, (long unsigned int)fault_info.registers[i]);
        }
        for (i = 0; i < ARRAY_SIZE(reg_names); i++)
        {
            printf(UNIQUE_PRINTF_FORMAT_STRING("%s\t0x%08lx\n"), reg_names[i], (long unsigned int)fault_info.registers[i+13]);
        }
        if (fault_info.active_task_name[0])
        {
            printf(UNIQUE_PRINTF_FORMAT_STRING("Task at time of fault: %s\n"), fault_info.active_task_name);
            printf(UNIQUE_PRINTF_FORMAT_STRING("Task Info:\n"));
            for (i = 0; i < ARRAY_SIZE(fault_info.task_info); i++)
            {
                task_info = &fault_info.task_info[i];
                if (task_info->backtrace[0])
                {
                    printf(UNIQUE_PRINTF_FORMAT_STRING("Task: %s%s\n"), task_info->task_name, task_info->task_state);
                    for (j = 0; j < ARRAY_SIZE(task_info->backtrace) && task_info->backtrace[j] != 0; j++)
                    {
                        printf(UNIQUE_PRINTF_FORMAT_STRING("\t0x%08lx\n"), (long unsigned int)task_info->backtrace[j]);
                    }
                }
            }
        }
        if (fault_info.machine_backtrace[0])
        {
            printf(UNIQUE_PRINTF_FORMAT_STRING("Machine Backtrace:\n"));
            for (j = 0; j < ARRAY_SIZE(fault_info.machine_backtrace) && fault_info.machine_backtrace[j] != 0; j++)
            {
                printf(UNIQUE_PRINTF_FORMAT_STRING("\t0x%08lx\n"), (long unsigned int)fault_info.machine_backtrace[j]);
            }
        }
        if (fault_info.description[0])
        {
            printf(UNIQUE_PRINTF_FORMAT_STRING("Fault description: [%*s]\n"), (int)sizeof(fault_info.description),
                   fault_info.description);
        }
    }
    else
    {
        printf(UNIQUE_PRINTF_FORMAT_STRING("No previous fault info\n"));
    }
}
#endif // DEBUG

void nlplatform_reset_info_init_done_default(void);
void nlplatform_reset_info_init_done_default(void)
{
}

#endif // BUILD_FEATURE_RESET_INFO
