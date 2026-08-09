/**
 * @file mock_spi_capture.h
 * @brief Test-only capture contract for the spi_write_blocking mock.
 *
 * Every spi_write_blocking() call made by the code under test is recorded so
 * tests can assert on transaction batching, not just on buffer contents.
 *
 * This lives in its own header so hardware_mocks.c (the writer) and the test
 * translation units (the readers) share a single definition of the record
 * layout. It deliberately does not declare the SPI functions themselves:
 * hardware_mocks.c cannot include mock_headers/hardware/spi.h because the
 * mock's spi_init() signature differs from that header's prototype.
 */
#ifndef MOCK_SPI_CAPTURE_H
#define MOCK_SPI_CAPTURE_H

#include <stdint.h>
#include <stddef.h>

/** Maximum number of recorded spi_write_blocking() calls. */
#define MOCK_SPI_MAX_CALLS     32U
/** Bytes retained per recorded call. */
#define MOCK_SPI_CAPTURE_BYTES 24U

/** One recorded spi_write_blocking() call. */
typedef struct {
    size_t len;                          /**< Length passed to the call. */
    uint8_t data[MOCK_SPI_CAPTURE_BYTES]; /**< Leading bytes of the payload. */
} mock_spi_call_t;

/** Discard all recorded calls. */
void mock_spi_reset(void);

/** @return Number of calls recorded since the last reset. */
size_t mock_spi_call_count(void);

/**
 * @brief Fetch a recorded call.
 *
 * @param[in] index Zero-based call index.
 * @return Pointer to the record, or NULL when @p index is out of range.
 */
const mock_spi_call_t *mock_spi_call(size_t index);

#endif // MOCK_SPI_CAPTURE_H
