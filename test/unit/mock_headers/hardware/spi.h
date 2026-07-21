#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct spi_inst_t spi_inst_t;
extern spi_inst_t *spi0;

void spi_init(spi_inst_t *spi, unsigned int baudrate);
void spi_set_format(spi_inst_t *spi, unsigned int data_bits, unsigned int cpol, unsigned int cpha, bool lsb_first);
int spi_write_blocking(spi_inst_t *spi, const uint8_t *src, size_t len);

/* Test-only capture helpers (implemented in hardware_mocks.c): every
 * spi_write_blocking call is recorded so tests can assert on transaction
 * batching, not just buffer contents. */
#define MOCK_SPI_MAX_CALLS     32U
#define MOCK_SPI_CAPTURE_BYTES 24U

typedef struct {
    size_t len;
    uint8_t data[MOCK_SPI_CAPTURE_BYTES];
} mock_spi_call_t;

void mock_spi_reset(void);
size_t mock_spi_call_count(void);
const mock_spi_call_t *mock_spi_call(size_t index);
