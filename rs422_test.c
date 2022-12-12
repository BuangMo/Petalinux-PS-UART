#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include "rs422_test.h"

boolean set_flag = false;

int main( int argc, char *argv[] ){
    int mem_fd;
	unsigned write_val;
    static unsigned uart_addr = 0xFF010000;
    unsigned page_addr;
	unsigned page_sizee = sysconf( _SC_PAGESIZE );  // gets the page size in bytes
    unsigned page_offset;

    // maps physical memory into user space for read and write access
	mem_fd = open( "/dev/mem", O_RDWR );
	if( mem_fd < 1 ){
		perror( argv[0] );
		exit( -1 );
	}

    page_addr = ( uart_addr & ~( page_sizee-1 ));
	page_offset = uart_addr-page_addr;

    // memory map the physical address of the hardware into virtual address space
	uart_base_reg = mmap( NULL, page_sizee, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, ( uart_addr & ~( page_sizee-1 )));
	if( uart_base_reg == MAP_FAILED ){
		printf( "Failed to map physical address into virtual address space.\n" );
		exit( -1 );
	}

    // verifies the number of arguments given
	if( argc != 3 ){
		usage( argv[ 0 ]);
		exit( -1 );
	}
		
	// gets the value of the hex value of the command to be written to the register
	write_val = strtoul( argv[ 1 ], NULL, 0 );
	if(( write_val < 0 ) || ( write_val > 0xFF )){
		printf( "The value=%d specified is out of range.\n", write_val );
		printf( "The hex value has to be between 0x00 and 0xFF.\n" );
		exit( -1 );
	}

    if( set_flag == false )
        uart_init(); // sets up the uart 

    if( mode_setup( atoi( argv[ 2 ])) != 0 ) // sets up the uart mode of operation
        exit( -1 );

    if( data_transfer( atoi( argv[ 1 ])) != 0 )    // sends the data to the TX FIFO
        exit( -1 );

    if(( atoi( argv[ 2 ]) == LPBCK_MODE) || ( atoi( argv[ 2 ]) == RM_LPBCK_MODE))
        poll_rcvr_fifo();

    printf( "Successfully ran the RS-422 loopback test.\n" );

    return 0;
}

/*
 * func: uart_setup()
 * desc: sets up the registers for uart operation
 */
void uart_init(){
    // enables the transmitter and receiver blocks
    *(( volatile unsigned * )( uart_base_reg+CONTROL_REG )) = 0x114;

    // setup the baud rate to 115200 Baud
    // formula: BAUD_RATE = (clk_sel/(clock_divisor*(baud_rate_divider+1)))
    *(( volatile unsigned * )( uart_base_reg+BAUD_RATE_GEN_REG )) = 0x0000007C;
    *(( volatile unsigned * )( uart_base_reg+BAUD_RATE_DIV_REG )) = 0x00000006;

    // set the RX FIFO trigger level to 8 bytes
    *(( volatile unsigned * )( uart_base_reg+RCVR_FIFO_TRIG_REG )) = 0x00000008;

    // setup RX timeout period
    // formula: Rcvr_timeout = RTO*(clk_sel/clock_divisor)
    *(( volatile unsigned * )( uart_base_reg+RCVR_TIMEOUT_REG )) = 0x00000001;

    // disable all interrupts
    *(( volatile unsigned * )( uart_base_reg+INTRPT_DIS_REG )) = 0x00001FFF;
    
    set_flag = true;
}

/*
 * func: usage( char *prog ) 
 * desc: prints the help information to the monitor
 */
void usage( char *prog ){
    printf( "usage: %s VAL\n", prog );
	printf( "\n" );
	printf( "VAL is the command to execute and there are 3.\n" );
}

/*
 * func: uint16_t mode_setup( uint16_t mode_sel )
 * desc: sets up the mode of operation of the uart
 */
uint16_t mode_setup( uint16_t mode_sel ){
    switch( mode_sel ){
        case NRML_MODE:
            *(( volatile unsigned * )( uart_base_reg+MODE_REG )) = 0x00000020;
            break;
        case AUTOECHO_MODE:
            *(( volatile unsigned * )( uart_base_reg+MODE_REG )) = 0x00000120;
            break;
        case LPBCK_MODE:
            *(( volatile unsigned * )( uart_base_reg+MODE_REG )) = 0x00000220;
            break;
        case RM_LPBCK_MODE:
            *(( volatile unsigned * )( uart_base_reg+MODE_REG )) = 0x00000320;
            break;
        default:
            printf( "The value of %d is not a recognised mode. Mode takes a value between 0 and 3./n", mode_sel );
            printf( "0 - Normal Mode.\n" );
            printf( "1 - Automatic Echo Mode.\n" );
            printf( "2 - Loopback mode.\n" );
            printf( "3 - Remote Loopback Mode.\n" );
            return 1;
    }

    return 0;
}

/*
 * func: uint16_t data_transfer( uint16_t cmd_sel )
 * desc: sends the data to the TX FIFO
 */
uint16_t data_transfer( uint16_t cmd_sel ){
    uint32_t cmds[3][8] = {
		{ 0x48, 0x45, 0x4C, 0x4C, 0x4F, 0x2E, 0x2E, 0x2E },
		{ 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F },
		{ 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28 }
	};

    // verifies that the right command has been selected
    if(( cmd_sel < 0 ) || ( cmd_sel > 0x2 )){
		printf( "The value=%d specified for the command is out of range.\n", cmd_sel );
		printf( "The value has to be between 0x00 and 0x02.\n" );
		return 1;
	}

    // sends the command data to the TX FIFO
    for( int i = 0; i<8; i++ ){
        *(( volatile unsigned * )( uart_base_reg+TX_RX_FIFO_REG )) = cmds[ cmd_sel ][ i ];
    }

    return 0;
}

/*
 * func: void poll_rcvr_fifo()
 * desc: polls the receiver fifo for data by looking for the RX FIFO full bit
 */
void poll_rcvr_fifo(){
    uint32_t chnl_sts_reg_value;

    while( true ){
        chnl_sts_reg_value = *((unsigned *)( uart_base_reg+CHANNEL_STS_REG ));

        // breaks out if TX FIFO is empty( all data has bben sent ) and RX FIFO has bytes equal in number to RTRIG
        if( chnl_sts_reg_value == 0x00000009 )
            break;
    }

    printf( "Data that has been looped back:\n\t[ " );
   
    for( int i=0; i<7; i++ ) 
		  printf( "0x%08X, ", *((unsigned *)( uart_base_reg+TX_RX_FIFO_REG )));
        
    printf( "0x%08X ]\n ", *((unsigned *)( uart_base_reg+TX_RX_FIFO_REG )));
}