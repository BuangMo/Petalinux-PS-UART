#ifndef _RS422_TEST_H_
#define _RS422_TEST_H_

#include <stdint.h>

// definition of UART registers
#define CONTROL_REG         0x00
#define MODE_REG            0x04
#define INTRPT_DIS_REG      0x0C
#define BAUD_RATE_GEN_REG   0x18
#define RCVR_TIMEOUT_REG    0x1C
#define RCVR_FIFO_TRIG_REG  0x20
#define CHANNEL_STS_REG     0x2C
#define TX_RX_FIFO_REG      0x30
#define BAUD_RATE_DIV_REG   0x34

// function prototypes
void usage( char *prog );
void uart_init();
uint16_t mode_setup( uint16_t mode_sel );
void poll_rcvr_fifo();
uint16_t data_transfer( uint16_t cmd_sel );

// global variables
static char *uart_base_reg;
typedef enum{ false, true } boolean;
enum op_mode{ NRML_MODE, AUTOECHO_MODE, LPBCK_MODE, RM_LPBCK_MODE };

#endif 