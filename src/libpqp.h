/* Copyright (C) 2026 szlldm
 * 
 * LibPQP header.
 * This file is part of libpqp.
 * 
 * LibPQP is dual-licensed: you may use it under the terms of the
 * GNU Affero General Public License version 3 (AGPLv3), or alternatively
 * under a commercial license.
 * You should have received a copy of the AGPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 * For commercial licensing, please contact.
 */

//
// LIBPQP_IS_BOOTLOADER -=OR=- LIBPQP_IS_FIRMWARE must be defined globally or in pqp_defines.h
//

#ifndef LIBPQP_H
#define LIBPQP_H

#include "../pqp_defines.h"

#if defined(LIBPQP_IS_BOOTLOADER) + defined(LIBPQP_IS_FIRMWARE) != 1
#error LIBPQP_IS_BOOTLOADER or LIBPQP_IS_FIRMWARE not defined!
#endif


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define LIBPQP_BROADCAST_ADDRESS		(255)
#define PQP_MAX_PAYLOAD				(264)		// firmware block must fit...
#ifndef PQP_NUMBER_OF_TX_BUFFERS
#define PQP_NUMBER_OF_TX_BUFFERS		(8)
#endif
#ifndef PQP_NUMBER_OF_RX_BUFFERS
#define PQP_NUMBER_OF_RX_BUFFERS		(16)
#endif
#ifndef PQP_SEND_TTL
#define PQP_SEND_TTL				(2)
#endif

#define PQP_PACKET_DROP_AFTER_MS		(10000)		// 10sec

#ifdef __cplusplus
// when included in C++ file, let compiler know these are C functions
extern "C" {
#endif


typedef enum {
	PQP_PRIO_CRITICAL = 0,
	PQP_PRIO_HIGH,
	PQP_PRIO_NORMAL,
	PQP_PRIO_LOW,
	
	PQP_PRIO_LEVELS
} pqp_prio_t;

void 		pqp_init( uint8_t my_address );			// must be called once on system startup 
void		pqp_process( void );				// must be called periodically
uint8_t 	pqp_get_my_address( void );

// You can choose the zero copy interface (advanced), or the simply interface

// Zero copy interface
bool 		pqp_received_packet_available( void );
int 		pqp_take_received_packet( void );		// return packet-id (must be call pqp_release_received_packet() at the end !! ), else returns -1 if no packet available
uint8_t 	pqp_get_dst_addr( int pid );
uint8_t 	pqp_get_src_addr( int pid );
uint8_t 	pqp_get_dst_port( int pid );
uint8_t 	pqp_get_src_port( int pid );
pqp_prio_t 	pqp_get_priority( int pid );
uint8_t 	pqp_get_flags( int pid );
int 		pqp_get_payload_length( int pid );
void 		pqp_release_received_packet( int pid );
int 		pqp_request_blank_packet( void );		// return packet-id (must be call pqp_send_packet() at the end !! ), else returns -1 if no free buffer available
void 		pqp_set_flags( int pid, uint8_t flags );
void 		pqp_set_payload_length( int pid, int len );
void 		pqp_send_packet( int pid, uint8_t dst_addr, uint8_t dst_port, uint8_t src_port, pqp_prio_t priority );
bool 		pqp_packet_sent( int pid );
uint8_t *	pqp_get_packet_payload_pointer( int pid );	// get the payload pointer, where data can be written (if will be sent), or can be read (if received), if pid invalid, returns NULL

// Simple interface
int 		pqp_recv( uint8_t * data, uint8_t * dst_addr , uint8_t * src_addr, uint8_t * dst_port, uint8_t * src_port, pqp_prio_t * priority );	// returns the number of bytes copied to data, or -1 on error / no packet
																			// copy address, ports and priority to the variables
int 		pqp_send( uint8_t * data, int data_len, uint8_t dst_addr, uint8_t dst_port, uint8_t src_port, pqp_prio_t priority );			// returns data_len, or -1 on error (eg. no available buffer in PQP at the moment)


// OBC only
void 		pqp_set_outgoing_interface( uint8_t dst_addr, int interface );		// interface = -1 means default

// COM only (raw packet max size: 10+PQP_MAX_PAYLOAD)
int 		pqp_raw_pop( uint8_t * data );			// returns the number of bytes of the raw packet, or -1 on error / no packet
								// gets the raw packet, that has to be transmitted to GND
int 		pqp_raw_push( uint8_t * data, int data_len );	// returns 0 if raw packet taken, or -1 if cannot take at the moment (try later)
								// take the raw packet, that has been received from GND

// Bootloader interface
#define PQP_BLDR_CODE_BOOTLOADER_LATCH		(0xB1AA55AA)
#define PQP_BLDR_CODE_FIRMWARE_UPDATE		(0xFF696969)
uint32_t 	pqp_bootloader_latch_code( void );			// returns 0 if no latched code

#ifdef __cplusplus
}
#endif
#endif //LIBPQP_H
