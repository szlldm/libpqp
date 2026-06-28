/* Copyright (C) 2026 szlldm
 * 
 * CAN interface.
 * This file is part of libpqp.
 * 
 * LibPQP is dual-licensed: you may use it under the terms of the
 * GNU Affero General Public License version 3 (AGPLv3), or alternatively
 * under a commercial license.
 * You should have received a copy of the AGPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 * For commercial licensing, please contact.
 */

#include "libpqp_if_can.h"
#include <string.h>	//memcpy

#define PACKET_COUNTER_MASK	( 0x07 )
#define PACKET_MIN_SIZE		( 1 + 1 + 1 + 4 )		// {FLAGS | HLEN |   TTL} + SRC port + DST port + CRC32

#ifndef LIBPQP_IF_CAN_RX_DISCARD_INCOMPLETE_PACKETS_AFTER_MS
	#define PACKET_DISCARD_AGE			( 30000 )		// 30 sec
#else
	#define PACKET_DISCARD_AGE			( LIBPQP_IF_CAN_RX_DISCARD_INCOMPLETE_PACKETS_AFTER_MS )
#endif

#ifndef LIBPQP_IF_CAN_TX_RESET_TRANSMITTER_AFTER_MS
	#define TX_UNSTUCK_AGE				( 10000 )		// 10 sec
#else
	#define TX_UNSTUCK_AGE				( LIBPQP_IF_CAN_TX_RESET_TRANSMITTER_AFTER_MS )
#endif

typedef struct
{
	uint8_t payload[8];
	uint8_t payload_length;
	uint8_t src_addr;
	uint8_t dst_addr;
	uint8_t priority;
	uint8_t packet_cntr;
	uint8_t fragment_cntr;
} can_fragment_t;			// should be 14 bytes

typedef struct
{
	uint32_t eid;
	uint32_t data_len;
	uint32_t sent;
	uint8_t fragment_cntr;
	uint8_t head_bytes[3];
	uint8_t * payload;
	uint8_t crc32_bytes[4];
} tx_status_t;

#ifndef LIBPQP_IF_CAN_RX_FRAGMENT_BUFFER_SIZE
	#define RX_FRAG_BUF_SIZE		( 64 )
#else
	#define RX_FRAG_BUF_SIZE		( LIBPQP_IF_CAN_RX_FRAGMENT_BUFFER_SIZE )
#endif

#ifndef LIBPQP_IF_CAN_RX_MAX_PACKET_BUFFERS
	#define RX_MAX_PACKETS			( 8 )
#else
	#define RX_MAX_PACKETS			( LIBPQP_IF_CAN_RX_MAX_PACKET_BUFFERS )
#endif

#if ( RX_FRAG_BUF_SIZE < 64 )
	#error LIBPQP_IF_CAN_RX_FRAGMENT_BUFFER_SIZE is not sufficient
#endif

#if ( RX_MAX_PACKETS < 2 )
	#error LIBPQP_IF_CAN_RX_MAX_PACKET_BUFFERS is not sufficient
#endif

typedef struct
{
	void				( * bus_deinit )( void );
	void				( * bus_clear_tx )( void );
	int				( * bus_enqueue_fragment )( uint32_t eid, uint8_t * data, int data_len);
	int				( * bus_received_fragment )( uint32_t * eid, uint8_t * data );
	
	uint32_t			rx_stack_head, rx_stack_tail;
	can_fragment_t			rx_fragment_stack[RX_FRAG_BUF_SIZE];
	
	uint8_t				rx_assembly_buffer[PQP_MAX_PAYLOAD + 10];
	uint32_t			rx_packets_head, rx_packets_tail;
	pqp_packet_t			rx_packets[RX_MAX_PACKETS];
	
	uint32_t			tx_last_updated;
	pqp_packet_t *			tx_packets[PQP_PRIO_LEVELS];
	tx_status_t			tx_status[PQP_PRIO_LEVELS];
	
	uint32_t 			stat_tx_cntr, stat_tx_unstuck_cntr, stat_rx_cntr, stat_rx_lost_cntr, stat_rx_lost_fragment_cntr;
	uint16_t 			stat_rogue_cntr;
} can_t;

#if defined( LIBPQP_HAS_CAN1 ) || defined( LIBPQP_HAS_CAN2 )

static uint8_t pqp_can_packet_counter[128];	// 3 bit per address

static void init( can_t * can )
{
	can->rx_stack_head = 0;
	can->rx_stack_tail = 0;
	can->rx_packets_head = 0;
	can->rx_packets_tail = 0;
	
	for ( unsigned int i = 0; i < PQP_PRIO_LEVELS; i++ )
	{
		can->tx_packets[i] = NULL;
	}
	
	for ( int i=0; i<128; i++ )
	{
		pqp_can_packet_counter[i] = 0;
	}
	
	can->stat_tx_cntr = 0;
	can->stat_tx_unstuck_cntr = 0;
	can->stat_rx_cntr = 0;
	can->stat_rx_lost_cntr = 0;
	can->stat_rx_lost_fragment_cntr = 0;
	can->stat_rogue_cntr = 0;
}

static inline uint8_t src_addr_from_eid( uint32_t eid )
{
	return ( ( eid >> 21 ) & 0xFF );
}

static inline uint8_t dst_addr_from_eid( uint32_t eid )
{
	return ( ( eid >> 11 ) & 0xFF );
}

static inline uint8_t priority_from_eid( uint32_t eid )
{
	return ( ( eid >> 19 ) & 0x03 );
}

static inline bool end_flag_from_eid( uint32_t eid )
{
	return ( eid & 0x0400 );
}

static inline uint8_t packet_cntr_from_eid( uint32_t eid )
{
	return ( ( eid >> 7 ) & 0x07 );
}

static inline uint8_t fragment_cntr_from_eid( uint32_t eid )
{
	return ( eid  & 0x7F );
}

static uint8_t get_packet_counter_of( uint8_t dst_addr )
{
	//packet_counter is 3 bit
	uint8_t dc = pqp_can_packet_counter[ dst_addr / 2 ];
	uint8_t pc;
	if ( dst_addr % 2 )
	{
		pc = ( ( dc >> 4 ) & 0x07 );
	}
	else
	{
		pc = ( dc & 0x07 );
	}
	pc += 1;
	pc &= 0x07;
	if ( dst_addr % 2 )
	{
		dc &= 0x07;
		dc |= ( pc << 4 );
	}
	else
	{
		dc &= 0x70;
		dc |= pc;
	}
	pqp_can_packet_counter[ dst_addr / 2 ] = dc;
	return pc;
}

static uint8_t get_nth_tx_byte( tx_status_t * tx_status, uint32_t n )
{
	if ( n < 3 )
	{
		return tx_status->head_bytes[n];
	}
	if ( n < ( tx_status->data_len - 4 ) )
	{
		return tx_status->payload[ n - 3 ];
	}
	if ( n < tx_status->data_len )
	{
		uint32_t offset = tx_status->data_len - n;	// 4 .. 1
		offset = 4 - offset;				// 0 .. 3
		if ( offset > 3 ) return 0;
		return tx_status->crc32_bytes[offset];
	}
	
	return 0;
}

static void transmit_fragments( can_t * can, uint32_t now )
{
	uint32_t eid;
	uint8_t data[8];
	int data_len;
	uint32_t amount;
	bool tx_full = false;
	bool tx_attempt = false;
	
	for ( unsigned int i = 0; i < PQP_PRIO_LEVELS; i++ )
	{
		if ( can->tx_packets[i] != NULL )
		{
			while ( can->tx_status[i].sent < can->tx_status[i].data_len )
			{
				eid = can->tx_status[i].eid;
				eid |= ( can->tx_status[i].fragment_cntr & 0x7F );
				amount = can->tx_status[i].data_len - can->tx_status[i].sent;
				if ( amount <= 8 )	// last fragment
				{
					eid |= 0x0400;
					data_len = amount;
				}
				else
				{
					data_len = 8;
				}
				
				for ( uint32_t k=0; k<data_len; k++ )
				{
					data[k] = get_nth_tx_byte( &(can->tx_status[i]), ( can->tx_status[i].sent + k ) );
				}
				
				tx_attempt = true;
				if ( can->bus_enqueue_fragment( eid, data, data_len) < 0 )
				{
					tx_full = true;
					break;
				}
				else
				{
					can->tx_last_updated = now;
					can->tx_status[i].fragment_cntr += 1;
					can->tx_status[i].sent += data_len;
					if ( can->tx_status[i].sent >= can->tx_status[i].data_len )	// all sent
					{
						can->tx_packets[i]->packet_status = FREE;
						can->tx_packets[i] = NULL;
						can->stat_tx_cntr += 1;
						break;
					}
				}
			}
			
			if ( tx_full )
			{
				break;
			}
		}
	}
	
	if ( !tx_attempt )
	{
		can->tx_last_updated = now;
	}
}


static void deinit( void * if_private )
{
	can_t * can = (can_t*)if_private;
	can->bus_deinit( );

	for ( unsigned int i = 0; i < PQP_PRIO_LEVELS; i++ )
	{
		if ( can->tx_packets[i] != NULL )
		{
			can->tx_packets[i]->packet_status = FREE;
			can->tx_packets[i] = NULL;
		}
	}
}

static void process( void * if_private )
{
	can_t * can = (can_t*)if_private;
	uint32_t now = pgpf_get_ms_timestamp( );
	int data_len;
	uint32_t eid;
	can_fragment_t * current_fragment = NULL;
	
	// send fragments
	transmit_fragments( can, now );
	
	can->rx_stack_head %= RX_FRAG_BUF_SIZE;
	can->rx_stack_tail %= RX_FRAG_BUF_SIZE;
	current_fragment = &( can->rx_fragment_stack[can->rx_stack_head] );
	
	// check received fragments
	while ( ( data_len = can->bus_received_fragment( &eid, current_fragment->payload ) ) >= 0 )
	{
		if ( data_len > 8 ) continue;
		if ( pqp_destination_irrelevant( src_addr_from_eid( eid ), dst_addr_from_eid( eid ) ) ) continue;		// drop fragment
		
		current_fragment->payload_length = data_len;
		current_fragment->src_addr = src_addr_from_eid( eid );
		current_fragment->dst_addr = dst_addr_from_eid( eid );
		current_fragment->priority = priority_from_eid( eid );
		current_fragment->packet_cntr = packet_cntr_from_eid( eid );
		current_fragment->fragment_cntr = fragment_cntr_from_eid( eid );
		
		can->rx_stack_head += 1;
		if ( can->rx_stack_head >= RX_FRAG_BUF_SIZE )
		{
			can->rx_stack_head = 0;
		}
		if ( can->rx_stack_head == can->rx_stack_tail )	// buffer overflow -> clean til packet start
		{
			do {
				can->rx_stack_tail += 1;
				if ( can->rx_stack_tail >= RX_FRAG_BUF_SIZE )
				{
					can->rx_stack_tail = 0;
				}
				can->stat_rx_lost_fragment_cntr += 1;
				if ( can->rx_fragment_stack[can->rx_stack_tail].fragment_cntr == 0 ) break;
			} while ( can->rx_stack_head != can->rx_stack_tail );
		}
		
		if ( !end_flag_from_eid(eid) )
		{
			current_fragment = &( can->rx_fragment_stack[can->rx_stack_head] );
			continue;
		}
		
		// else: end_flag
		int reverse_pos = PQP_MAX_PAYLOAD + 10;
		uint8_t src_addr = current_fragment->src_addr;
		uint8_t dst_addr = current_fragment->dst_addr;
		uint8_t priority = current_fragment->priority;
		uint8_t packet_cntr = current_fragment->packet_cntr;
		uint8_t fragment_cntr = current_fragment->fragment_cntr + 1;
		uint32_t head = can->rx_stack_head;
		bool broken = false;
		if ( current_fragment->fragment_cntr >= 64 )
		{
			reverse_pos = -1;
			can->stat_rogue_cntr += 1;
		}
		while ( head != can->rx_stack_tail )
		{
			head -= 1;
			if ( head > ( RX_FRAG_BUF_SIZE - 1 ) )
			{
				head = ( RX_FRAG_BUF_SIZE - 1 );
			}
			current_fragment = &( can->rx_fragment_stack[head] );
			
			if ( ( src_addr != current_fragment->src_addr ) || 
			     ( dst_addr != current_fragment->dst_addr ) || 
			     ( priority != current_fragment->priority ) || 
			     ( packet_cntr != current_fragment->packet_cntr ) )
			{
				broken = true;
				continue;
			}
			
			int payload_len;
			if ( ( fragment_cntr - 1 ) == current_fragment->fragment_cntr )	// ok
			{
				payload_len = current_fragment->payload_length;
			} else
			if ( fragment_cntr == current_fragment->fragment_cntr )		// retransmission
			{
				payload_len = 0;
			}
			else								// missing fragment
			{
				payload_len = 0;
				reverse_pos = -1;
				can->stat_rx_lost_fragment_cntr += 1;
			}
			
			if ( ( payload_len > 0 ) && ( reverse_pos >= 0 ) )
			{
				reverse_pos -= payload_len;
				if ( reverse_pos >= 0 )
				{
					memcpy( can->rx_assembly_buffer + reverse_pos, current_fragment->payload, payload_len );
				}
				else
				{
					can->stat_rogue_cntr += 1;
				}
			}
			
			fragment_cntr = current_fragment->fragment_cntr;
			
			if ( broken )
			{
				current_fragment->fragment_cntr = 255;	// invalidate
			}
			else
			{
				can->rx_stack_head = head;
			}
			
			if ( fragment_cntr == 0 )
			{
				break;	// done
			}
		}
		
		// clean
		while ( can->rx_stack_head != can->rx_stack_tail ) {
			if ( can->rx_fragment_stack[can->rx_stack_tail].fragment_cntr == 0 ) break;
			if ( can->rx_fragment_stack[can->rx_stack_tail].fragment_cntr != 255 )
			{
				can->stat_rx_lost_fragment_cntr += 1;
			}
			can->rx_stack_tail += 1;
			if ( can->rx_stack_tail >= RX_FRAG_BUF_SIZE )
			{
				can->rx_stack_tail = 0;
			}
		}
		
		if ( ( fragment_cntr == 0 ) && ( reverse_pos >= 0 ) )	// assembled frame
		{
			can->rx_packets_head %= RX_MAX_PACKETS;
			can->rx_packets_tail %= RX_MAX_PACKETS;
			pqp_packet_t * packet = &( can->rx_packets[can->rx_packets_head] );
			bool exit_rx = false;
			bool packet_received = false;
			do {
				uint32_t next_head = can->rx_packets_head + 1;
				if ( next_head >= RX_MAX_PACKETS )
				{
					next_head = 0;
				}
				if ( can->rx_packets_tail == next_head )	// overflow, drop lower priority
				{
					if ( packet->src_addr > can->rx_packets[can->rx_packets_tail].src_addr ) break;
					if ( packet->src_addr == can->rx_packets[can->rx_packets_tail].src_addr )
					{
						if ( packet->priority > can->rx_packets[can->rx_packets_tail].priority ) break;
						if ( packet->priority == can->rx_packets[can->rx_packets_tail].priority )
						{
							if ( packet->dst_addr >= can->rx_packets[can->rx_packets_tail].dst_addr ) break;
						}
					}
					can->rx_packets_tail += 1;
					if ( can->rx_packets_tail >= RX_MAX_PACKETS )
					{
						can->rx_packets_tail = 0;
					}
				}
				
				int len =  PQP_MAX_PAYLOAD + 10 - reverse_pos;
				if ( len < 7 ) break;
				uint8_t hlen = ( ( can->rx_assembly_buffer[reverse_pos] >> 4 ) & 0x03 );
				if ( hlen != 2 ) break;
				packet->ttl = ( can->rx_assembly_buffer[reverse_pos] & 0x0F );
				packet->flags = ( ( can->rx_assembly_buffer[reverse_pos] >> 6 ) & 0x03 );
				packet->src_port = can->rx_assembly_buffer[reverse_pos + 1];
				packet->dst_port = can->rx_assembly_buffer[reverse_pos + 2];
				packet->crc32c = 0;
				for ( int i=0; i<4; i++ )
				{
					packet->crc32c <<= 8;
					packet->crc32c += can->rx_assembly_buffer[PQP_MAX_PAYLOAD + 10 - 4 + i];
				}
				reverse_pos += 3;
				len -= 7;
				packet->payload_length = len;
				memcpy( packet->payload, &( can->rx_assembly_buffer[reverse_pos] ), len );
				packet->priority = priority;
				packet->src_addr = src_addr;
				packet->dst_addr = dst_addr;
				if ( packet->crc32c != pqp_calculate_crc32c( packet ) ) break;
				packet_received = true;
				
				can->rx_packets_head = next_head;
				
				next_head += 1;
				if ( next_head >= RX_MAX_PACKETS )
				{
					next_head = 0;
				}
				if ( can->rx_packets_tail == next_head )	// next would not fit
				{
					exit_rx = true;
					break;
				}
			} while( 0 );
			
			if ( packet_received )
			{
				can->stat_rx_cntr += 1;
			}
			else
			{
				can->stat_rx_lost_cntr += 1;
			}
			
			if ( exit_rx ) break;
		}
		
		current_fragment = &( can->rx_fragment_stack[can->rx_stack_head] );
	}
	
	// unstuck CAN transmitter
	if ( ( now - can->tx_last_updated ) > TX_UNSTUCK_AGE )
	{
		can->bus_clear_tx( );
		for ( unsigned int i = 0; i < PQP_PRIO_LEVELS; i++ )
		{
			if ( can->tx_packets[i] != NULL )
			{
				if ( can->tx_status[i].sent > 0 )	// if tx not started yet, keep the packet
				{
					can->tx_packets[i]->packet_status = FREE;
					can->tx_packets[i] = NULL;
				}
			}
		}
		can->stat_tx_unstuck_cntr += 1;
	}
	
	// send fragments again ( if receive process took a lot of time, there might be chance to schedule fragments again )
	transmit_fragments( can, now );
}

static bool rx_available( void * if_private, pqp_prio_t priority )
{
	can_t * can = (can_t*)if_private;
	if ( can->rx_packets_head == can->rx_packets_tail ) return false;
	can->rx_packets_head %= RX_MAX_PACKETS;
	can->rx_packets_tail %= RX_MAX_PACKETS;
	if ( can->rx_packets_head == can->rx_packets_tail ) return false;
	
	return ( can->rx_packets[can->rx_packets_tail].priority == priority );
}

static bool rx_get_packet( void * if_private, pqp_prio_t priority, pqp_packet_t * packet )
{
	can_t * can = (can_t*)if_private;
	if ( can->rx_packets_head == can->rx_packets_tail ) return false;
	can->rx_packets_head %= RX_MAX_PACKETS;
	can->rx_packets_tail %= RX_MAX_PACKETS;
	if ( can->rx_packets_head == can->rx_packets_tail ) return false;
	if ( can->rx_packets[can->rx_packets_tail].priority != priority ) return false;
	memcpy( packet, &( can->rx_packets[can->rx_packets_tail] ), sizeof( pqp_packet_t ) );
	
	can->rx_packets_tail += 1;
	if ( can->rx_packets_tail >= RX_MAX_PACKETS )
	{
		can->rx_packets_tail = 0;
	}
	
	return true;
}

static bool tx_enqueue_packet( void * if_private, pqp_packet_t * packet )
{
	can_t * can = (can_t*)if_private;
	
	if ( packet->priority >= PQP_PRIO_LEVELS ) return false;
	
	if ( can->tx_packets[packet->priority] != NULL ) return false;	// slot already taken
	
	packet->packet_status = INTERFACE;
	can->tx_packets[packet->priority] = packet;
	can->tx_status[packet->priority].fragment_cntr = 0;
	can->tx_status[packet->priority].eid = 0;
	can->tx_status[packet->priority].eid |= ( ( get_packet_counter_of( packet->dst_addr ) & 0x07 ) << 7 );
	can->tx_status[packet->priority].eid |= ( ( packet->dst_addr & 0xFF ) << 11 );
	can->tx_status[packet->priority].eid |= ( ( packet->priority & 0x03 ) << 19 );
	can->tx_status[packet->priority].eid |= ( ( packet->src_addr & 0xFF ) << 21 );
	can->tx_status[packet->priority].data_len = PACKET_MIN_SIZE + packet->payload_length;
	can->tx_status[packet->priority].sent = 0;
	uint8_t hlen = 2;
	can->tx_status[packet->priority].head_bytes[0] = ( ( ( packet->flags & 0x03 ) << 6 )  |  ( ( hlen & 0x03 ) << 4 )  |  ( ( packet->ttl & 0x0F ) << 0 ) );
	can->tx_status[packet->priority].head_bytes[1] = packet->src_port;
	can->tx_status[packet->priority].head_bytes[2] = packet->dst_port;
	can->tx_status[packet->priority].payload = packet->payload;
	uint32_t crc32 = packet->crc32c;
	for ( int i=0; i<4; i++ )
	{
		can->tx_status[packet->priority].crc32_bytes[i] = ( ( crc32 >> 24 ) & 0xFF );	// MSB first
		crc32 <<= 8;
	}
	
	return true;
}

static uint8_t get_info( void * if_private, uint8_t * data )	// data can be NULL, in this case the same length must be returned
{
	if ( if_private == NULL ) return 0;
	can_t * can = (can_t*)if_private;
	
	uint8_t len = 0;
	if ( data != NULL )
	{
		memcpy( data + len, "CAN", 3 );
	}
	len += 3;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( can->stat_tx_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( can->stat_tx_unstuck_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( can->stat_rx_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( can->stat_rx_lost_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( can->stat_rx_lost_fragment_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( can->stat_rogue_cntr ), 2 );
	}
	len += 2;
	
	if ( data != NULL )
	{
		data[len] = 0;
		for ( unsigned int i = 0; i < PQP_PRIO_LEVELS; i++ )
		{
			if ( can->tx_packets[i] != NULL )
			{
				data[len] += 1;
			}
		}
	}
	len += 1;
	
	if ( data != NULL )
	{
		uint32_t head = can->rx_stack_head % RX_FRAG_BUF_SIZE;
		uint32_t tail = can->rx_stack_tail % RX_FRAG_BUF_SIZE;
		if ( head < tail )
		{
			head += RX_FRAG_BUF_SIZE;
		}
		uint16_t u16 = RX_FRAG_BUF_SIZE - ( head - tail );
		memcpy( data + len + 0 , &u16, 2 );
		u16 = RX_FRAG_BUF_SIZE;
		memcpy( data + len + 2 , &u16, 2 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		data[len + 0] = 0;
		data[len + 1] = 0;
		uint32_t head = can->rx_packets_head % RX_MAX_PACKETS;
		uint32_t tail = can->rx_packets_tail % RX_MAX_PACKETS;
		if ( head < tail )
		{
			head += RX_MAX_PACKETS;
		}
		data[len + 0] = head - tail;
		data[len + 2] = RX_MAX_PACKETS;
	}
	len += 3;
	
	return len;
}

#endif


#if defined( LIBPQP_HAS_CAN1 )
static can_t can1;
#endif

#if defined( LIBPQP_HAS_CAN2 )
static can_t can2;
#endif

// public

#if defined( LIBPQP_HAS_CAN1 )

void PQP_IF_OBJ_INITIALIZER_CAN1( pqp_interface_t * pqp_interface )
{
	init( &can1 );
	can1.bus_deinit = pqpf_can_if1_deinit;
	can1.bus_clear_tx = pqpf_can_if1_clear_tx;
	can1.bus_enqueue_fragment = pqpf_can_if1_enqueue_fragment;
	can1.bus_received_fragment = pqpf_can_if1_received_fragment;

	pqp_interface->if_private = &can1;
	pqp_interface->if_deinit = deinit;
	pqp_interface->if_process = process;
	pqp_interface->if_rx_available = rx_available;
	pqp_interface->if_rx_get_packet = rx_get_packet;
	pqp_interface->if_tx_enqueue_packet = tx_enqueue_packet;
	pqp_interface->if_info = get_info;

	pqpf_can_if1_init( );
};
#endif

#if defined( LIBPQP_HAS_CAN2 )
void PQP_IF_OBJ_INITIALIZER_CAN2( pqp_interface_t * pqp_interface )
{
	init( &can2 );
	can2.bus_deinit = pqpf_can_if2_deinit;
	can2.bus_clear_tx = pqpf_can_if2_clear_tx;
	can2.bus_enqueue_fragment = pqpf_can_if2_enqueue_fragment;
	can2.bus_received_fragment = pqpf_can_if2_received_fragment;

	pqp_interface->if_private = &can2;
	pqp_interface->if_deinit = deinit;
	pqp_interface->if_process = process;
	pqp_interface->if_rx_available = rx_available;
	pqp_interface->if_rx_get_packet = rx_get_packet;
	pqp_interface->if_tx_enqueue_packet = tx_enqueue_packet;
	pqp_interface->if_info = get_info;

	pqpf_can_if2_init( );
};
#endif

