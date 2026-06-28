/* Copyright (C) 2026 szlldm
 * 
 * Half duplex UART interface.
 * This file is part of libpqp.
 * 
 * LibPQP is dual-licensed: you may use it under the terms of the
 * GNU Affero General Public License version 3 (AGPLv3), or alternatively
 * under a commercial license.
 * You should have received a copy of the AGPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 * For commercial licensing, please contact.
 */

#include "libpqp_if_hduart.h"
#include <stddef.h>
#include <string.h>

// private


#define HEADER_SIZE			( 6 )
#define CSP_HEADER_SIZE			( 5 )
#define CRC32_SIZE			( 4 )
#define SLIP_PACKET_MAX_SIZE		( ( ( HEADER_SIZE + PQP_MAX_PAYLOAD + CRC32_SIZE) * 2 )  + 2 )		// slip encoded worst case + 2x Frame End (at the begin and at the end)

#define MAX_TX_RETY_COUNT		( 3 )

#define FRAME_END			( 0xC0 )
#define FRAME_ESCAPE			( 0xDB )
#define ESCAPED_END			( 0xDC )
#define ESCAPED_ESCAPE			( 0xDD )

//#define CSP_HEADER_FLAGS		( 0x01 )	// correct CSP
#define CSP_HEADER_FLAGS		( 0x00 )	// incorrect CSP-LIGHT compatible

#ifndef LIBPQP_LEGACY_CSP_DEFAULT_TTL
	#define LIBPQP_LEGACY_CSP_DEFAULT_TTL	(4)
#endif

typedef enum {
	NOP = 0,
	WAIT_FOR_LINE,
	UNDER_SEND
} tx_status_t;

typedef struct
{
	void				( * bus_deinit )( void );
	bool				( * bus_rx_busy )( void );
	void				( * bus_send_data )( uint8_t * data, int data_len );
	int				( * bus_send_status )( void );
	int				( * bus_receive_data )( void );
	
	uint8_t				tx_slip_buffer[SLIP_PACKET_MAX_SIZE];
	int				tx_slip_length;
	unsigned int			tx_attempt_cntr;
	tx_status_t			tx_status;
	uint32_t			next_tx_attempt_time;
	
	uint8_t				rx_double_buffer[2][HEADER_SIZE + PQP_MAX_PAYLOAD + CRC32_SIZE];
	int				current_rx_buffer;
	int				rx_length;
	bool				rx_slip_escaped;
	bool				csp_packet;
	int				received_packet_length;
	bool				received_csp_packet;
	pqp_prio_t			rx_priority;
	uint32_t			last_rx_time;
	
	uint32_t 			stat_tx_cntr, stat_tx_collision_cntr, stat_rx_valid_cntr, stat_rx_crc_error_cntr, stat_rx_error_cntr;
} hdu_t;

static inline uint32_t bswap32( uint32_t x )
{
	return ( ( ( ( x ) & 0xFF000000U ) >> 24 )  |  ( ( ( x ) & 0x00FF0000U ) >> 8 )  |  ( ( ( x ) & 0x0000FF00U ) << 8 )  |  ( ( ( x ) & 0x000000FFU ) << 24 ) );
}

static void init( hdu_t * hdu )
{
	hdu->tx_slip_length = 0;
	hdu->tx_status = NOP;
	hdu->current_rx_buffer = 0;
	hdu->rx_length = -1;
	hdu->rx_slip_escaped = false;
	hdu->csp_packet = false;
	hdu->received_packet_length = 0;
	hdu->received_csp_packet = false;
	hdu->last_rx_time = 0;
	hdu->stat_tx_cntr = 0;
	hdu->stat_tx_collision_cntr = 0;
	hdu->stat_rx_valid_cntr = 0;
	hdu->stat_rx_crc_error_cntr = 0;
	hdu->stat_rx_error_cntr = 0;
}

static void deinit( void * if_private )
{
	hdu_t * hdu = (hdu_t*)if_private;
	hdu->bus_deinit( );
	hdu->received_packet_length = 0;
}

static void process( void * if_private )
{
	hdu_t * hdu = (hdu_t*)if_private;
	uint32_t now = pgpf_get_ms_timestamp( );
	
	hdu->current_rx_buffer = ( hdu->current_rx_buffer % 2 );
	uint8_t * rx_buf = hdu->rx_double_buffer[hdu->current_rx_buffer];
	
	int break_cntr = 0;
	int rx_byte;
	while ( ( rx_byte = hdu->bus_receive_data() ) >= 0 )
	{
		if ( break_cntr == 0 )
		{
			hdu->last_rx_time = now;
		}
		
		if ( rx_byte == FRAME_END )
		{
			if ( hdu->rx_length >= ( HEADER_SIZE + CRC32_SIZE ) )
			{
				if ( !hdu->rx_slip_escaped )
				{
					// TODO ? CRC check here ?
					hdu->received_packet_length = hdu->rx_length;
					hdu->received_csp_packet = hdu->csp_packet;
					if ( hdu->csp_packet )
					{
						hdu->rx_priority = ( ( rx_buf[2] >> 6 ) & 0x03 );
					}
					else
					{
						hdu->rx_priority = ( ( rx_buf[0] >> 6 ) & 0x03 );
					}
					hdu->current_rx_buffer = ( ( hdu->current_rx_buffer + 1 ) % 2 );
				}
				else
				{
					hdu->stat_rx_error_cntr += 1;
				}
			}
			hdu->rx_length = 0;
			hdu->rx_slip_escaped = false;
		} else
		if ( hdu->rx_length >= 0 )
		{
			if ( hdu->rx_length >= ( HEADER_SIZE + PQP_MAX_PAYLOAD + CRC32_SIZE ) )
			{
				hdu->rx_length = -1;
				hdu->stat_rx_error_cntr += 1;
			}
			else
			{
				if ( hdu->rx_slip_escaped )
				{
					hdu->rx_slip_escaped = false;
					if ( rx_byte == ESCAPED_END )
					{
						rx_buf[hdu->rx_length] = FRAME_END;
						hdu->rx_length += 1;
					} else
					if ( rx_byte == ESCAPED_ESCAPE )
					{
						rx_buf[hdu->rx_length] = FRAME_ESCAPE;
						hdu->rx_length += 1;
					}
					else
					{
						hdu->rx_length = -1;
						hdu->stat_rx_error_cntr += 1;
					}
				}
				else
				{
					if ( rx_byte == FRAME_ESCAPE )
					{
						hdu->rx_slip_escaped = true;
					}
					else
					{
						rx_buf[hdu->rx_length] = rx_byte;
						hdu->rx_length += 1;
					}
				}
				
				if ( hdu->rx_length == 1 )
				{
					hdu->csp_packet = ( rx_buf[0] == 0x00 );
					if ( hdu->csp_packet )
					{
						hdu->rx_length += 1;	// padding --> same size as PQP
					}
				}
				
				if ( hdu->csp_packet )
				{
					if ( hdu->rx_length == 6 )	// check dst addr, reserved and flags
					{
						if ( rx_buf[5] != CSP_HEADER_FLAGS )	// reserved and flags
						{
							hdu->rx_length = -1;
							hdu->stat_rx_error_cntr += 1;
						}
						uint16_t addr = rx_buf[2];
						addr <<= 8;
						addr += rx_buf[3];
						if ( pqp_destination_irrelevant( ( ( addr >> 9 ) & 0x1F ), ( ( addr >> 4 ) & 0x1F ) ) )
						{
							hdu->rx_length = -1;
						}
					}
				}
				else
				{
					if ( hdu->rx_length == 4 )	// check dst addr, 111110 bits and HLEN
					{
						if ( pqp_destination_irrelevant( rx_buf[2], rx_buf[3] ) )
						{
							hdu->rx_length = -1;
						}
						if ( ( rx_buf[0] & 0x3F ) != 0x3E )
						{
							hdu->rx_length = -1;
							hdu->stat_rx_error_cntr += 1;
						}
						if ( ( ( rx_buf[1] >> 4 ) & 0x03 ) != 2 )
						{
							hdu->rx_length = -1;
							hdu->stat_rx_error_cntr += 1;
						}
					}
				}
			}
		}
		break_cntr += 1;
		if ( break_cntr > ( HEADER_SIZE + PQP_MAX_PAYLOAD + CRC32_SIZE ) )
		{
			break;
		}
	}
	
	
	if ( hdu->tx_status > NOP )
	{
		if ( hdu->tx_slip_length <= 0)
		{
			hdu->tx_status = NOP;
		}
		
		if ( hdu->tx_status == WAIT_FOR_LINE )
		{
			if ( hdu->tx_attempt_cntr > MAX_TX_RETY_COUNT )
			{
				hdu->tx_status = NOP;
				hdu->tx_slip_length = 0;
			}
			else
			{
				bool idle = true;
				if ( ( now - hdu->last_rx_time ) <= 1 )	// received bytes within 1ms, not idle
				{
					idle = false;
				}
				if ( idle && hdu->bus_rx_busy() )
				{
					idle = false;
				}
#ifdef LIBPQP_TWEAK_HDUART_PRIORITY
				idle = true;
#endif
				if ( idle && ( hdu->tx_attempt_cntr > 0 ) )
				{
					if ( ( ( int32_t )( now - hdu->next_tx_attempt_time ) ) < 0 )
					{
						idle = false;
					}
				}
				
				if ( idle )
				{
					hdu->bus_send_data( hdu->tx_slip_buffer, hdu->tx_slip_length );
					hdu->tx_status = UNDER_SEND;
				}
			}
		} else
		if ( hdu->tx_status == UNDER_SEND )
		{
			int status = hdu->bus_send_status( );
			if ( status > 0 )
			{
				hdu->tx_status = NOP;
				hdu->tx_slip_length = 0;
				hdu->stat_tx_cntr += 1;
			} else
			if ( status < 0 )
			{
				hdu->tx_status = WAIT_FOR_LINE;
				hdu->tx_attempt_cntr += 1;
				uint32_t random_mask = 8 << hdu->tx_attempt_cntr;		// 16, 32, 64, ...
				random_mask -= 1;						// %16, %32, ..
				// "randomness" from low bits of "now"
				hdu->next_tx_attempt_time = now + ( ( pqp_get_my_address( ) * hdu->tx_attempt_cntr ) / 2 ) + ( now & random_mask );
				hdu->stat_tx_collision_cntr += 1;
			}
		}
		else
		{
			hdu->tx_status = NOP;
		}
	}
}

static bool rx_available( void * if_private, pqp_prio_t priority )
{
	hdu_t * hdu = (hdu_t*)if_private;
	
	if ( hdu->received_packet_length < ( HEADER_SIZE + CRC32_SIZE ) )
	{
		return false;
	}
	return ( hdu->rx_priority == priority );
}

static bool rx_get_packet( void * if_private, pqp_prio_t priority, pqp_packet_t * packet )
{
	hdu_t * hdu = (hdu_t*)if_private;
	
	if ( hdu->received_packet_length < ( HEADER_SIZE + CRC32_SIZE ) )
	{
		return false;
	}
	if ( hdu->rx_priority != priority )
	{
		return false;
	}
	
	int buf_index = ( ( hdu->current_rx_buffer + 1 ) % 2 );
	uint8_t * rx_buffer = hdu->rx_double_buffer[buf_index];
	
	if ( hdu->received_csp_packet )
	{
		// rx_buffer[0] is 0x00, [1] is padding
		uint32_t csp_header = 0;
		csp_header += rx_buffer[2];
		csp_header <<= 8;
		csp_header += rx_buffer[3];
		csp_header <<= 8;
		csp_header += rx_buffer[4];
		csp_header <<= 8;
		csp_header += rx_buffer[5];	// from big-endian
		
		packet->priority = hdu->rx_priority;
		packet->flags = 0x01;	// CSP
		packet->ttl = LIBPQP_LEGACY_CSP_DEFAULT_TTL;
		packet->src_addr = ( ( csp_header >> 25 ) & 0x1F);
		packet->dst_addr = ( ( csp_header >> 20 ) & 0x1F);
		packet->src_port = ( ( csp_header >>  8 ) & 0x3F);
		packet->dst_port = ( ( csp_header >> 14 ) & 0x3F);
		packet->payload_length = hdu->received_packet_length - ( HEADER_SIZE + CRC32_SIZE );
		
		hdu->received_packet_length = 0;		// invalidate the buffer
		
		uint32_t crc;
		pqp_crc32_init( &crc );
		for ( int i = 0; i < packet->payload_length; i++ )	// only payload
		{
			pqp_crc32_update( &crc, rx_buffer[6 + i] );
		}
		crc = pqp_crc32_final( &crc );
		crc = bswap32( crc );	// to big-endian
		
		if ( memcmp( &crc, rx_buffer + HEADER_SIZE + packet->payload_length, 4 ) != 0 )	// crc mistmatch
		{
			hdu->stat_rx_crc_error_cntr += 1;
			return false;
		}
		
		if ( ( packet->src_addr == 31 ) || ( packet->dst_addr == 31 ) )
		{
			// broadcast not allowed
			hdu->stat_rx_error_cntr += 1;
			return false;
		}
		
		if ( packet->payload_length > 0 )
		{
			memcpy( packet->payload, rx_buffer + HEADER_SIZE, packet->payload_length );
		}
		
		packet->crc32c = pqp_calculate_crc32c( packet );	// have to calculate PQP checksum
		
		hdu->stat_rx_valid_cntr += 1;
		return true;
	}
	
	packet->priority = hdu->rx_priority;
	packet->flags = ( ( rx_buffer[1] >> 6 ) & 0x03 );
	packet->ttl = ( ( rx_buffer[1] >> 0 ) & 0x0F );
	packet->src_addr = rx_buffer[2];
	packet->dst_addr = rx_buffer[3];
	packet->src_port = rx_buffer[4];
	packet->dst_port = rx_buffer[5];
	memcpy( &( packet->crc32c ), rx_buffer + 6, 4 );
	packet->payload_length = hdu->received_packet_length - ( HEADER_SIZE + CRC32_SIZE );
	if ( packet->payload_length > 0 )
	{
		memcpy( packet->payload, rx_buffer + ( HEADER_SIZE + CRC32_SIZE ), packet->payload_length );
	}
	
	hdu->received_packet_length = 0;		// invalidate the buffer
	
	if ( packet->crc32c == pqp_calculate_crc32c( packet ) )
	{
		hdu->stat_rx_valid_cntr += 1;
		return true;
	}
	
	hdu->stat_rx_crc_error_cntr += 1;
	return false;
}

static int encode_byte_to_slip( uint8_t byte, uint8_t * buf, int len )
{
	if ( byte == FRAME_END )
	{
		buf[len] = FRAME_ESCAPE;
		len += 1;
		buf[len] = ESCAPED_END;
		len += 1;
		return len;
	}
	if ( byte == FRAME_ESCAPE )
	{
		buf[len] = FRAME_ESCAPE;
		len += 1;
		buf[len] = ESCAPED_ESCAPE;
		len += 1;
		return len;
	}
	
	buf[len] = byte;
	len += 1;
	return len;
}

static bool tx_enqueue_packet( void * if_private, pqp_packet_t * packet )
{
	hdu_t * hdu = (hdu_t*)if_private;
	
	if ( hdu->tx_slip_length > 0 )
	{
		return false;
	}
	
	uint8_t * tx_buf = hdu->tx_slip_buffer;
	tx_buf[0] = FRAME_END;
	int tx_len = 1;
	uint8_t tmp8, crc_bytes[4];
	uint8_t prio = packet->priority;
	
	if ( packet->flags == 0x01 )	// CSP packet
	{
		if ( ( packet->src_addr >= 31 ) || ( packet->dst_addr >= 31 ) || ( packet->src_port > 63 ) || ( packet->dst_port > 63 ) )
		{
			// silently drop legacy CSP packets with invalid addresses and/or ports
			// broadcast not allowed
			packet->packet_status = FREE;
			return true;
		}
		
		uint32_t csp_header = 0;
		csp_header += prio;
		csp_header <<= 5;
		csp_header += packet->src_addr;
		csp_header <<= 5;
		csp_header += packet->dst_addr;
		csp_header <<= 6;
		csp_header += packet->dst_port;
		csp_header <<= 6;
		csp_header += packet->src_port;
		csp_header <<= 8;
		csp_header += CSP_HEADER_FLAGS;
		csp_header = bswap32( csp_header );	// to big-endian
		
		tmp8 = 0x00;
		tx_len = encode_byte_to_slip( tmp8, tx_buf, tx_len );
		for ( int i = 0; i < 4; i++ )
		{
			tmp8 = (csp_header & 0xFF );
			tx_len = encode_byte_to_slip( tmp8, tx_buf, tx_len );
			csp_header >>= 8;
		}
		
		uint32_t crc;
		pqp_crc32_init( &crc );
		for ( int i = 0; i < packet->payload_length; i++ )	// only payload
		{
			pqp_crc32_update( &crc, packet->payload[i] );
			tx_len = encode_byte_to_slip( packet->payload[i], tx_buf, tx_len );
		}
		crc = pqp_crc32_final( &crc );
		crc = bswap32( crc );	// to big-endian
		for ( int i = 0; i < 4; i++ )
		{
			tmp8 = (crc & 0xFF );
			tx_len = encode_byte_to_slip( tmp8, tx_buf, tx_len );
			crc >>= 8;
		}
	}
	else
	{
		tmp8 = ( ( prio << 6 ) | 0x3E );
		tx_len = encode_byte_to_slip( tmp8, tx_buf, tx_len );
		tmp8 = ( ( ( packet->flags & 0x03 ) << 6 ) | ( 0x20 ) | ( ( packet->ttl & 0x0F) << 0 ) );
		tx_len = encode_byte_to_slip( tmp8, tx_buf, tx_len );
		tx_len = encode_byte_to_slip( packet->src_addr, tx_buf, tx_len );
		tx_len = encode_byte_to_slip( packet->dst_addr, tx_buf, tx_len );
		tx_len = encode_byte_to_slip( packet->src_port, tx_buf, tx_len );
		tx_len = encode_byte_to_slip( packet->dst_port, tx_buf, tx_len );
		memcpy( crc_bytes, &( packet->crc32c ), 4 );
		for ( int i = 0; i < 4; i++ )		// CRC sent before the payload
		{
			tx_len = encode_byte_to_slip( crc_bytes[i], tx_buf, tx_len );
		}
		for ( int i = 0; i < packet->payload_length; i++ )
		{
			tx_len = encode_byte_to_slip( packet->payload[i], tx_buf, tx_len );
		}
	}
	tx_buf[tx_len] = FRAME_END;
	tx_len += 1;
	hdu->tx_slip_length = tx_len;
	hdu->tx_status = WAIT_FOR_LINE;
	hdu->tx_attempt_cntr = 0;
	
	packet->packet_status = FREE;
	return true;
}

static uint8_t get_info( void * if_private, uint8_t * data )	// data can be NULL, in this case the same length must be returned
{
	if ( if_private == NULL ) return 0;
	hdu_t * hdu = (hdu_t*)if_private;
	
	uint8_t len = 0;
	if ( data != NULL )
	{
		memcpy( data + len, "HDU", 3 );
	}
	len += 3;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( hdu->stat_tx_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( hdu->stat_tx_collision_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( hdu->stat_rx_valid_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( hdu->stat_rx_crc_error_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( hdu->stat_rx_error_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		data[len] = hdu->tx_status;
	}
	len += 1;
	
	return len;
}




#if defined( LIBPQP_HAS_HDUPLEX_UART1 )
static hdu_t hdu1;
#endif

#if defined( LIBPQP_HAS_HDUPLEX_UART2 )
static hdu_t hdu2;
#endif

// public

#if defined( LIBPQP_HAS_HDUPLEX_UART1 )
void PQP_IF_OBJ_INITIALIZER_HDUART1( pqp_interface_t * pqp_interface )
{
	init( &hdu1 );
	hdu1.bus_deinit = pqpf_hduart_if1_deinit;
	hdu1.bus_rx_busy = pqpf_hduart_if1_is_busy;
	hdu1.bus_send_data = pqpf_hduart_if1_send_data;
	hdu1.bus_send_status = pqpf_hduart_if1_send_status;
	hdu1.bus_receive_data = pqpf_hduart_if1_receive_data;
	
	pqp_interface->if_private = &hdu1;
	pqp_interface->if_deinit = deinit;
	pqp_interface->if_process = process;
	pqp_interface->if_rx_available = rx_available;
	pqp_interface->if_rx_get_packet = rx_get_packet;
	pqp_interface->if_tx_enqueue_packet = tx_enqueue_packet;
	pqp_interface->if_info = get_info;
	
	pqpf_hduart_if1_init( );
};
#endif

#if defined( LIBPQP_HAS_HDUPLEX_UART2 )
void PQP_IF_OBJ_INITIALIZER_HDUART2( pqp_interface_t * pqp_interface )
{
	init( &hdu2 );
	hdu2.bus_deinit = pqpf_hduart_if2_deinit;
	hdu2.bus_rx_busy = pqpf_hduart_if2_is_busy;
	hdu2.bus_send_data = pqpf_hduart_if2_send_data;
	hdu2.bus_send_status = pqpf_hduart_if2_send_status;
	hdu2.bus_receive_data = pqpf_hduart_if2_receive_data;
	
	pqp_interface->if_private = &hdu2;
	pqp_interface->if_deinit = deinit;
	pqp_interface->if_process = process;
	pqp_interface->if_rx_available = rx_available;
	pqp_interface->if_rx_get_packet = rx_get_packet;
	pqp_interface->if_tx_enqueue_packet = tx_enqueue_packet;
	pqp_interface->if_info = get_info;
	
	pqpf_hduart_if2_init( );
};
#endif

