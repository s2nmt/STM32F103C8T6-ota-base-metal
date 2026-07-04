/*
 * msp.c — default weak MSP hooks; override in board-specific translation unit.
 */

#include <msp.h>

__attribute__((weak)) void msp_init(void)
{
}
