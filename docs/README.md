# Documentation Index

Technical documentation for the Signalbridge controller firmware. For build/test
commands and contribution rules, see [`../CLAUDE.md`](../CLAUDE.md). For the
project overview, see [`../README.md`](../README.md).

## Architecture & Protocol
- [ARCHITECTURE.md](ARCHITECTURE.md) — FreeRTOS SMP task layout, queues, data flows, error management.
- [PERFORMANCE.md](PERFORMANCE.md) — prioritized performance analysis: implemented quick wins, proposed optimizations, revert guide.
- [COMMANDS.md](COMMANDS.md) — USB CDC command protocol: COBS framing, command IDs, payload formats, example frames.
- [OUTPUT.md](OUTPUT.md) — output/display subsystem: SPI fabric, TM1639/TM1637 drivers, buffering, concurrency.

## Host Integration
- [SIGNALBRIDGE_HOST_LIB_SPEC.md](SIGNALBRIDGE_HOST_LIB_SPEC.md) — self-contained specification for implementing a host-side communication library (COBS protocol, command reference, event-driven API, multi-board management, wire-format examples).

## Tooling
- [CPPCHECK_SETUP.md](CPPCHECK_SETUP.md) — Cppcheck + MISRA C:2012 configuration and workflow.

## Meta
- [PROMPT.md](PROMPT.md) — guidance for keeping the root README synchronized with the codebase.
- [AGENTS.md](AGENTS.md) — agent persona; defers to [`../CLAUDE.md`](../CLAUDE.md).

## Generated API reference
`Doxyfile` and `mainpage.dox` produce HTML output under `docs/html/` when the
Doxygen target is built. See the **Code Quality and Documentation** section
of the [root README](../README.md) for details.
