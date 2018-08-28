Overview
========

This is not an officially supported Google product.

`nlplatform` provides driver implementations for multiple hardware platforms
under a common interface. It is composed of several repositories, whose names
start with `platform/nlplatform*`.

The central repository is `platform/nlplatform`; it is hardware-agnostic and
defines interfaces implemented by the other, hardware-specific repositories. It
also provides modules implemented in terms of those interfaces, and some
compiler- and CPU-specific code. The `nlgpio_button.[ch]` module in
`platform/nlplatform` is an example of a hardware agnostic module, built on top
of the `platform/nlgpio.h` and `platform/nlswtimer.h` interfaces.

The hardware-specific repositories at the time of writing are:

-   `platform/nlplatform_sim`
-   `platform/nlplatform_em358x`
-   `platform/nlplatform_nrf52x`
-   `platform/nlplatform_stm32l1`
-   `platform/nlplatform_stm32l4`

Products use `nlplatform` by building against both `platform/nlplatform` and one
of the `platform/nlplatform_<hw>` implementations; it's always a pair.

Developers for a product that uses `nlplatform` access its functions and types
by including `nlplatform.h` first, and then including specific platform
interfaces afterward; the ordering is important and explained later.

``` {.c}
#include <stdio.h>

#include <nlplatform.h>
#include <nlplatform/nlgpio.h>

int main(void)
{
    printf("Compelling example using platform GPIO below\n");

    //
    // Implementation left as an exercise for the reader.
    //

    return 0;
}
```

While `platform/nlplatform` defines all the interfaces in `nlplatform`, some
types in those interfaces are defined by the hardware-specific implementation
it's paired with, and others are provided by the product, through
`nlproduct_config.h`.

For example, `platform/nlplatform/include/nlgpio.h` uses `nlgpio_id_t`, but
doesn't define it; it's defined by the hardware-specific platform entries. In
the em358x platform entry, it is defined by `em3xx_gpio.h`.

Similarly, `platform/nlplatform/include/nltimer.h` uses `nltimer_id_t`, but
doesn't define it; it's defined by the product in its `product_config.h`.

The mechanism for embedding types from the product and hardware-specific
platform entries in `platform/nlplatform` interfaces is carefully ordered,
header-file inclusion. This is why clients of `nlplatform` must include
`nlplatform.h` first.

Two lines near the top of `nlplatform.h` are crucial for this effort:

``` {.c}
#include <nlplatform/nlreset_info.h> // Includes nlproduct_config.h
#include <nlplatform_soc.h>          // Provided by platform/nlplatform_<hw> implementation
```

For support with problems or to learn more please contact the Nest Embedded
Platforms team at nest-embedded-os@google.com.

Available Build Features
========================

Several preprocessor definitions are used to customize the
`platform/nlplatform*` repositories.

The following build features are available in `platform/nlplatform`:

- `BUILD_FEATURE_ANTENNA_DIVERSITY`
    * Makes `nlradio_set_prevent_antenna_switch()` available in `nlradio.h` when
      defined.

- `BUILD_FEATURE_BREADCRUMBS`
    * Adds a `LINKER_REPLACEABLE()` function, `crash_dump_breadcrumbs()`, which
      stores fault information in breadcrumbs. This allows fault information to
      be recovered after a rebooting or power-cycling the device.

- `BUILD_FEATURE_BUTTON_DEBOUNCE`
    * Enables debouncing for `nlgpio_button.c` based buttons. The debounce
      interval is set by `NL_BUTTON_DEBOUNCE_TIME_INTERVAL_MS`.

- `BUILD_FEATURE_FAT_FILES`
    * Adds support for handling FAT files in `nlfs.c`.

- `BUILD_FEATURE_FAULT_DUMP_TASK_STACKS`
    * Dump backtraces for all tasks on fault when defined.

- `BUILD_FEATURE_LOG_TOKENIZATION`
    * Used to define `UNIQUE_LOG_FORMAT_STRING()` in `nlplatform.h` to support
      log tokenization. **NOTE:** This is currently unused.

- `BUILD_FEATURE_NL_PLATFORM_SPI_IPC`
    * Makes `platform/nlspi_ipc.h`, `nlspi_ipc_work()` and `nspi_ipc_send()`
      available.

- `BUILD_FEATURE_NL_PROFILE`
    * Makes `platform/nlprofile.h` available, which provides functions for task
      and resource profiling.

- `BUILD_FEATURE_NL_TRACE`
    * Makes `platform/nltrace.h` available, which provides functions for tracing
      events of interest available.

- `BUILD_FEATURE_NO_BKPT_ON_FAULT`
    * By default `crash_dump_default()`, in `fault.c`, executes a `bkpt`
      instruction (on ARM processors) to pause device execution when a debugger
      is connected. Defining `BUILD_FEATURE_NO_BKPT_ON_FAULT` disables this.

- `BUILD_FEATURE_NO_SPI_FLASH`
    * By default `platform/nlflash_spi.h` and `platform/nlfs.h` are available.
      Defining `BUILD_FEATURE_NO_SPI_FLASH` removes them.

- `BUILD_FEATURE_PLATFORM_SPI_SLAVE_STATISTICS`
    * By default `platform/nlspi_slave.h` is available. The functions in that
      interface related to SPI slave statistics are only included when this is
      defined.

- `BUILD_FEATURE_PRE_WATCHDOG_ISR_EXTENSION`
    * By default `platform/nlwatchdog.h` is available. Defining
      `BUILD_FEATURE_PRE_WATCHDOG_ISR_EXTENSION` makes
      `nlwatchdog_ignore_pre_watchdog_isr()` available (which must be
      implemented by a `platform/nlplatform_<hw>` entry). This function can
      effectively extend the HW watchdog timeout.

- `BUILD_FEATURE_PRINTF_TOKENIZATION`
    * Used in the cpu-specific implementations of `fault.c` and
      `nlreset_info.c`. When `BUILD_FEATURE_PRINTF_TOKENIZATION` is defined it
      allows for the tokeniztion of format strings in calls to `printf()`. When
      not defined, those format strings are made into unique string literals,
      allowing the linker to dead strip them when not used.

- `BUILD_FEATURE_RAM_CONSOLE`
    * The ram console stores the most recent log messages in a section of RAM;
      it's used for debugging. On fault, some things shouldn't be stored. When
      `BUILD_FEATURE_RAM_CONSOLE` is defined, the ram console is **disabled** in
      `dump_context_default()` in `fault.c`.

- `BUILD_FEATURE_RESET_INFO`
    * When defined, the device stores information about each reset it undergoes.

- `BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM`
    * If defined `BUILD_FEATURE_RESET_INFO` must also be defined. Reset info
      must live in a section of RAM that persists across boots. This can be
      achieved either by dedicating a section of retained RAM to reset info, or
      using an overlaid section of retained RAM that the reset info owns between
      a reset and early in the next boot. Defining
      `BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM` chooses the second option.

- `BUILD_FEATURE_RESET_ON_FAULT`
    * When defined the platform will call `nlplatform_reset()` on fault.

- `BUILD_FEATURE_SOFT_PARTITIONS`
    * When defined the partition table, stored in `g_flash_partitions[]`,
      becomes writable.

- `BUILD_FEATURE_SW_CRC`
    * By default the `platform/nlplatform_<hw>` entry implements the functions
      defined in `platform/nlcrc.h`. When `BUILD_FEATURE_SW_CRC` is defined, a
      software based implementation, backed by `platform/nlsystem/nlcrc`, is
      used instead.

- `BUILD_FEATURE_SW_TIMER`
    * The software timer module uses an interrupt to provide a measure of time
      and provide several software timers to clients. Defining
      `BUILD_FEATURE_SW_TIMER` makes the module available through
      `platform/nlswtimer.h`.

- `BUILD_FEATURE_SW_TIMER_USES_RTOS_TICK`
    * If defined `BUILD_FEATURE_SW_TIMER` must also be defined. Defining this
      extends the `platform/nlswtimer.h` interface with functions for
      integrating the software timer module with an RTOS. This includes using
      the RTOS tick handler as the interrupt that drives the software timer
      module.

- `BUILD_FEATURE_UNIT_TEST`
    * When defined unit tests are built and some modules are instrumented with
      logic to permit unit testing.

TODO
====

Necessary for completeness:

- Add descriptions for `NL_` -style configuration options in
  `platform/nlplatform`.
