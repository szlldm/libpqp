/* Copyright (C) 2026 szlldm
 * 
 * LibPQP foreign functions
 * This file is part of libpqp.
 * 
 * LibPQP is dual-licensed: you may use it under the terms of the
 * GNU Affero General Public License version 3 (AGPLv3), or alternatively
 * under a commercial license.
 * You should have received a copy of the AGPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 * For commercial licensing, please contact.
 */

#ifndef LIBPQP_FOREIGN_H
#define LIBPQP_FOREIGN_H

#include <stdint.h>
#include <stdbool.h>
#include "../pqp_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

void 		pqpf_reboot( uint32_t latch_bootloader );					// if technically possible, the device should store bootloader latch request on soft reboot
uint32_t	pqpf_bootloader_latch_reboot( void );						// if cannot be implemented, this should always return 0

bool		pqpf_check_valid_firmware( void );						// returns true if there is valid marker and the calculated checksum matches the stored (@ the last 4 bytes of the FLASH memory) checksum
void 		pqpf_start_firmware( void );							// if success, it should not return

uint32_t 	pqpf_get_routing_table_checksum( void );
int 		pqpf_clear_routing_table( void );						// sets the routing table to 0x00 or 0xFF; returns -1 on failure, else 0
int 		pqpf_write_routing_table( uint8_t src_addr, uint8_t dst_addr, uint8_t data );	// returns -1 on failure, else 0
uint8_t 	pqpf_read_routing_table( uint8_t src_addr, uint8_t dst_addr );

int 		pqpf_firmware_erase( void );							// returns -1 on failure, else 0
int 		pqpf_erase_all( void );								// returns -1 on failure, else 0
int 		pqpf_firmware_erase_section( uint32_t address, uint32_t length );		// returns -1 on failure, else 0
int 		pqpf_firmware_read( uint8_t * dst_buf, uint32_t address, int size );		// returns -1 on failure, else returns the number of bytes read
int 		pqpf_firmware_write( uint8_t * src_buf, uint32_t address, int size );		// returns -1 on failure, else returns the number of bytes written
												// address must be PAGE (256) aligned  !!
												// size must be multiple of PAGE (256) !!
int 		pqpf_clone_local_firmware( uint16_t block_count, uint32_t write_address_start, uint32_t read_address_start );	// returns -1 on failure, else 0
uint16_t 	pqpf_check_valid_clone_firmware( uint32_t address_start );			// returns 0 if no valid cone firmware, else returns the block count

uint32_t 	pqpf_get_bootloader_checksum( void );
uint32_t 	pqpf_get_firmware_checksum( void );
uint32_t 	pqpf_get_firmware_stored_checksum( void );
uint32_t 	pqpf_get_firmware_length( void );
uint32_t 	pqpf_get_section_checksum( uint32_t address, uint32_t length );			// returns 0xFFFFFFFF on failure, else the checksum
bool 		pqpf_get_section_erased( uint32_t address, uint32_t length );			// returns true if section is earesed, or false if it was already written

uint32_t 	pgpf_get_ms_timestamp( void );
uint32_t 	pgpf_get_uptime( void );

void 		pqpf_multicore_init_lock( void );						// on single core devices these could be empty functions
void 		pqpf_multicore_lock( void );
void 		pqpf_multicore_unlock( void );

uint8_t		pqpf_hw_info_length( void );
void		pqpf_copy_hw_info( uint8_t * data );

#ifdef LIBPQP_HAS_STDIOIF
void 		pqpf_stdio_if_init( void );							// should contain some initialization code, if needed
int		pqpf_stdio_if_getchar( void );							// returns received char, or -1 if no data available
void		pqpf_stdio_if_sendchar( uint8_t data );						// start a single char send
void		pqpf_stdio_if_flush( void );							// flush send buffer
#endif

#ifdef LIBPQP_HAS_CAN1
void 		pqpf_can_if1_init( void );							// should contain some initialization code, if needed
void 		pqpf_can_if1_deinit( void );							// stop and deinit the interface
void		pqpf_can_if1_clear_tx( void );							// clear transmitter queue
int		pqpf_can_if1_enqueue_fragment( uint32_t eid, uint8_t * data, int data_len);	// returns -1 on failure; copy fragment into the transmitter queue; eid lower 29 bits
int		pqpf_can_if1_received_fragment( uint32_t * eid, uint8_t * data );		// returns data length of the received fragment
#endif

#ifdef LIBPQP_HAS_HDUPLEX_UART1
void 		pqpf_hduart_if1_init( void );							// should contain some initialization code, if needed
void 		pqpf_hduart_if1_deinit( void );							// stop and deinit the interface
bool 		pqpf_hduart_if1_is_busy( void );							// return true, if there are already trafic on the HDUART, or last RX happened in less than 1ms
void 		pqpf_hduart_if1_send_data( uint8_t * data, int len );				// starts a HDUART transmit procedure (probably interrupt driven),
												// if there are already trafic on the HDUART, it does not starts
												// if HDUART echo is not what it transmits, it stops
int 		pqpf_hduart_if1_send_status( void );						// returns -1 on failure (HDUART busy or HDUART echo failed), 0 on busy, 1 on success
int 		pqpf_hduart_if1_receive_data( void );						// get received bytes (not from own transmission), or returns -1 if no data available
#endif

#ifdef LIBPQP_HAS_I2C1
void 		pqpf_i2c_if1_init( void );							// should contain some initialization code, if needed
void 		pqpf_i2c_if1_hw_init( void );
void 		pqpf_i2c_if1_deinit( void );
bool 		pqpf_i2c_if1_is_busy( void );							// return true, if there are already trafic on the HDUART, or last RX happened in less than 1ms
void 		pqpf_i2c_if1_send_data(uint8_t addr, uint8_t * dst, int len);

int  		pqpf_i2c_if1_send_status(void);
int  		pqpf_i2c_if1_receive_data(void);
uint8_t  	pqpf_i2c_if1_getModeState(void);
int 		pqpf_i2c_if1_get_write_state(void);
#endif

#ifdef LIBPQP_HAS_CAN2
void 		pqpf_can_if2_init( void );
void 		pqpf_can_if2_deinit( void );
void		pqpf_can_if2_clear_tx( void );
int		pqpf_can_if2_enqueue_fragment( uint32_t eid, uint8_t * data, int data_len);
int		pqpf_can_if2_received_fragment( uint32_t * eid, uint8_t * data );
#endif

#ifdef LIBPQP_HAS_HDUPLEX_UART2
void 		pqpf_hduart_if2_init( void );
void 		pqpf_hduart_if2_deinit( void );
bool 		pqpf_hduart_if2_is_busy( void );
void 		pqpf_hduart_if2_send_data( uint8_t * data, int len );
int 		pqpf_hduart_if2_send_status( void );
void 		pqpf_hduart_if2_receive( uint8_t data );
int 		pqpf_hduart_if2_receive_data( void );
#endif

#ifdef LIBPQP_HAS_I2C2
void 		pqpf_i2c_if2_init( void );							// should contain some initialization code, if needed
void 		pqpf_i2c_if2_hw_init( void );
void 		pqpf_i2c_if2_deinit( void );
bool 		pqpf_i2c_if2_is_busy( void );							// return true, if there are already trafic on the I2C, or last RX happened in less than 1ms
void 		pqpf_i2c_if2_send_data(uint8_t addr, uint8_t * dst, int len);

int  		pqpf_i2c_if2_send_status(void);
int  		pqpf_i2c_if2_receive_data(void);
uint8_t  	pqpf_i2c_if2_getModeState(void);
int 		pqpf_i2c_if2_get_write_state(void);
#endif


#ifdef __cplusplus
}
#endif

#endif //LIBPQP_FOREIGN_H
