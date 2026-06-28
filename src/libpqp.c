/* Copyright (C) 2026 szlldm
 * 
 * LibPQP core.
 * This file is part of libpqp.
 * 
 * LibPQP is dual-licensed: you may use it under the terms of the
 * GNU Affero General Public License version 3 (AGPLv3), or alternatively
 * under a commercial license.
 * You should have received a copy of the AGPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 * For commercial licensing, please contact.
 */

#include "libpqp.h"
#include "libpqp_private.h"
#include "libpqp_foreign.h"

#include <stddef.h>
#include <string.h>		// memcpy

#include <stdio.h>

#ifdef DEBUG
	#define DEBUG_PRINT(fmt, args...) fprintf(stderr, "DEBUG%d: %s:%d:%s(): " fmt, _my_address, __FILE__, __LINE__, __func__, ##args)
#else
	#define DEBUG_PRINT(fmt, args...)
#endif

#include "libpqp_if_null.h"
#if defined( LIBPQP_HAS_CAN1 ) || defined( LIBPQP_HAS_CAN2 ) || defined( LIBPQP_HAS_CAN3 ) || defined( LIBPQP_HAS_CAN4 ) || defined( LIBPQP_HAS_CAN5 ) || defined( LIBPQP_HAS_CAN6 ) || defined( LIBPQP_HAS_CAN7 ) || defined( LIBPQP_HAS_CAN8 )
#include "libpqp_if_can.h"
#endif
#if defined( LIBPQP_HAS_HDUPLEX_UART1 ) || defined( LIBPQP_HAS_HDUPLEX_UART2 ) || defined( LIBPQP_HAS_HDUPLEX_UART3 ) || defined( LIBPQP_HAS_HDUPLEX_UART4 ) || defined( LIBPQP_HAS_HDUPLEX_UART5 ) || defined( LIBPQP_HAS_HDUPLEX_UART6 ) || defined( LIBPQP_HAS_HDUPLEX_UART7 ) || defined( LIBPQP_HAS_HDUPLEX_UART8 )
#include "libpqp_if_hduart.h"
#endif
#if defined( LIBPQP_HAS_I2C1 ) || defined( LIBPQP_HAS_I2C2 ) || defined( LIBPQP_HAS_I2C3 ) || defined( LIBPQP_HAS_I2C4 ) || defined( LIBPQP_HAS_I2C5 ) || defined( LIBPQP_HAS_I2C6 ) || defined( LIBPQP_HAS_I2C7 ) || defined( LIBPQP_HAS_I2C8 )
#include "libpqp_if_i2c.h"
#endif
#if defined( LIBPQP_HAS_RAW1 ) || defined( LIBPQP_HAS_RAW2 ) || defined( LIBPQP_HAS_RAW3 ) || defined( LIBPQP_HAS_RAW4 ) || defined( LIBPQP_HAS_RAW5 ) || defined( LIBPQP_HAS_RAW6 ) || defined( LIBPQP_HAS_RAW7 ) || defined( LIBPQP_HAS_RAW8 )
#include "libpqp_if_raw.h"
#endif
#if defined( LIBPQP_HAS_FDUPLEX_UART1 ) || defined( LIBPQP_HAS_FDUPLEX_UART2 ) || defined( LIBPQP_HAS_FDUPLEX_UART3 ) || defined( LIBPQP_HAS_FDUPLEX_UART4 ) || defined( LIBPQP_HAS_FDUPLEX_UART5 ) || defined( LIBPQP_HAS_FDUPLEX_UART6 ) || defined( LIBPQP_HAS_FDUPLEX_UART7 ) || defined( LIBPQP_HAS_FDUPLEX_UART8 )
#include "libpqp_if_fduart.h"
#endif
#if defined( LIBPQP_HAS_PIPE1 ) || defined( LIBPQP_HAS_PIPE2 ) || defined( LIBPQP_HAS_PIPE3 ) || defined( LIBPQP_HAS_PIPE4 ) || defined( LIBPQP_HAS_PIPE5 ) || defined( LIBPQP_HAS_PIPE6 ) || defined( LIBPQP_HAS_PIPE7 ) || defined( LIBPQP_HAS_PIPE8 )
#include "libpqp_if_pipe.h"
#endif
#if defined( LIBPQP_HAS_STDIOIF )
#include "libpqp_if_stdioif.h"
#endif

#define PQP_ADD_INTERFACE(X)	PQP_IF_OBJ_INITIALIZER_##X,
//static pqp_interface_initializer_t _interface_initializer_list[](pqp_interface_t * pqp_interface) =
static void * _interface_initializer_list[] = 
{
	PQP_ADD_INTERFACE( NULL )
	#include "libpqp_interface_initializer_list.h"
};
#undef PQP_ADD_INTERFACE

#define NUM_OF_INTERFACES	(sizeof(_interface_initializer_list) / sizeof(void *))

static pqp_interface_t _interface_list[NUM_OF_INTERFACES];

enum routing_interface_counter_enums {
	RICE_NULL = 0,
	#include "libpqp_routing_interface_counter_enums.h"
};



static uint8_t _my_address = LIBPQP_BROADCAST_ADDRESS;
static uint32_t _bootloader_latch = 0;

#define BUFSIZE_TX	(NUM_OF_INTERFACES + PQP_NUMBER_OF_TX_BUFFERS)
#define BUFSIZE_RX	(NUM_OF_INTERFACES + PQP_NUMBER_OF_RX_BUFFERS)


static pqp_packet_t _pbuf[BUFSIZE_TX + BUFSIZE_RX];
static pqp_packet_t * _tx_fifo[BUFSIZE_TX + BUFSIZE_RX];
static pqp_packet_t * _rx_fifo[BUFSIZE_TX + BUFSIZE_RX];

static uint8_t _tx_routing_table[256];

static uint8_t _progmap[64];

static uint8_t  cloning_target_address = 0xFF;
static uint8_t  cloning_flags = 0;
static uint16_t cloning_block_count;
static uint32_t cloning_magic = 0;
static uint32_t cloning_write_address = 0;
static uint32_t cloning_read_address = 0;

// private functions

static pqp_packet_t * _get_free_rx_packet( )
{
	for ( int k = BUFSIZE_TX; k < BUFSIZE_TX + BUFSIZE_RX; k++ )
	{
		if ( _pbuf[k].packet_status == FREE )
		{
			return ( _pbuf + k );
		}
	}
	return NULL;
}

static void _append_to( pqp_packet_t ** fifo, pqp_packet_t * packet )
{
	for ( int k=0; k<BUFSIZE_TX + BUFSIZE_RX; k++ )
	{
		if ( fifo[k] == NULL )
		{
			fifo[k] = packet;
			return;
		}
	}
}

static void _delete_element_from( pqp_packet_t ** fifo, int k )
{
	fifo[k] = NULL;
	k++;
	for ( ; k<BUFSIZE_TX + BUFSIZE_RX; k++ )
	{
		fifo[k-1] = fifo[k];
	}
	fifo[BUFSIZE_TX + BUFSIZE_RX - 1] = NULL;
}

static int _packet_to_pid( pqp_packet_t * packet )
{
	if ( packet == NULL ) return -1;
	for ( int k = 0; k < BUFSIZE_TX + BUFSIZE_RX; k++ )
	{
		if ( ( _pbuf + k ) == packet )
		{
			return k;
		}
	}
	return -1;
}

static void _swap_addresses_and_ports( pqp_packet_t * packet )
{
	uint8_t tmp;
	
	tmp = packet->src_addr;
	packet->src_addr = packet->dst_addr;
	packet->dst_addr = tmp;
	
	tmp = packet->src_port;
	packet->src_port = packet->dst_port;
	packet->dst_port = tmp;
}

static void _process_received_packet( pqp_packet_t * packet )
{
	DEBUG_PRINT("Received packet [%d]->[%d]\n", packet->src_addr, packet->dst_addr);
	// destination reached?
	if ( ( packet->dst_addr == _my_address ) || ( packet->dst_addr == LIBPQP_BROADCAST_ADDRESS ) )
	{
		packet->packet_status = USERSPACE;
		if ( packet->dst_addr == _my_address )
		{
			_tx_routing_table[packet->src_addr] = packet->interface;
		}
		_append_to( _rx_fifo, packet );
		return;
	}
	if ( packet->src_addr == _my_address )
	{
		// packet somehow came back ?!?
		packet->packet_status = FREE;
		return;
	}
	
	// fwd?
	if ( packet->ttl == 0 )
	{
		DEBUG_PRINT("TTL=0, no forwarding\n");
		packet->packet_status = FREE;
		return;
	}
	
	uint8_t fwd_if = routing_interface_translator_table[pqpf_read_routing_table( packet->src_addr, packet->dst_addr ) & 0xFF];
	if ( ( fwd_if == 0 ) || ( fwd_if >= NUM_OF_INTERFACES ) )
	{
		DEBUG_PRINT("no IF, no forwarding\n");
		packet->packet_status = FREE;
		return;
	}
	
	if (packet->interface == fwd_if)	// prevent reflection on the same interface
	{
		DEBUG_PRINT("same IF, no forwarding\n");
		packet->packet_status = FREE;
		return;
	}
	
	// fwd!
	DEBUG_PRINT("Forwarding\n");
	packet->interface = fwd_if;
	packet->timestamp = pgpf_get_ms_timestamp( );
	packet->packet_status = TO_BE_SENT;
	packet->ttl -= 1;
	_append_to( _tx_fifo, packet );
}

static const uint32_t _crc_table[256] = {
	0x00000000, 0xF26B8303, 0xE13B70F7, 0x1350F3F4, 0xC79A971F, 0x35F1141C, 0x26A1E7E8, 0xD4CA64EB,
	0x8AD958CF, 0x78B2DBCC, 0x6BE22838, 0x9989AB3B, 0x4D43CFD0, 0xBF284CD3, 0xAC78BF27, 0x5E133C24,
	0x105EC76F, 0xE235446C, 0xF165B798, 0x030E349B, 0xD7C45070, 0x25AFD373, 0x36FF2087, 0xC494A384,
	0x9A879FA0, 0x68EC1CA3, 0x7BBCEF57, 0x89D76C54, 0x5D1D08BF, 0xAF768BBC, 0xBC267848, 0x4E4DFB4B,
	0x20BD8EDE, 0xD2D60DDD, 0xC186FE29, 0x33ED7D2A, 0xE72719C1, 0x154C9AC2, 0x061C6936, 0xF477EA35,
	0xAA64D611, 0x580F5512, 0x4B5FA6E6, 0xB93425E5, 0x6DFE410E, 0x9F95C20D, 0x8CC531F9, 0x7EAEB2FA,
	0x30E349B1, 0xC288CAB2, 0xD1D83946, 0x23B3BA45, 0xF779DEAE, 0x05125DAD, 0x1642AE59, 0xE4292D5A,
	0xBA3A117E, 0x4851927D, 0x5B016189, 0xA96AE28A, 0x7DA08661, 0x8FCB0562, 0x9C9BF696, 0x6EF07595,
	0x417B1DBC, 0xB3109EBF, 0xA0406D4B, 0x522BEE48, 0x86E18AA3, 0x748A09A0, 0x67DAFA54, 0x95B17957,
	0xCBA24573, 0x39C9C670, 0x2A993584, 0xD8F2B687, 0x0C38D26C, 0xFE53516F, 0xED03A29B, 0x1F682198,
	0x5125DAD3, 0xA34E59D0, 0xB01EAA24, 0x42752927, 0x96BF4DCC, 0x64D4CECF, 0x77843D3B, 0x85EFBE38,
	0xDBFC821C, 0x2997011F, 0x3AC7F2EB, 0xC8AC71E8, 0x1C661503, 0xEE0D9600, 0xFD5D65F4, 0x0F36E6F7,
	0x61C69362, 0x93AD1061, 0x80FDE395, 0x72966096, 0xA65C047D, 0x5437877E, 0x4767748A, 0xB50CF789,
	0xEB1FCBAD, 0x197448AE, 0x0A24BB5A, 0xF84F3859, 0x2C855CB2, 0xDEEEDFB1, 0xCDBE2C45, 0x3FD5AF46,
	0x7198540D, 0x83F3D70E, 0x90A324FA, 0x62C8A7F9, 0xB602C312, 0x44694011, 0x5739B3E5, 0xA55230E6,
	0xFB410CC2, 0x092A8FC1, 0x1A7A7C35, 0xE811FF36, 0x3CDB9BDD, 0xCEB018DE, 0xDDE0EB2A, 0x2F8B6829,
	0x82F63B78, 0x709DB87B, 0x63CD4B8F, 0x91A6C88C, 0x456CAC67, 0xB7072F64, 0xA457DC90, 0x563C5F93,
	0x082F63B7, 0xFA44E0B4, 0xE9141340, 0x1B7F9043, 0xCFB5F4A8, 0x3DDE77AB, 0x2E8E845F, 0xDCE5075C,
	0x92A8FC17, 0x60C37F14, 0x73938CE0, 0x81F80FE3, 0x55326B08, 0xA759E80B, 0xB4091BFF, 0x466298FC,
	0x1871A4D8, 0xEA1A27DB, 0xF94AD42F, 0x0B21572C, 0xDFEB33C7, 0x2D80B0C4, 0x3ED04330, 0xCCBBC033,
	0xA24BB5A6, 0x502036A5, 0x4370C551, 0xB11B4652, 0x65D122B9, 0x97BAA1BA, 0x84EA524E, 0x7681D14D,
	0x2892ED69, 0xDAF96E6A, 0xC9A99D9E, 0x3BC21E9D, 0xEF087A76, 0x1D63F975, 0x0E330A81, 0xFC588982,
	0xB21572C9, 0x407EF1CA, 0x532E023E, 0xA145813D, 0x758FE5D6, 0x87E466D5, 0x94B49521, 0x66DF1622,
	0x38CC2A06, 0xCAA7A905, 0xD9F75AF1, 0x2B9CD9F2, 0xFF56BD19, 0x0D3D3E1A, 0x1E6DCDEE, 0xEC064EED,
	0xC38D26C4, 0x31E6A5C7, 0x22B65633, 0xD0DDD530, 0x0417B1DB, 0xF67C32D8, 0xE52CC12C, 0x1747422F,
	0x49547E0B, 0xBB3FFD08, 0xA86F0EFC, 0x5A048DFF, 0x8ECEE914, 0x7CA56A17, 0x6FF599E3, 0x9D9E1AE0,
	0xD3D3E1AB, 0x21B862A8, 0x32E8915C, 0xC083125F, 0x144976B4, 0xE622F5B7, 0xF5720643, 0x07198540,
	0x590AB964, 0xAB613A67, 0xB831C993, 0x4A5A4A90, 0x9E902E7B, 0x6CFBAD78, 0x7FAB5E8C, 0x8DC0DD8F,
	0xE330A81A, 0x115B2B19, 0x020BD8ED, 0xF0605BEE, 0x24AA3F05, 0xD6C1BC06, 0xC5914FF2, 0x37FACCF1,
	0x69E9F0D5, 0x9B8273D6, 0x88D28022, 0x7AB90321, 0xAE7367CA, 0x5C18E4C9, 0x4F48173D, 0xBD23943E,
	0xF36E6F75, 0x0105EC76, 0x12551F82, 0xE03E9C81, 0x34F4F86A, 0xC69F7B69, 0xD5CF889D, 0x27A40B9E,
	0x79B737BA, 0x8BDCB4B9, 0x988C474D, 0x6AE7C44E, 0xBE2DA0A5, 0x4C4623A6, 0x5F16D052, 0xAD7D5351
};

static void _send_packet( pqp_packet_t * packet )
{
	if ( packet->dst_addr == LIBPQP_BROADCAST_ADDRESS )	// reject broadcast
	{
		packet->packet_status = FREE;
		return;
	}
	
	packet->timestamp = pgpf_get_ms_timestamp( );
	packet->ttl = PQP_SEND_TTL;
	// packet->flags is already set
	packet->crc32c = pqp_calculate_crc32c( packet );
	
	packet->packet_status = TO_BE_SENT;
	packet->interface = _tx_routing_table[packet->dst_addr];
	_append_to( _tx_fifo, packet );
}

static void _fw_cloning_next_block( void )
{
	if ( cloning_target_address == 0xFF ) return;
	if ( cloning_block_count == 0 ) return;		// done
	
	pqp_packet_t * packet = _get_free_rx_packet( );
	if ( packet == NULL ) return;
	
	while ( 1 )
	{
		if ( pqpf_firmware_read( packet->payload + 8, cloning_read_address, 256 ) != 256 ) return;
		memcpy( packet->payload + 0, &cloning_magic, 4 );
		memcpy( packet->payload + 4, &cloning_write_address, 4 );
		packet->payload_length = ( 256 + 4 + 4 );
		packet->dst_addr = cloning_target_address;
		packet->src_addr = _my_address;
		packet->dst_port = MGMT_PORT_FW_WRITE;
		packet->src_port = MGMT_PORT_FW_CLONE_CTRL;
		packet->priority = PQP_PRIO_LOW;
		packet->flags = 0;
		
		cloning_block_count -= 1;
		cloning_write_address += 256;
		cloning_read_address += 256;
		
		if ( cloning_flags & PQP_FW_CLONE_FLAG_SKIP_FF_BLOCK )
		{
			bool ff = true;
			for ( int i = 0; i < 256; i++ )
			{
				if ( packet->payload[8+i] != 0xFF )
				{
					ff = false;
					break;
				}
			}
			if ( ff )
			{
				if ( cloning_block_count == 0 ) return;		// done
				continue;
			}
		}
		
		_send_packet( packet );
		break;
	}
}

static bool _start_fw_cloning( uint8_t target_address, uint8_t flags, uint16_t block_count, uint32_t magic, uint32_t write_address_start, uint32_t read_address_start )
{
	if ( target_address == _my_address )
	{
#ifdef LIBPQP_IS_BOOTLOADER
		if ( ( memcmp( &magic, PQP_WRITE_MAGIC_NOREPLY, 4 ) == 0 ) && ( pqpf_clone_local_firmware( block_count, write_address_start, read_address_start ) == 0 ) )
		{
			cloning_target_address = target_address;
			cloning_flags = flags;
			cloning_block_count = 0;
			cloning_magic = 0;
			cloning_write_address = write_address_start + block_count * 256;
			cloning_read_address = read_address_start + block_count * 256;
			return true;
		}
		else
		{
			return false;
		}
#else
		return false;
#endif
	}
	cloning_target_address = 0xFF;
	if ( target_address == 0xFF ) return false;
	if ( block_count == 0 ) return false;
	if ( write_address_start % 256 ) return false;
	if ( read_address_start % 256 ) return false;
	cloning_target_address = target_address;
	cloning_flags = flags;
	cloning_block_count = block_count;
	cloning_magic = magic;
	cloning_write_address = write_address_start;
	cloning_read_address = read_address_start;
	
	_fw_cloning_next_block( );
	return true;
}

static void _abort_fw_cloning( void )
{
	cloning_target_address = 0xFF;
	cloning_magic = 0;
	cloning_write_address = 0;
	cloning_read_address = 0;
}

static int _fw_cloning_status( uint8_t * payload )
{
	memcpy( payload + 0, &cloning_target_address, 1 );
	memcpy( payload + 1, &cloning_flags, 1 );
	memcpy( payload + 2, &cloning_block_count, 2 );
	memcpy( payload + 4, &cloning_write_address, 4 );
	memcpy( payload + 8, &cloning_read_address, 4 );
	return 12;
}

static bool _process_extensions( pqp_packet_t * packet )
{
	if ( packet == NULL ) return false;
	if ( packet->payload_length < 4 ) return false;
	uint8_t * payload = packet->payload + 4;
	uint32_t tmpu32 = 0;
	if ( memcmp( packet->payload, "INFO", 4 ) == 0 )
	{
		int len = 0;
		int part_len;
		part_len = pqpf_hw_info_length( );
		if ( ( len + 1 + part_len ) > 256 ) return false;
		payload[len] = part_len;
		len += 1;
		pqpf_copy_hw_info( payload + len );
		len += part_len;
		
		part_len = 11;
		if ( ( len + 1 + part_len ) > 256 ) return false;
		payload[len] = part_len;
		len += 1;
		memcpy( payload + len, __DATE__, part_len );
		len += part_len;
		
		part_len = 8;
		if ( ( len + 1 + part_len ) > 256 ) return false;
		payload[len] = part_len;
		len += 1;
		memcpy( payload + len, __TIME__, part_len );
		len += part_len;
		
	#ifdef MAIN_GIT_HASH
		tmpu32 = MAIN_GIT_HASH;
	#else
		#warning MAIN_GIT_HASH not defined
		tmpu32 = 0;
	#endif
		part_len = 4;
		if ( ( len + 1 + part_len ) > 256 ) return false;
		payload[len] = part_len;
		len += 1;
		memcpy( payload + len, &tmpu32, part_len );
		len += part_len;
		
	#ifdef PQP_GIT_HASH
		tmpu32 = PQP_GIT_HASH;
	#else
		#warning PQP_GIT_HASH not defined
		tmpu32 = 0;
	#endif
		part_len = 4;
		if ( ( len + 1 + part_len ) > 256 ) return false;
		payload[len] = part_len;
		len += 1;
		memcpy( payload + len, &tmpu32, part_len );
		len += part_len;
		
		part_len = 4;
		if ( ( len + 1 + part_len ) > 256 ) return false;
		payload[len] = part_len;
		len += 1;
		payload[len + 0] = 0;
		for ( int k = 0; k < BUFSIZE_TX; k++ )
		{
			if ( _pbuf[k].packet_status != FREE )
			{
				payload[len + 0] += 1;
			}
		}
		payload[len + 1] = BUFSIZE_TX;
		payload[len + 2] = 0;
		for ( int k = BUFSIZE_TX; k < BUFSIZE_TX + BUFSIZE_RX; k++ )
		{
			if ( _pbuf[k].packet_status != FREE )
			{
				payload[len + 2] += 1;
			}
		}
		payload[len + 3] = BUFSIZE_RX;
		len += part_len;
		
		for ( int i = 1; i < NUM_OF_INTERFACES; i++ )
		{
			if ( _interface_list[i].if_info != NULL )
			{
				part_len = _interface_list[i].if_info( _interface_list[i].if_private, NULL );
				if ( ( len + 1 + part_len ) > 256 ) break;
				payload[len] = part_len;
				len += 1;
				_interface_list[i].if_info( _interface_list[i].if_private, payload + len );
				len += part_len;
			}
		}
		
		packet->payload_length = ( 4 + len );
		return true;
	} else
	if ( memcmp( packet->payload, "PING", 4 ) == 0 )
	{
		return true;
	}
	return false;
}

// private-public functions

void pqp_crc32_init( uint32_t * crc )
{
	if ( crc )
	{
		*crc = 0xFFFFFFFF;
	}
}

void pqp_crc32_update( uint32_t * crc, const uint8_t data )
{
	if ( crc )
	{
		*crc = _crc_table[( (*crc) ^ data ) & 0xFFUL] ^ ( (*crc) >> 8 );
	}
}

uint32_t pqp_crc32_final( uint32_t * crc )
{
	if ( crc )
	{
		return ( (*crc) ^ 0xFFFFFFFFUL );
	}
	return 0;
}

uint32_t pqp_calculate_crc32c( pqp_packet_t * packet )
{
	uint32_t crc;
	
	pqp_crc32_init( &crc );
	pqp_crc32_update( &crc, packet->src_addr );
	pqp_crc32_update( &crc, packet->dst_addr );
	pqp_crc32_update( &crc, packet->src_port );
	pqp_crc32_update( &crc, packet->dst_port );
	
	for ( uint32_t i = 0; i < packet->payload_length; i++ )
	{
		pqp_crc32_update( &crc, packet->payload[i] );
	}
	
	return pqp_crc32_final( &crc );
}

// public functions

void pqp_init( uint8_t my_address )
{
	_my_address = my_address;

#ifndef LIBPQP_USED_IN_GND_SW
	if (_my_address < 2)
	{
		_my_address = LIBPQP_BROADCAST_ADDRESS;
	}
#endif
	if (_my_address == LIBPQP_BROADCAST_ADDRESS )
	{
		while ( 1 )
		{
			;
		}
	}
	
	_bootloader_latch = pqpf_bootloader_latch_reboot( );
	
	for ( int k = 0; k < BUFSIZE_TX + BUFSIZE_RX; k++ )
	{
		_pbuf[k].packet_status = FREE;
		_tx_fifo[k] = NULL;
		_rx_fifo[k] = NULL;
	}
	
	for ( int i = 0; i < NUM_OF_INTERFACES; i++ )
	{
		memset( _interface_list + i, 0, sizeof( pqp_interface_t ) );
		((pqp_interface_initializer_t)_interface_initializer_list[i])( _interface_list + i );
	}
	
	for ( int i = 0; i < 256; i++ )
	{
		_tx_routing_table[i] = 1;
	}
	
	memset( _progmap, 0, sizeof(_progmap));
	
	pqpf_multicore_init_lock( );
}

void pqp_process( void )
{
	pqpf_multicore_lock( );
	// IF Process
	for ( int i = 1; i < NUM_OF_INTERFACES; i++ )
	{
		_interface_list[i].if_process( _interface_list[i].if_private );
	}
	// IF RX
	for ( int p = 0; p < PQP_PRIO_LEVELS; p++)
	{
		for ( int i = 1; i < NUM_OF_INTERFACES; i++ )
		{
			if ( _interface_list[i].if_rx_available( _interface_list[i].if_private, p ) )
			{
				pqp_packet_t * rx_packet = _get_free_rx_packet( );
				if ( rx_packet == NULL ) break;
				if ( _interface_list[i].if_rx_get_packet( _interface_list[i].if_private, p, rx_packet ) )
				{
					// fill missing parameters
					rx_packet->interface = i;
					rx_packet->timestamp = pgpf_get_ms_timestamp( );
					rx_packet->packet_status = RECEIVED;
					
					_process_received_packet( rx_packet );
				}
				else
				{
					// failed to get packet (might be CRC failure)
					rx_packet->packet_status = FREE;
				}
			}
		}
	}
	// IF TX
	for ( int p = 0; p < PQP_PRIO_LEVELS; p++)
	{
		for ( int k = 0; k < BUFSIZE_TX + BUFSIZE_RX; k++ )
		{
			if ( _tx_fifo[k] == NULL ) break;
			if ( _tx_fifo[k]->priority == p )
			{
				int i = _tx_fifo[k]->interface;
				if ( i < NUM_OF_INTERFACES )
				{
					if ( _interface_list[i].if_tx_enqueue_packet( _interface_list[i].if_private, _tx_fifo[k] ) )
					{
						_delete_element_from( _tx_fifo, k );
					}
					else
					{
						// if cant enqueue long enough, drop the packet
						if ( ( pgpf_get_ms_timestamp( ) - _tx_fifo[k]->timestamp ) > PQP_PACKET_DROP_AFTER_MS )
						{
							_tx_fifo[k]->packet_status = FREE;
							_delete_element_from( _tx_fifo, k );
						}
					}
				}
				else
				{
					// wrong interface
					_tx_fifo[k]->packet_status = FREE;
					_delete_element_from( _tx_fifo, k );
				}
			}
		}
	}
	// Process management packets
	int k=0;
	while ( ( k < BUFSIZE_TX + BUFSIZE_RX ) && ( _rx_fifo[k] != NULL) )
	{
		pqp_packet_t * packet = _rx_fifo[k];
		switch ( packet->dst_port )
		{
			case MGMT_PORT_PING:
					_swap_addresses_and_ports( packet );
				  #ifdef LIBPQP_IS_BOOTLOADER
					packet->payload[0] = 0xB0;
				  #else
					packet->payload[0] = 0xF3;
				  #endif
					uint32_t uptime = pgpf_get_uptime( );
					memcpy( packet->payload + 1, &uptime, 4 );
					packet->payload_length = 5;
					_send_packet( packet );
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_TRANSFER_TEST:
					_swap_addresses_and_ports( packet );
					_send_packet( packet );
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_REBOOT:
					if ( packet->payload_length == 4 )
					{
						if ( memcmp( packet->payload + 0, PQP_REBOOT_MAGIC, 4 ) == 0 )
						{
							while( 1 )
							{
								pqpf_reboot( 0 );	// no bootloader latch
							}
						}
					}
					packet->packet_status = FREE;
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_BOOTLOADER:
					if ( packet->payload_length == 4 )
					{
						if ( memcmp( packet->payload + 0, PQP_BOOTLOADER_LATCH_MAGIC, 4 ) == 0 )
						{
						  #ifdef LIBPQP_IS_BOOTLOADER
							_swap_addresses_and_ports( packet );
							packet->payload_length = 0;
							_send_packet( packet );
							packet = NULL;
							_bootloader_latch = PQP_BLDR_CODE_BOOTLOADER_LATCH;
						  #else
							while( 1 )
							{
								pqpf_reboot( PQP_BLDR_CODE_BOOTLOADER_LATCH );		// indicate bootloader latch if possible
							}
						  #endif
						} else
						if ( memcmp( packet->payload + 0, PQP_BOOTLOADER_FWUPD_MAGIC, 4 ) == 0 )
						{
							while( 1 )
							{
								pqpf_reboot( PQP_BLDR_CODE_FIRMWARE_UPDATE );		// indicate bootloader fw update if possible
							}
						}
					}
					if ( packet != NULL )
					{
						packet->packet_status = FREE;
					}
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_CHECKSUM:
					if ( packet->payload_length >= 1 )
					{
						if ( packet->payload[0] == CUE_CHECKSUM_FW )
						{
							uint32_t checksum;
							_swap_addresses_and_ports( packet );
							packet->payload[0] = CUE_CHECKSUM_FW;
							checksum = pqpf_get_bootloader_checksum();
							memcpy( packet->payload + 1, &checksum, 4 );
							checksum = pqpf_get_firmware_checksum();
							memcpy( packet->payload + 5, &checksum, 4 );
							checksum = pqpf_get_firmware_stored_checksum();
							memcpy( packet->payload + 9, &checksum, 4 );
							checksum = pqpf_get_firmware_length();
							memcpy( packet->payload + 13, &checksum, 4 );
							packet->payload_length = 17;
							_send_packet( packet );
							packet = NULL;
						} else
						if ( packet->payload[0] == CUE_CHECKSUM_SECTION )
						{
							if ( packet->payload_length == 9 )
							{
								uint32_t addr, length, checksum;
								memcpy( &addr, packet->payload + 1, 4 );
								memcpy( &length, packet->payload + 5, 4 );
								checksum = pqpf_get_section_checksum( addr, length );
								_swap_addresses_and_ports( packet );
								packet->payload[0] = CUE_CHECKSUM_SECTION;
								memcpy( packet->payload + 1, &checksum, 4 );
								memcpy( packet->payload + 5, &addr, 4 );
								memcpy( packet->payload + 9, &length, 4 );
								packet->payload_length = 13;
								_send_packet( packet );
								packet = NULL;
							}
						} else
						if ( packet->payload[0] == CUE_ERASED_SECTION )
						{
							if ( packet->payload_length == 9 )
							{
								uint32_t addr, length;//checksum;
								memcpy( &addr, packet->payload + 1, 4 );
								memcpy( &length, packet->payload + 5, 4 );
								_swap_addresses_and_ports( packet );
								packet->payload[0] = CUE_ERASED_SECTION;
								memcpy( packet->payload + 1, &addr, 4 );
								memcpy( packet->payload + 5, &length, 4 );
								packet->payload_length = 9;
								while ( length > 0 )
								{
									packet->payload[packet->payload_length] = 0;
									for ( int i = 0; i < 8; i++ )
									{
										if ( pqpf_get_section_erased( addr, 256 ) )
										{
											packet->payload[packet->payload_length] |= ( 1 << i );
										}
										addr += 256;
										if ( length <= 256)
										{
											length = 0;
											break;
										}
										length -= 256;
									}
									packet->payload_length += 1;
									if ( packet->payload_length >= 256 + 9 ) break;
								}
								_send_packet( packet );
								packet = NULL;
							}
						} else
						if ( packet->payload[0] == CUE_CHECKSUM_ROUTING_TABLE )
						{
							uint32_t checksum;
							_swap_addresses_and_ports( packet );
							packet->payload[0] = CUE_CHECKSUM_ROUTING_TABLE;
							checksum = pqpf_get_routing_table_checksum();
							memcpy( packet->payload + 1, &checksum, 4 );
							packet->payload_length = 5;
							_send_packet( packet );
							packet = NULL;
						}
					}
					if ( packet != NULL )
					{
						packet->packet_status = FREE;
					}
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_RUN:
					if ( packet->payload_length == 0 )
					{
				  #ifdef LIBPQP_IS_BOOTLOADER
					if ( pqpf_check_valid_firmware( ) )
					{
						for ( int i = 1; i < NUM_OF_INTERFACES; i++ )
						{
							_interface_list[i].if_deinit( _interface_list[i].if_private );
						}
						pqpf_start_firmware( );
						// this should be unreachable:
						while(1)
						{
							;
						}
					}
				  #endif
					}
					packet->packet_status = FREE;
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_ERASE:
					if ( packet->payload_length >= 1 )
					{
						if ( packet->payload[0] == CUE_ERASE_FIRMWARE )
						{
							if ( packet->payload_length == 5 )
							{
								if ( memcmp( packet->payload + 1, PQP_ERASE_MAGIC, 4 ) == 0 )
								{
									_swap_addresses_and_ports( packet );
									packet->payload[0] = CUE_ERASE_FIRMWARE;
								  #ifdef LIBPQP_IS_BOOTLOADER
									if ( pqpf_firmware_erase( ) < 0)
									{
										packet->payload[1] = 0xEE;
									}
									else
									{
										packet->payload[1] = 0x0C;
									}
								  #else
									packet->payload[1] = 0x00;
								  #endif
									packet->payload_length = 2;
									_send_packet( packet );
									packet = NULL;
								}
							}
						} else
						if ( packet->payload[0] == CUE_ERASE_FW_ALL )
						{
							if ( packet->payload_length == 5 )
							{
								if ( memcmp( packet->payload + 1, PQP_ERASE_ALL_MAGIC, 4 ) == 0 )
								{
									_swap_addresses_and_ports( packet );
									packet->payload[0] = CUE_ERASE_FW_ALL;
								  #ifdef LIBPQP_IS_BOOTLOADER
									if ( pqpf_erase_all( ) < 0)
									{
										packet->payload[1] = 0xEE;
									}
									else
									{
										packet->payload[1] = 0x0C;
									}
								  #else
									packet->payload[1] = 0x00;
								  #endif
									packet->payload_length = 2;
									_send_packet( packet );
									packet = NULL;
								}
							}
						} else
						if ( packet->payload[0] == CUE_ERASE_SECTION )
						{
							if ( packet->payload_length == 13 )
							{
								if ( memcmp( packet->payload + 1, PQP_ERASE_SECTION_MAGIC, 4 ) == 0 )
								{
									_swap_addresses_and_ports( packet );
									uint32_t erase_addr, erase_length;
									memcpy( &erase_addr, packet->payload + 5, 4 );
									memcpy( &erase_length, packet->payload + 9, 4 );
									packet->payload[0] = CUE_ERASE_SECTION;
									if ( pqpf_firmware_erase_section( erase_addr, erase_length ) < 0)
									{
										packet->payload[1] = 0xEE;
									}
									else
									{
										packet->payload[1] = 0x0C;
									}
									packet->payload_length = 2;
									_send_packet( packet );
									packet = NULL;
								}
							}
						}
					}
					if ( packet != NULL )
					{
						packet->packet_status = FREE;
					}
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_CLEAR_MAP:
					if ( packet->payload_length == 0 )
					{
						memset( _progmap, 0, sizeof(_progmap) );
						_swap_addresses_and_ports( packet );
						packet->payload_length = 0;
						_send_packet( packet );
						packet = NULL;
					}
					if ( packet != NULL )
					{
						packet->packet_status = FREE;
					}
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_GET_MAP:
					if ( packet->payload_length == 0 )
					{
						_swap_addresses_and_ports( packet );
						memcpy( packet->payload, _progmap, sizeof(_progmap) );
						packet->payload_length = sizeof(_progmap);
						_send_packet( packet );
						packet = NULL;
					}
					if ( packet != NULL )
					{
						packet->packet_status = FREE;
					}
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_FW_WRITE:
					if ( packet->payload_length == (8+256) )
					{
						if ( ( memcmp( packet->payload + 0, PQP_WRITE_MAGIC, 4 ) == 0 ) || ( memcmp( packet->payload + 0, PQP_WRITE_MAGIC_NOREPLY, 4 ) == 0 ) )
						{
							bool reply = ( memcmp( packet->payload + 0, PQP_WRITE_MAGIC, 4 ) == 0 );
							uint32_t address;
							memcpy( &address, packet->payload + 4, 4);
							if ( ( address & 0xFF ) == 0 )	// must be page boundary
							{
								if ( pqpf_firmware_write( packet->payload + 8, address, 256 ) < 0 )
								{
									packet->payload[0] = 0xEE;
								}
								else
								{
									packet->payload[0] = 0x0C;
									uint32_t page = address / 256;
									_progmap[ ( page / 8 ) % 64 ] |= ( 1 << ( page % 8) );
								}
								if ( reply )
								{
									packet->payload_length = 1;
									_swap_addresses_and_ports( packet );
									_send_packet( packet );
									packet = NULL;
								}
							}
						}
					}
					if ( packet != NULL )
					{
						packet->packet_status = FREE;
					}
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_FW_CLONE:
					if ( packet->payload_length == 16 )
					{
						uint8_t target_address, flags;
						uint16_t block_count;
						uint32_t magic;
						uint32_t write_address_start;
						uint32_t read_address_start;
						memcpy( &target_address, packet->payload + 0, 1);
						memcpy( &flags, packet->payload + 1, 1);
						memcpy( &block_count, packet->payload + 2, 2);
						memcpy( &magic, packet->payload + 4, 4);
						memcpy( &write_address_start, packet->payload + 8, 4);
						memcpy( &read_address_start, packet->payload + 12, 4);
						_swap_addresses_and_ports( packet );
						packet->payload[0] = ( _start_fw_cloning( target_address, flags, block_count, magic, write_address_start, read_address_start ) ? 0x0C : 0xEE );
						packet->payload_length = 1;
						_send_packet( packet );
						packet = NULL;
					}
					if ( packet != NULL )
					{
						packet->packet_status = FREE;
					}
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_FW_CLONE_CTRL:
					if ( packet->payload_length == 1 )
					{
						if ( ( packet->payload[0] == 0xAB ) || ( packet->payload[0] == 0x57 ) )
						{
							if ( packet->payload[0] == 0xAB )
							{
								_abort_fw_cloning( );
							}
							packet->payload_length = _fw_cloning_status( packet->payload );
							_swap_addresses_and_ports( packet );
							_send_packet( packet );
							packet = NULL;
						} else
						if ( packet->payload[0] == 0x0C )	// cloning process reply
						{
							if ( packet->src_addr == cloning_target_address )
							{
								_fw_cloning_next_block( );
							}
						}
					} else
					if ( packet->payload_length == 3 )
					{
						if ( packet->payload[0] == 0x1F )
						{
							pqp_set_outgoing_interface( packet->payload[1], packet->payload[2] );
							_swap_addresses_and_ports( packet );
							packet->payload[0] = 0x0C;
							packet->payload_length = 1;
							_send_packet( packet );
							packet = NULL;
						}
					}
					if ( packet != NULL )
					{
						packet->packet_status = FREE;
					}
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_CLEAR_ROUTING_TABLE:
					if ( packet->payload_length == 4 )
					{
						if ( memcmp( packet->payload, PQP_CLEAR_ROUTING_TABLE_MAGIC, 4 ) == 0 )
						{
							_swap_addresses_and_ports( packet );
							if ( pqpf_clear_routing_table( ) < 0 )
							{
								packet->payload[0] = 0xEE;
							}
							else
							{
								packet->payload[0] = 0x0C;
							}
							packet->payload_length = 1;
							_send_packet( packet );
							packet = NULL;
						}
					}
					if ( packet != NULL )
					{
						packet->packet_status = FREE;
					}
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_ROUTING_TABLE_ENTRY:
					if ( packet->payload_length >= 3 )
					{
						if ( ( packet->payload_length % 3 ) == 0 )
						{
							int entries = packet->payload_length / 3;
							_swap_addresses_and_ports( packet );
							for ( int i = 0; i < entries; i++ )
							{
								uint8_t src_address = packet->payload[3*i + 0];
								uint8_t dst_address = packet->payload[3*i + 1];
								uint8_t data = packet->payload[3*i + 2];
								if ( pqpf_write_routing_table( src_address, dst_address, data ) < 0 )
								{
									packet->payload[0] = 0xEE;
									break;
								}
								else
								{
									packet->payload[0] = 0x0C;
								}
							}
							packet->payload_length = 1;
							_send_packet( packet );
							packet = NULL;
						}
					}
					if ( packet != NULL )
					{
						packet->packet_status = FREE;
					}
					_delete_element_from( _rx_fifo, k );
				break;
			case MGMT_PORT_EXTENSIONS:
					if ( packet->payload_length >= 4 )
					{
						if ( _process_extensions( packet ) )
						{
							_swap_addresses_and_ports( packet );
							_send_packet( packet );
							packet = NULL;
						}
					}
					if ( packet != NULL )
					{
						packet->packet_status = FREE;
					}
					_delete_element_from( _rx_fifo, k );
				break;
			//case MGMT_PORT_TTY:		// handled in the app
			//		packet->packet_status = FREE;
			//		_delete_element_from( _rx_fifo, k );
			//	break;
			default:		// skip non management packets
					k++;
				break;
		}
	}
	pqpf_multicore_unlock( );
}

uint8_t pqp_get_my_address( void )
{
	return _my_address;
}

// Zero copy interface
bool pqp_received_packet_available( void )
{
	return ( _rx_fifo[0] != NULL );
}

int pqp_take_received_packet( void )
{
	pqpf_multicore_lock( );
	if ( !pqp_received_packet_available( ) )
	{
		pqpf_multicore_unlock( );
		return -1;
	}
	pqp_packet_t * packet = _rx_fifo[0];
	_delete_element_from( _rx_fifo, 0 );
	int pid = _packet_to_pid( packet );
	if ( pid < 0 )
	{
		packet->packet_status = FREE;
	}
	else
	{
		packet->packet_status = USERSPACE;
	}
	pqpf_multicore_unlock( );
	return pid;
}

uint8_t pqp_get_dst_addr( int pid )
{
	if ( pid < 0 ) pid = 0;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) pid = ( BUFSIZE_TX + BUFSIZE_RX - 1 );
	
	return _pbuf[pid].dst_addr;
}

uint8_t pqp_get_src_addr( int pid )
{
	if ( pid < 0 ) pid = 0;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) pid = ( BUFSIZE_TX + BUFSIZE_RX - 1 );
	
	return _pbuf[pid].src_addr;
}

uint8_t pqp_get_dst_port( int pid )
{
	if ( pid < 0 ) pid = 0;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) pid = ( BUFSIZE_TX + BUFSIZE_RX - 1 );
	
	return _pbuf[pid].dst_port;
}

uint8_t pqp_get_src_port( int pid )
{
	if ( pid < 0 ) pid = 0;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) pid = ( BUFSIZE_TX + BUFSIZE_RX - 1 );
	
	return _pbuf[pid].src_port;
}

pqp_prio_t pqp_get_priority( int pid )
{
	if ( pid < 0 ) pid = 0;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) pid = ( BUFSIZE_TX + BUFSIZE_RX - 1 );
	
	return _pbuf[pid].priority;
}

uint8_t pqp_get_flags( int pid )
{
	if ( pid < 0 ) pid = 0;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) pid = ( BUFSIZE_TX + BUFSIZE_RX - 1 );
	
	return _pbuf[pid].flags;
}

int pqp_get_payload_length( int pid )
{
	if ( pid < 0 ) pid = 0;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) pid = ( BUFSIZE_TX + BUFSIZE_RX - 1 );
	
	return _pbuf[pid].payload_length;
}

void pqp_release_received_packet( int pid )
{
	if ( pid < 0 ) return;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) return;
	if ( _pbuf[pid].packet_status != USERSPACE ) return;
	
	_pbuf[pid].packet_status = FREE;
}

int pqp_request_blank_packet( void )
{
	pqpf_multicore_lock( );
	for ( int k = 0; k < BUFSIZE_TX; k++ )
	{
		if ( _pbuf[k].packet_status == FREE )
		{
			_pbuf[k].packet_status = USERSPACE;
			_pbuf[k].flags = 0;
			pqpf_multicore_unlock( );
			return k;
		}
	}
	pqpf_multicore_unlock( );
	return -1;
}

void pqp_set_payload_length( int pid, int len )
{
	if ( pid < 0 ) return;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) return;
	if ( _pbuf[pid].packet_status != USERSPACE ) return;
	
	if ( len > PQP_MAX_PAYLOAD ) len = PQP_MAX_PAYLOAD;
	_pbuf[pid].payload_length = len;
}

void pqp_set_flags( int pid, uint8_t flags )
{
	if ( pid < 0 ) return;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) return;
	if ( _pbuf[pid].packet_status != USERSPACE ) return;
	
	flags &= 0x03;
	_pbuf[pid].flags = flags;
}

void pqp_send_packet( int pid, uint8_t dst_addr, uint8_t dst_port, uint8_t src_port, pqp_prio_t priority )
{
	if ( pid < 0 ) return;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) return;
	if ( _pbuf[pid].packet_status != USERSPACE ) return;
	
	_pbuf[pid].dst_addr = dst_addr;
	_pbuf[pid].src_addr = _my_address;
	_pbuf[pid].dst_port = dst_port;
	_pbuf[pid].src_port = src_port;
	_pbuf[pid].priority = priority;
	pqpf_multicore_lock( );
	_send_packet( _pbuf + pid );
	pqpf_multicore_unlock( );
}

bool pqp_packet_sent( int pid )
{
	if ( pid < 0 ) return false;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) return false;
	
	return ( _pbuf[pid].packet_status < TO_BE_SENT );
}

uint8_t * pqp_get_packet_payload_pointer( int pid )
{
	if ( pid < 0 ) pid = 0;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) pid = ( BUFSIZE_TX + BUFSIZE_RX - 1 );
	
	return _pbuf[pid].payload;
}


// Simple interface
int pqp_recv( uint8_t * data, uint8_t * dst_addr , uint8_t * src_addr, uint8_t * dst_port, uint8_t * src_port, pqp_prio_t * priority )
{
	if ( !data ) return -1;
	if ( !pqp_received_packet_available() ) return -1;
	
	int pid = pqp_take_received_packet();
	if ( pid < 0 ) return -1;
	
	int len = pqp_get_payload_length( pid );
	
	if ( dst_addr )
	{
		*dst_addr = pqp_get_dst_addr( pid );
	}
	if ( src_addr )
	{
		*src_addr = pqp_get_src_addr( pid );
	}
	if ( dst_port )
	{
		*dst_port = pqp_get_dst_port( pid );
	}
	if ( src_port )
	{
		*src_port = pqp_get_src_port( pid );
	}
	if ( priority )
	{
		*priority = pqp_get_priority( pid );
	}
	
	if ( len > PQP_MAX_PAYLOAD )
	{
		len = -1;
	}
	
	if ( len > 0 )
	{
		memcpy( data, pqp_get_packet_payload_pointer( pid ), len );
	}
	
	pqp_release_received_packet( pid );
	
	return len;
}

int pqp_send( uint8_t * data, int data_len, uint8_t dst_addr, uint8_t dst_port, uint8_t src_port, pqp_prio_t priority )
{
	if ( !data ) return -1;
	if ( data_len < 0 ) return -1;
	if ( data_len > PQP_MAX_PAYLOAD ) return -1;
	
	int pid = pqp_request_blank_packet();
	if ( pid < 0 ) return -1;
	
	pqp_set_payload_length( pid, data_len );
	if ( data_len > 0 )
	{
		memcpy( pqp_get_packet_payload_pointer( pid ), data, data_len );
	}
	
	pqp_send_packet( pid, dst_addr, dst_port, src_port, priority );
	
	return data_len;
}



// OBC only
void pqp_set_outgoing_interface( uint8_t dst_addr, int interface )
{
	if (interface < 0)
	{
		_tx_routing_table[dst_addr] = 1;
		return;
	}
	_tx_routing_table[dst_addr] = routing_interface_translator_table[interface & 0xFF];
}

// COM only
int pqp_raw_pop( uint8_t * data )			// returns the number of bytes of the raw packet, or -1 on error / no packet
{
#ifdef LIBPQP_HAS_RAW
	return pqp_if_raw_get_packet( data );
#else
	return -1;
#endif
}

int pqp_raw_push( uint8_t * data, int data_len )	// returns 0 if raw packet taken, or -1 if cannot take at the moment (try later)
{
#ifdef LIBPQP_HAS_RAW
	return pqp_if_raw_add_packet( data, data_len );
#else
	return -1;
#endif
}
// Bootloader interface
uint32_t pqp_bootloader_latch_code( void )
{
	return _bootloader_latch;
}


bool pqp_destination_irrelevant( uint8_t src_addr, uint8_t dst_addr )
{
	if ( src_addr == LIBPQP_BROADCAST_ADDRESS ) return false;
	if ( dst_addr == _my_address ) return false;
	if ( dst_addr == LIBPQP_BROADCAST_ADDRESS ) return false;
	
	uint8_t fwd_if = routing_interface_translator_table[pqpf_read_routing_table( src_addr, dst_addr ) & 0xFF];
	if ( ( fwd_if == 0 ) || ( fwd_if >= NUM_OF_INTERFACES ) )
	{
		return true;
	}
	
	return false;
}

void pqp_volatile_copy( volatile void * destination, const volatile void * source, size_t num )
{
	volatile uint8_t * dst = ( volatile uint8_t * )destination;
	const volatile uint8_t * src = ( const volatile uint8_t * )source;
	
	while ( num-- )
	{
		*dst++ = *src++;
	}
}

bool pqp_volatile_compare( volatile void * destination, const volatile void * source, size_t num )
{
	volatile uint8_t * dst = ( volatile uint8_t * )destination;
	const volatile uint8_t * src = ( const volatile uint8_t * )source;
	
	while ( num-- )
	{
		if ( *dst++ != *src++ ) return false;
	}
	return true;
}

#ifdef LIBPQP_TWEAK_TX_PACKET_INJECTION
 #ifdef LIBPQP_IS_BOOTLOADER
 #error do not use in bootloader mode
 #endif
void pqp_send_packet_injected( int pid, uint8_t dst_addr, uint8_t dst_port, uint8_t src_addr, uint8_t src_port, pqp_prio_t priority, uint8_t flags, uint8_t ttl, uint8_t interface )
{

	if ( pid < 0 ) return;
	if ( pid >= BUFSIZE_TX + BUFSIZE_RX ) return;
	if ( _pbuf[pid].packet_status != USERSPACE ) return;
	
	_pbuf[pid].dst_addr = dst_addr;
	_pbuf[pid].src_addr = src_addr;
	_pbuf[pid].dst_port = dst_port;
	_pbuf[pid].src_port = src_port;
	_pbuf[pid].priority = priority;
	pqpf_multicore_lock( );
	_pbuf[pid].timestamp = pgpf_get_ms_timestamp( );
	_pbuf[pid].ttl = ttl;
	_pbuf[pid].flags = flags;
	_pbuf[pid].crc32c = pqp_calculate_crc32c( _pbuf + pid );
	_pbuf[pid].packet_status = TO_BE_SENT;
	_pbuf[pid].interface = interface;
	_append_to( _tx_fifo, _pbuf + pid );
	pqpf_multicore_unlock( );
}
#endif

