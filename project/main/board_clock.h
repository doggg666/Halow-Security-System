#pragma once

#include "driver/i2c_master.h"

void board_clock_attach(i2c_master_bus_handle_t bus);
void board_clock_start(void);
