#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct spi_inst_t spi_inst_t;
extern spi_inst_t *spi0;

void spi_init(spi_inst_t *spi, unsigned int baudrate);
void spi_set_format(spi_inst_t *spi, unsigned int data_bits, unsigned int cpol, unsigned int cpha, bool lsb_first);
int spi_write_blocking(spi_inst_t *spi, const uint8_t *src, size_t len);

/* Test-only capture helpers for spi_write_blocking, implemented in
 * hardware_mocks.c. The record layout is shared with the mock through a
 * dedicated header so the two can never drift apart. */
#include "mock_spi_capture.h"
