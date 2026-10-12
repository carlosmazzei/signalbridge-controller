# MISRA C:2012 Deviation Register

MISRA C:2012 is enforced strictly on project code (`src/`, `include/`):
`scripts/run_cppcheck.sh` returns nonzero for every MISRA diagnostic,
including advisory rules such as 15.5 (single point of exit) and 15.6
(braces). No MISRA rule is listed in `.cppcheck_advisory`.

A finding may only be accepted through one of the deviations below. Each
accepted site carries a comment naming the deviation, so reviewers can audit
it next to the code:

- `// cppcheck-suppress <id> ; Deviation Dn: <reason>` on the line before the
  finding (used by cppcheck directly).
- `// MISRA Deviation D2 (Rule 8.7): <reason>` on the line before a callback;
  `run_cppcheck.sh` turns each marker into a suppression for the next line,
  because cppcheck ignores inline comments for cross-translation-unit findings.

Adding a new deviation requires a new entry here with its rationale. Prefer
changing the code.

| ID | Rule(s) | Scope | Rationale |
|----|---------|-------|-----------|
| D0 | All | `lib/**` and SDK headers generated into the build tree (`*/generated/*`) | Third-party code (Pico SDK, FreeRTOS, TinyUSB) is outside the project's compliance scope. |
| D1 | 1.2 | `src/atomic_u32.c` only | The C11 `<stdatomic.h>` generic functions expand to GCC `__atomic` built-ins. They are the only portable primitive that is safe across both RP2040 cores and IRQ context, so every atomic access goes through the `atomic_u32_*` wrappers (sequentially consistent ordering). |
| D2 | 8.7 | Functions marked `MISRA Deviation D2 (Rule 8.7)` | TinyUSB callbacks (`tud_*_cb`), FreeRTOS hooks (`vApplication*Hook`, `ulPortGetRunTime`) and the SDK panic hook (`panic_handler`) must have external linkage because `lib/` code calls them. cppcheck does not analyze `lib/`, so it sees no second reference. |
| D3 | 11.3 | `spi0` / `uart0` uses in `src/app_outputs.c` | The Pico SDK instance macros cast the peripheral register block to the instance type. SPI access is funnelled through `output_spi()` so the cast appears once. |
| D4 | 10.1, 10.4, 10.8, 12.2 | `bi_decl(...)` in `src/app_outputs.c` | SDK binary-info macro that only emits picotool metadata; its expansion is SDK code. |
| D5 | 10.1, 10.4, 10.8, 12.1, 12.2 | `TUD_CONFIG_DESCRIPTOR` / `TUD_CDC_DESCRIPTOR` in `src/usb_descriptors.c` | TinyUSB descriptor macros. A static assertion checks the generated length against `CONFIG_TOTAL_LEN`, and `test_usb_descriptors` walks the result. |
| D6 | 11.5 | FreeRTOS task entry points and `pvPortMalloc` results | FreeRTOS passes task parameters and heap blocks as `void *`; the conversion happens once, at the task or allocation boundary. |
| D7 | 2.5 | Project-wide (`.cppcheck_suppressions`) | The addon cannot track macro use across translation units: it reports macros used only in `.c` files (for example `TM1637_DIGIT_COUNT`) and FreeRTOS/TinyUSB configuration macros consumed by `lib/`. Review unused macros manually. |

## Tool limitations

cppcheck's MISRA addon is not a complete checker. It has missed, for example,
a function with two `return` statements and a braceless `if` (`tm1639_clear`)
that a manual scan found. A passing run is therefore not a MISRA compliance
certification; reviews must still check rules 15.5 and 15.6 by reading the
code.
