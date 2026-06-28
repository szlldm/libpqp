/* Copyright (C) 2026 szlldm
 * 
 * Raw interface.
 * This file is part of libpqp.
 * 
 * LibPQP is dual-licensed: you may use it under the terms of the
 * GNU Affero General Public License version 3 (AGPLv3), or alternatively
 * under a commercial license.
 * You should have received a copy of the AGPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 * For commercial licensing, please contact.
 */

#include "libpqp_if_raw.h"
#include <stddef.h>
#include <string.h>	//memcpy

// private


static volatile bool initialized = false;
static volatile bool rx_buffer_occupied = false;
static volatile bool tx_buffer_occupied = false;
static volatile int rx_buffer_len;
static volatile int tx_buffer_len;
static volatile uint8_t rx_buffer[RAW_PACKET_MAX_SIZE];
static volatile uint8_t tx_buffer[RAW_PACKET_MAX_SIZE];
static bool rx_buffer_verified = false;
static pqp_prio_t rx_priority;
static uint32_t stat_tx_cntr = 0, stat_rx_cntr = 0, stat_valid_rx_cntr = 0;

static void deinit( void * if_private )
{
	initialized = false;
}

static void process( void * if_private )
{
	if ( !rx_buffer_verified )
	{
		if ( rx_buffer_occupied )
		{
			do {
				rx_priority = ( ( rx_buffer[0] >> 6 ) & 0x03 );
				if ( ( rx_buffer[0] & 0x3F ) != 0x3E ) break;			// check 111110 bits
				if ( ( ( rx_buffer[1] >> 4 ) & 0x03 ) != 2 ) break;		// check HLEN == 2
				if ( rx_buffer_len < RAW_PACKET_HEADER_SIZE ) break;
				if ( rx_buffer_len > RAW_PACKET_MAX_SIZE ) break;
				
				rx_buffer_verified = true;
			} while ( 0 );
			
			if ( !rx_buffer_verified )
			{
				rx_buffer_occupied = false;
			}
		}
	}
}

static bool rx_available( void * if_private, pqp_prio_t priority )
{
	if ( !rx_buffer_verified )
	{
		return false;
	}
	return ( rx_priority == priority );
}

static bool rx_get_packet( void * if_private, pqp_prio_t priority, pqp_packet_t * packet )
{
	if ( !rx_buffer_verified )
	{
		return false;
	}
	if ( rx_priority != priority )
	{
		return false;
	}
	if ( ( rx_buffer_len < RAW_PACKET_HEADER_SIZE ) || ( rx_buffer_len > RAW_PACKET_MAX_SIZE ) )
	{
		rx_buffer_occupied = false;
		rx_buffer_verified = false;
		return false;
	}

	
	packet->priority = rx_priority;
	packet->flags = ( ( rx_buffer[1] >> 6 ) & 0x03 );
	packet->ttl = ( ( rx_buffer[1] >> 0 ) & 0x0F );
	packet->src_addr = rx_buffer[2];
	packet->dst_addr = rx_buffer[3];
	packet->src_port = rx_buffer[4];
	packet->dst_port = rx_buffer[5];
	pqp_volatile_copy( &( packet->crc32c ), rx_buffer + 6, 4 );
	packet->payload_length = rx_buffer_len - RAW_PACKET_HEADER_SIZE;
	if ( packet->payload_length > 0 )
	{
		pqp_volatile_copy( packet->payload, rx_buffer + RAW_PACKET_HEADER_SIZE, packet->payload_length );
	}
	
	rx_buffer_occupied = false;
	rx_buffer_verified = false;
	
	if ( packet->crc32c == pqp_calculate_crc32c( packet ) )
	{
		stat_valid_rx_cntr += 1;
		return true;
	}
	
	return false;
}

static bool tx_enqueue_packet( void * if_private, pqp_packet_t * packet )
{
	if ( tx_buffer_occupied )
	{
		return false;
	}
	
	tx_buffer_len = RAW_PACKET_HEADER_SIZE + packet->payload_length;
	if ( tx_buffer_len <= RAW_PACKET_MAX_SIZE )
	{
		uint8_t prio = packet->priority;
		tx_buffer[0] = ( ( prio << 6 ) | 0x3E );
		tx_buffer[1] = ( ( ( packet->flags & 0x03 ) << 6 ) | ( 0x20 ) | ( ( packet->ttl & 0x0F) << 0 ) );
		tx_buffer[2] = packet->src_addr;
		tx_buffer[3] = packet->dst_addr;
		tx_buffer[4] = packet->src_port;
		tx_buffer[5] = packet->dst_port;
		pqp_volatile_copy( tx_buffer + 6, &( packet->crc32c ), 4 );
		pqp_volatile_copy( tx_buffer + RAW_PACKET_HEADER_SIZE, packet->payload, packet->payload_length );
		tx_buffer_occupied = true;
	}
	
	packet->packet_status = FREE;
	return true;
}

static uint8_t get_info( void * if_private, uint8_t * data )	// data can be NULL, in this case the same length must be returned
{
	// not used: if ( if_private == NULL ) return 0;
	uint8_t len = 0;
	if ( data != NULL )
	{
		memcpy( data + len, "RAW", 3 );
	}
	len += 3;
	
	if ( data != NULL )
	{
		memcpy( data + len, &stat_tx_cntr, 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &stat_rx_cntr, 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &stat_valid_rx_cntr, 4 );
	}
	len += 4;
	
	return len;
}


// public

void PQP_IF_OBJ_INITIALIZER_RAW1( pqp_interface_t * pqp_interface )
{
	pqp_interface->if_private = NULL;
	pqp_interface->if_deinit = deinit;
	pqp_interface->if_process = process;
	pqp_interface->if_rx_available = rx_available;
	pqp_interface->if_rx_get_packet = rx_get_packet;
	pqp_interface->if_tx_enqueue_packet = tx_enqueue_packet;
	pqp_interface->if_info = get_info;
	
	initialized = true;
	rx_buffer_verified = false;
	rx_buffer_occupied = false;
	tx_buffer_occupied = false;
	stat_tx_cntr = 0;
	stat_rx_cntr = 0;
	stat_valid_rx_cntr = 0;
};


int pqp_if_raw_get_packet( uint8_t * data )			// returns the number of bytes of the raw packet, or -1 on error / no packet
{
	if ( !initialized ) return -2;
	if ( !tx_buffer_occupied ) return -3;
	
	int len = tx_buffer_len;
	if ( len <= 0) len = -1;
	if ( len > RAW_PACKET_MAX_SIZE ) len = -1;
	if ( len > 0 )
	{
		pqp_volatile_copy( data, tx_buffer, len );
	}
	tx_buffer_occupied = false;
	stat_tx_cntr += 1;
	
	return len;
}

int pqp_if_raw_add_packet( uint8_t * data, int data_len )	// returns 0 if raw packet taken, or -1 if cannot take at the moment (try later)
{
	if ( !initialized ) return -1;
	if ( rx_buffer_occupied ) return -2;
	
	if ( data_len < RAW_PACKET_HEADER_SIZE ) return 0;			// invalid lengths, indicate as taken
	if ( data_len > RAW_PACKET_MAX_SIZE ) return 0;
	
	pqp_volatile_copy( rx_buffer, data, data_len );
	rx_buffer_len = data_len;
	rx_buffer_occupied = true;
	stat_rx_cntr += 1;
	
	return 0;
}

