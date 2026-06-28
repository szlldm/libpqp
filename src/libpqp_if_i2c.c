/* Copyright (C) 2026 szlldm
 * 
 * I2C interface.
 * This file is part of libpqp.
 * 
 * LibPQP is dual-licensed: you may use it under the terms of the
 * GNU Affero General Public License version 3 (AGPLv3), or alternatively
 * under a commercial license.
 * You should have received a copy of the AGPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 * For commercial licensing, please contact.
 */

#include "libpqp_if_i2c.h"
#include <stddef.h>
#include <string.h>

// private


#define I2C_HEADER_SIZE			( 6 )
#define I2C_CRC32_SIZE			( 4 )
#define PACKET_MAX_SIZE			( ( ( I2C_HEADER_SIZE + PQP_MAX_PAYLOAD + I2C_CRC32_SIZE) * 2 )  + 2 )		// slip encoded worst case + 2x Frame End (at the begin and at the end)

#define I2C_MAX_TX_RETY_COUNT		( 3 )

#define FRAME_END			( 0xC0 )
#define FRAME_ESCAPE			( 0xDB )
#define ESCAPED_END			( 0xDC )
#define ESCAPED_ESCAPE			( 0xDD )

typedef enum {
	NOP = 0,
	WAIT_FOR_LINE,
	UNDER_SEND
} i2c_tx_status_t;

typedef struct
{
	void				( * bus_deinit )( void );
	void				( * bus_init )( void );
	bool				( * bus_rx_busy )( void );
	void				( * bus_send_data )(uint8_t addr, uint8_t * data, int data_len );
	int					( * bus_send_status )( void );
	int					( * bus_receive_data )( void );
	uint8_t				( * bus_mode_status )( void );
	int 				( * bus_write_stm_status )(void );
	
	uint8_t				tx_slip_buffer[PACKET_MAX_SIZE];
	int					tx_slip_length;
	unsigned int		tx_attempt_cntr;
	i2c_tx_status_t		tx_status;
	uint32_t			next_tx_attempt_time;
	
	uint8_t				rx_double_buffer[2][I2C_HEADER_SIZE + PQP_MAX_PAYLOAD + I2C_CRC32_SIZE];
	int					current_rx_buffer;
	int					rx_length;
	bool				rx_slip_escaped;
	int					received_packet_length;
	pqp_prio_t			rx_priority;
	uint32_t			last_rx_time;
	
	uint32_t 			stat_tx_cntr, stat_tx_collision_cntr, stat_rx_valid_cntr, stat_rx_crc_error_cntr, stat_rx_error_cntr;
} i2c_t;

static void init( i2c_t * i2c )
{
	i2c->tx_slip_length = 0;
	i2c->tx_status = NOP;
	i2c->current_rx_buffer = 0;
	i2c->rx_length = -1;
	i2c->rx_slip_escaped = false;
	i2c->received_packet_length = 0;
	i2c->last_rx_time = 0;
	i2c->stat_tx_cntr = 0;
	i2c->stat_tx_collision_cntr = 0;
	i2c->stat_rx_valid_cntr = 0;
	i2c->stat_rx_crc_error_cntr = 0;
	i2c->stat_rx_error_cntr = 0;
}

static void deinit( void * if_private )
{
	i2c_t * i2c = (i2c_t*)if_private;
	i2c->bus_deinit( );
	i2c->received_packet_length = 0;
}

/*static void init( void * if_private )
{
	i2c_t * i2c = (i2c_t*)if_private;
	i2c->bus_init( );
	i2c->tx_slip_length = 0;
	i2c->tx_status = NOP;
	i2c->current_rx_buffer = 0;
	i2c->rx_length = -1;
	i2c->rx_slip_escaped = false;
	i2c->received_packet_length = 0;
}*/

static void process( void * if_private )
{
	i2c_t * i2c = (i2c_t*)if_private;
	uint32_t now = pgpf_get_ms_timestamp( );
	
	i2c->current_rx_buffer = ( i2c->current_rx_buffer % 2 );
	uint8_t * rx_buf = i2c->rx_double_buffer[i2c->current_rx_buffer];
	
	int break_cntr = 0;
	int rx_byte;
	while ( ( rx_byte = i2c->bus_receive_data() ) >= 0 )
	{
		if ( break_cntr == 0 )
		{
			i2c->last_rx_time = now;
		}
		
		if ( rx_byte == FRAME_END )
		{
			if ( i2c->rx_length >= ( I2C_HEADER_SIZE + I2C_CRC32_SIZE ) )
			{
				if ( !i2c->rx_slip_escaped )
				{
					// TODO ? CRC check here ?
					i2c->received_packet_length = i2c->rx_length;
					i2c->rx_priority = ( ( rx_buf[2] >> 6 ) & 0x03 );
					i2c->current_rx_buffer = ( ( i2c->current_rx_buffer + 1 ) % 2 );
				}
				else
				{
					i2c->stat_rx_error_cntr += 1;
				}
			}
			i2c->rx_length = 0;
			i2c->rx_slip_escaped = false;
		}
		else if ( i2c->rx_length >= 0 )
		{
			if ( i2c->rx_length >= ( I2C_HEADER_SIZE + PQP_MAX_PAYLOAD + I2C_CRC32_SIZE ) )
			{
				i2c->rx_length = -1;
				i2c->stat_rx_error_cntr += 1;
			}
			else
			{
				if ( i2c->rx_slip_escaped )
				{
					i2c->rx_slip_escaped = false;
					if ( rx_byte == ESCAPED_END )
					{
						rx_buf[i2c->rx_length] = FRAME_END;
						i2c->rx_length += 1;
					} else
					if ( rx_byte == ESCAPED_ESCAPE )
					{
						rx_buf[i2c->rx_length] = FRAME_ESCAPE;
						i2c->rx_length += 1;
					}
					else
					{
						i2c->rx_length = -1;
						i2c->stat_rx_error_cntr += 1;
					}
				}
				else
				{
					if ( rx_byte == FRAME_ESCAPE )
					{
						i2c->rx_slip_escaped = true;
					}
					else
					{
						rx_buf[i2c->rx_length] = rx_byte;
						i2c->rx_length += 1;
					}
				}
				
				if ( i2c->rx_length == 4 )	// check dst addr, 111110 bits and HLEN
				{
					if ( pqp_destination_irrelevant( rx_buf[2], rx_buf[3] ) )
					{
						i2c->rx_length = -1;
					}
					if ( ( rx_buf[0] & 0x3F ) != 0x3E )
					{
						i2c->rx_length = -1;
						i2c->stat_rx_error_cntr += 1;
					}
					if ( ( ( rx_buf[1] >> 4 ) & 0x03 ) != 2 )
					{
						i2c->rx_length = -1;
						i2c->stat_rx_error_cntr += 1;
					}
				}
			}
		}
		break_cntr += 1;
		if ( break_cntr > ( I2C_HEADER_SIZE + PQP_MAX_PAYLOAD + I2C_CRC32_SIZE ) )
		{
			break;
		}
	}
	
	
	if ( i2c->tx_status > NOP )
	{
		if ( i2c->tx_slip_length <= 0)
		{
			i2c->tx_status = NOP;
		}
		
		if ( i2c->tx_status == WAIT_FOR_LINE )
		{
			if ( i2c->tx_attempt_cntr > I2C_MAX_TX_RETY_COUNT )
			{
				i2c->tx_status = NOP;
				i2c->tx_slip_length = 0;
			}
			else
			{
				bool idle = true;
				if ( ( now - i2c->last_rx_time ) <= 1 )	// received bytes within 1ms, not idle
				{
					idle = false;
				}
				if ( idle && i2c->bus_rx_busy() )
				{
					idle = false;
				}
				if ( idle && ( i2c->tx_attempt_cntr > 0 ) )
				{
					if ( ( ( int32_t )( now - i2c->next_tx_attempt_time ) ) < 0 )
					{
						idle = false;
					}
				}
				
				if ( idle )
				{
					i2c->bus_send_data( 0x00, i2c->tx_slip_buffer, i2c->tx_slip_length );
					i2c->tx_status = UNDER_SEND;
				}
			}
		}
		else if ( i2c->tx_status == UNDER_SEND )
		{
			int status = i2c->bus_send_status( );
			if ( status > 0 )
			{
				i2c->tx_status = NOP;
				i2c->tx_slip_length = 0;
				i2c->stat_tx_cntr += 1;
			} else
			if ( status < 0 )
			{
				i2c->tx_status = WAIT_FOR_LINE;
				i2c->tx_attempt_cntr += 1;
				uint32_t random_mask = 8 << i2c->tx_attempt_cntr;		// 16, 32, 64, ...
				random_mask -= 1;						// %16, %32, ..
				// "randomness" from low bits of "now"
				i2c->next_tx_attempt_time = now + ( ( pqp_get_my_address( ) * i2c->tx_attempt_cntr ) / 2 ) + ( now & random_mask );
				i2c->stat_tx_collision_cntr += 1;
				//i2c->bus_deinit();
				//i2c->bus_init();
				//i2c->tx_status = NOP;
			}
		}
		else
		{
			i2c->tx_status = NOP;
		}
	}
}

static bool rx_available( void * if_private, pqp_prio_t priority )
{
	i2c_t * i2c = (i2c_t*)if_private;
	
	if ( i2c->received_packet_length < ( I2C_HEADER_SIZE + I2C_CRC32_SIZE ) )
	{
		return false;
	}
	return ( i2c->rx_priority == priority );
}

static bool rx_get_packet( void * if_private, pqp_prio_t priority, pqp_packet_t * packet )
{
	i2c_t * i2c = (i2c_t*)if_private;
	
	if ( i2c->received_packet_length < ( I2C_HEADER_SIZE + I2C_CRC32_SIZE ) )
	{
		return false;
	}
	if ( i2c->rx_priority != priority )
	{
		return false;
	}
	
	int buf_index = ( ( i2c->current_rx_buffer + 1 ) % 2 );
	uint8_t * rx_buffer = i2c->rx_double_buffer[buf_index];
	
	packet->priority = i2c->rx_priority;
	packet->flags = ( ( rx_buffer[1] >> 6 ) & 0x03 );
	packet->ttl = ( ( rx_buffer[1] >> 0 ) & 0x0F );
	packet->src_addr = rx_buffer[2];
	packet->dst_addr = rx_buffer[3];
	packet->src_port = rx_buffer[4];
	packet->dst_port = rx_buffer[5];
	memcpy( &( packet->crc32c ), rx_buffer + 6, 4 );
	packet->payload_length = i2c->received_packet_length - ( I2C_HEADER_SIZE + I2C_CRC32_SIZE );
	if ( packet->payload_length > 0 )
	{
		memcpy( packet->payload, rx_buffer + ( I2C_HEADER_SIZE + I2C_CRC32_SIZE ), packet->payload_length );
	}
	
	i2c->received_packet_length = 0;		// invalidate the buffer
	
	if ( packet->crc32c == pqp_calculate_crc32c( packet ) )
	{
		i2c->stat_rx_valid_cntr += 1;
		return true;
	}
	
	i2c->stat_rx_crc_error_cntr += 1;
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
	i2c_t * i2c = (i2c_t*)if_private;
	
	if ( i2c->tx_slip_length > 0 )
	{
		return false;
	}
	
	uint8_t * tx_buf = i2c->tx_slip_buffer;
	tx_buf[0] = FRAME_END;
	int tx_len = 1;
	uint8_t tmp8, crc_bytes[4];
	uint8_t prio = packet->priority;
	
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
	tx_buf[tx_len] = FRAME_END;
	tx_len += 1;
	i2c->tx_slip_length = tx_len;
	i2c->tx_status = WAIT_FOR_LINE;
	i2c->tx_attempt_cntr = 0;
	
	packet->packet_status = FREE;
	return true;
}

static uint8_t get_info( void * if_private, uint8_t * data )	// data can be NULL, in this case the same length must be returned
{
	if ( if_private == NULL ) return 0;
	i2c_t * i2c = (i2c_t*)if_private;
	
	uint8_t len = 0;
	if ( data != NULL )
	{
		memcpy( data + len, "I2C", 3 );
	}
	len += 3;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( i2c->stat_tx_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( i2c->stat_tx_collision_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( i2c->stat_rx_valid_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( i2c->stat_rx_crc_error_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( i2c->stat_rx_error_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		data[len] = i2c->tx_status;
	}
	len += 1;
	
	if ( data != NULL )
	{
		data[len] = i2c->bus_mode_status();
	}
	len += 1;
	
	if ( data != NULL )
	{
		int tmp = i2c->bus_write_stm_status();
		memcpy( data + len, &( tmp ), 4 );
	}
	len += 4;
	
	return len;
}



#if defined( LIBPQP_HAS_I2C1 )
static i2c_t i2ci1;
#endif

#if defined( LIBPQP_HAS_I2C2 )
static i2c_t i2ci2;
#endif

// public

#if defined( LIBPQP_HAS_I2C1 )
void PQP_IF_OBJ_INITIALIZER_I2C1( pqp_interface_t * pqp_interface )
{
	init( &i2ci1 );
	i2ci1.bus_deinit = pqpf_i2c_if1_deinit;
	i2ci1.bus_init = pqpf_i2c_if1_hw_init;
	i2ci1.bus_rx_busy = pqpf_i2c_if1_is_busy;
	i2ci1.bus_send_data = pqpf_i2c_if1_send_data;
	i2ci1.bus_send_status = pqpf_i2c_if1_send_status;
	i2ci1.bus_receive_data = pqpf_i2c_if1_receive_data;
	i2ci1.bus_mode_status = pqpf_i2c_if1_getModeState;
	i2ci1.bus_write_stm_status = pqpf_i2c_if1_get_write_state;
	
	pqp_interface->if_private = &i2ci1;
	pqp_interface->if_deinit = deinit;
	pqp_interface->if_process = process;
	pqp_interface->if_rx_available = rx_available;
	pqp_interface->if_rx_get_packet = rx_get_packet;
	pqp_interface->if_tx_enqueue_packet = tx_enqueue_packet;
	pqp_interface->if_info = get_info;
	
	pqpf_i2c_if1_init( );
};
#endif

#if defined( LIBPQP_HAS_I2C2 )
void PQP_IF_OBJ_INITIALIZER_I2C2( pqp_interface_t * pqp_interface )
{
	init( &i2ci2 );
	i2ci2.bus_deinit = pqpf_i2c_if2_deinit;
	i2ci2.bus_init = pqpf_i2c_if2_hw_init;
	i2ci2.bus_rx_busy = pqpf_i2c_if2_is_busy;
	i2ci2.bus_send_data = pqpf_i2c_if2_send_data;
	i2ci2.bus_send_status = pqpf_i2c_if2_send_status;
	i2ci2.bus_receive_data = pqpf_i2c_if2_receive_data;
	i2ci2.bus_mode_status = pqpf_i2c_if2_getModeState;
	i2ci2.bus_write_stm_status = pqpf_i2c_if2_get_write_state;
	
	pqp_interface->if_private = &i2ci2;
	pqp_interface->if_deinit = deinit;
	pqp_interface->if_process = process;
	pqp_interface->if_rx_available = rx_available;
	pqp_interface->if_rx_get_packet = rx_get_packet;
	pqp_interface->if_tx_enqueue_packet = tx_enqueue_packet;
	pqp_interface->if_info = get_info;
	
	pqpf_i2c_if2_init( );
};
#endif

