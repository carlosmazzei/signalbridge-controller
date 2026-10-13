#ifndef MOCK_UNIQUE_ID_H
#define MOCK_UNIQUE_ID_H
#include <stddef.h>
#define PICO_UNIQUE_BOARD_ID_SIZE_BYTES 8U
void pico_get_unique_board_id_string(char *id, size_t len);
#endif
