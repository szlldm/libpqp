/* Copyright (C) 2026 szlldm
 * 
 * LibPQP private header.
 * This file is part of libpqp.
 * 
 * LibPQP is dual-licensed: you may use it under the terms of the
 * GNU Affero General Public License version 3 (AGPLv3), or alternatively
 * under a commercial license.
 * You should have received a copy of the AGPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 * For commercial licensing, please contact.
 */

// LibPQP internal stuff
//

#ifndef LIBPQP_PRIVATE_H
#define LIBPQP_PRIVATE_H

#include "libpqp.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef enum{
	FREE = 0,
	RECEIVED,
	USERSPACE,
	TO_BE_SENT,
	INTERFACE,
} pqp_packet_status_t;

typedef enum{
	MGMT_PORT_BEGIN = 16,
	MGMT_PORT_PING = 16,
	MGMT_PORT_TRANSFER_TEST,
	MGMT_PORT_REBOOT,
	MGMT_PORT_BOOTLOADER,
	MGMT_PORT_CHECKSUM,
	MGMT_PORT_RUN,
	MGMT_PORT_ERASE,
	MGMT_PORT_CLEAR_MAP,
	MGMT_PORT_GET_MAP,
	MGMT_PORT_FW_WRITE,
	MGMT_PORT_FW_CLONE,
	MGMT_PORT_FW_CLONE_CTRL,
	MGMT_PORT_CLEAR_ROUTING_TABLE,
	MGMT_PORT_ROUTING_TABLE_ENTRY,
	MGMT_PORT_EXTENSIONS,
	MGMT_PORT_TTY,
	MGMT_PORT_END = MGMT_PORT_TTY
} pqp_management_ports;

typedef enum{
	CUE_CHECKSUM_FW = 0,
	CUE_CHECKSUM_SECTION,
	CUE_ERASED_SECTION,
	CUE_CHECKSUM_ROUTING_TABLE
} pqp_management_port_checksum_cues;

typedef enum{
	CUE_ERASE_FIRMWARE = 0xAA,
	CUE_ERASE_FW_ALL = 0x55,
	CUE_ERASE_SECTION = 0x69
} pqp_management_port_erase_cues;

#define PQP_REBOOT_MAGIC		("ReB0")
#define PQP_BOOTLOADER_LATCH_MAGIC	("LaTC")
#define PQP_BOOTLOADER_FWUPD_MAGIC	("BlFg")
#define PQP_ERASE_MAGIC			("Er4S")
#define PQP_ERASE_ALL_MAGIC		("!EaL")
#define PQP_ERASE_SECTION_MAGIC		("eRSC")
#define PQP_WRITE_MAGIC			("wRT3")
#define PQP_WRITE_MAGIC_NOREPLY		("NyWr")
#define PQP_CLEAR_ROUTING_TABLE_MAGIC	("ClRt")

#define PQP_FW_CLONE_FLAG_SKIP_FF_BLOCK		0x01

typedef struct
{
	pqp_prio_t 		priority;
	uint8_t 		flags;
	uint8_t 		ttl;
	
	uint8_t 		src_addr;
	uint8_t 		dst_addr;
	uint8_t 		src_port;
	uint8_t 		dst_port;
	
	uint8_t 		payload[PQP_MAX_PAYLOAD];
	
	uint32_t 		crc32c;

	uint32_t 		payload_length;
	uint32_t		timestamp;
	uint8_t 		interface;
	pqp_packet_status_t 	packet_status;
} pqp_packet_t;

typedef struct pqp_interface_t_
{
	void * 			if_private;
	void 			( * if_deinit)( void * if_private );
	void 			( * if_process)( void * if_private );
	bool 			( * if_rx_available )( void * if_private, pqp_prio_t priority );
	bool 			( * if_rx_get_packet )( void * if_private, pqp_prio_t priority, pqp_packet_t * packet );
	bool 			( * if_tx_enqueue_packet )( void * if_private, pqp_packet_t * packet );
	uint8_t			( * if_info)( void * if_private, uint8_t * data );
} pqp_interface_t;

typedef void ( * pqp_interface_initializer_t )( pqp_interface_t * pqp_interface );

void pqp_crc32_init( uint32_t * crc );
void pqp_crc32_update( uint32_t * crc, const uint8_t data );
uint32_t pqp_crc32_final( uint32_t * crc );
uint32_t pqp_calculate_crc32c( pqp_packet_t * packet );

bool pqp_destination_irrelevant( uint8_t src_addr, uint8_t dst_addr );

void pqp_volatile_copy( volatile void * destination, const volatile void * source, size_t num );
bool pqp_volatile_compare( volatile void * destination, const volatile void * source, size_t num );

#ifdef LIBPQP_TWEAK_TX_PACKET_INJECTION
void pqp_send_packet_injected( int pid, uint8_t dst_addr, uint8_t dst_port, uint8_t src_addr, uint8_t src_port, pqp_prio_t priority, uint8_t flags, uint8_t ttl, uint8_t interface );
#endif


#ifdef __cplusplus
}
#endif

#endif // LIBPQP_PRIVATE_H
