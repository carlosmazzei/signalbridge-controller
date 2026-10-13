# Host tests

Run from the repository root inside the DevContainer:

```sh
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

The preset enables coverage. Reports are generated in `build-tests/coverage`,
with LCOV data at `build-tests/test/coverage.info`. GCC coverage counters use
atomic updates so the concurrent statistics test does not corrupt the report.

For address and undefined-behavior checks:

```sh
cmake -S . -B build-sanitize -DHOST_BUILD=ON -DBUILD_TESTING=ON -DSIGNALBRIDGE_BUILD_DOCS=OFF -DENABLE_SANITIZERS=ON
cmake --build build-sanitize -j4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-sanitize --output-on-failure
```

Leak detection is disabled because boot-lifetime RTOS/driver allocations in
host fixtures intentionally survive until process exit. Address and UB checks
remain enabled.

Tests use CMocka and the FreeRTOS POSIX port. Hardware APIs are mocked; the
mixed-output test executes real TM1637/TM1639 drivers, and descriptor tests
compile the firmware USB descriptor against vendored TinyUSB headers. The
ADC test executes bounded scans of the actual task with a stationary input.
Snapshot/session, malformed COBS, overflow recovery, concurrent counters,
watchdog health and fail-closed analysis scripts have regression coverage.

Measure source coverage with:

```sh
gcovr --root . --object-directory build-tests --filter src/ --txt-summary
```

The host denominator excludes uncompiled boot, task orchestration and IRQ
paths. POSIX tests do not validate RP2040 SMP timing, USB enumeration on a
physical host, real ACKs, watchdog reset latency or electrical behavior.
