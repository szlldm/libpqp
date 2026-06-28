/* Copyright (C) 2026 szlldm
 * 
 * STDIO interface.
 * This file is part of libpqp.
 * 
 * LibPQP is dual-licensed: you may use it under the terms of the
 * GNU Affero General Public License version 3 (AGPLv3), or alternatively
 * under a commercial license.
 * You should have received a copy of the AGPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 * For commercial licensing, please contact.
 */

#include "libpqp_if_stdioif.h"
#include <stddef.h>
#include <unistd.h>
#include <string.h>

// private

typedef struct
{
	int 	( * stdio_getchar )( void );
	void 	( * stdio_sendchar )( uint8_t data );
	void 	( * stdio_flush )( void ); 

	uint8_t rxbuf[1024];
	int rxbuf_len;
	bool rxbuf_escape;
	bool rx_packet_valid;
	pqp_packet_t rx_packet;
	pqp_packet_t * tx_packet;

	uint8_t txbuf[1024];
	int txbuf_len;
	int txbuf_ptr;
	uint32_t stat_tx_cntr, stat_rx_cntr;
} stdioif_t;


static void add_tx_char(stdioif_t * pp, uint8_t data)
{
	if (data == '\n')
	{
		pp->txbuf[pp->txbuf_len] = 0xFE;
		pp->txbuf_len++;
		pp->txbuf[pp->txbuf_len] = 0xFD;
		pp->txbuf_len++;
	} else
	if (data == 0xFE)
	{
		pp->txbuf[pp->txbuf_len] = 0xFE;
		pp->txbuf_len++;
		pp->txbuf[pp->txbuf_len] = 0xFC;
		pp->txbuf_len++;
	}
	else
	{
		pp->txbuf[pp->txbuf_len] = data;
		pp->txbuf_len++;
	}
}


static void deinit( void * if_private )
{
}

static void process( void * if_private )
{
	stdioif_t * pp = (stdioif_t*)if_private;
	int rbuf;
	while ( (rbuf = pp->stdio_getchar()) >= 0)
	{
		if ( pp->rxbuf_escape )
		{
			if (pp->rxbuf_len<1024)
			{
				if ((uint8_t)rbuf==0xFD)
				{
					pp->rxbuf[pp->rxbuf_len] = '\n';
					pp->rxbuf_len++;
				} else
				if ((uint8_t)rbuf==0xFC)
				{
					pp->rxbuf[pp->rxbuf_len] = 0xFE;
					pp->rxbuf_len++;
				}
			}
			pp->rxbuf_escape = false;
		}
		else
		{
			if ((uint8_t)rbuf=='\n')
			{
				if (pp->rxbuf_len >= 10 && !pp->rx_packet_valid)
				{
					if (((pp->rxbuf[0]&0x3F) == 0x3E) && ((pp->rxbuf[1]&0x30) == 0x20))	// preamble && hlen
					{
						pp->rx_packet.priority = ((pp->rxbuf[0] >> 6)&0x03);
						pp->rx_packet.flags = ((pp->rxbuf[1] >> 6)&0x03);
						pp->rx_packet.ttl = (pp->rxbuf[1]&0x0F);
						pp->rx_packet.src_addr = pp->rxbuf[2];
						pp->rx_packet.dst_addr = pp->rxbuf[3];
						pp->rx_packet.src_port = pp->rxbuf[4];
						pp->rx_packet.dst_port = pp->rxbuf[5];
						for ( int i=6; i<pp->rxbuf_len-4; i++)
						{
							if ((i-6) > PQP_MAX_PAYLOAD) break;
							pp->rx_packet.payload[i-6] = pp->rxbuf[i];
						}
						memcpy( &pp->rx_packet.crc32c, pp->rxbuf+pp->rxbuf_len-4, 4);
						pp->rx_packet.payload_length = pp->rxbuf_len-10;
						
						if ( pqp_calculate_crc32c( &pp->rx_packet ) == pp->rx_packet.crc32c )
						{
							pp->rx_packet_valid = true;
							pp->stat_rx_cntr += 1;
						}
						pp->rxbuf_len = 0;
						break;
					}
				}
				pp->rxbuf_len = 0;
			} else
			if ((uint8_t)rbuf==0xFE)
			{
				pp->rxbuf_escape = true;
			} else
			{
				if (pp->rxbuf_len<1024)
				{
					pp->rxbuf[pp->rxbuf_len] = (uint8_t)rbuf;
					pp->rxbuf_len++;
				}
			}
		}
	}
	
	if (pp->tx_packet)
	{
		if ( pp->txbuf_len == 0)
		{
			uint8_t t,crc[4];
			t = pp->tx_packet->priority;
			t <<= 6;
			t |= 0x3E;
			add_tx_char(pp, t);
			t = pp->tx_packet->flags;
			t <<= 6;
			t |= 0x20;
			t |= (pp->tx_packet->ttl & 0x0F);
			add_tx_char(pp, t);
			add_tx_char(pp, pp->tx_packet->src_addr);
			add_tx_char(pp, pp->tx_packet->dst_addr);
			add_tx_char(pp, pp->tx_packet->src_port);
			add_tx_char(pp, pp->tx_packet->dst_port);
			for ( int i=0; i<pp->tx_packet->payload_length; i++)
			{
				add_tx_char(pp, pp->tx_packet->payload[i]);
			}
			memcpy( &crc, &pp->tx_packet->crc32c, 4);
			add_tx_char(pp, crc[0]);
			add_tx_char(pp, crc[1]);
			add_tx_char(pp, crc[2]);
			add_tx_char(pp, crc[3]);
			pp->txbuf[pp->txbuf_len] = '\n';
			pp->txbuf_len++;
			pp->txbuf_ptr = 0;
		}
		
		if ( pp->txbuf_len > 0)
		{
			for ( int i = 0; i < pp->txbuf_len; i++ )
			{
				pp->stdio_sendchar(pp->txbuf[i]);
			}
			pp->txbuf_len = 0;
			pp->tx_packet->packet_status = FREE;
			pp->tx_packet = NULL;
			pp->stdio_flush();
			pp->stat_tx_cntr += 1;

		}
	}
}

static bool rx_available( void * if_private, pqp_prio_t priority )
{
	stdioif_t * pp = (stdioif_t*)if_private;
	if (!pp->rx_packet_valid) return false;
	if (pp->rx_packet.priority != priority) return false;
	return true;
}

static bool rx_get_packet( void * if_private, pqp_prio_t priority, pqp_packet_t * packet )
{
	stdioif_t * pp = (stdioif_t*)if_private;
	if (!pp->rx_packet_valid) return false;
	if (pp->rx_packet.priority != priority) return false;
	memcpy(packet, &pp->rx_packet, sizeof(pp->rx_packet));
	pp->rx_packet_valid = false;
	return true;
}

static bool tx_enqueue_packet( void * if_private, pqp_packet_t * packet )
{
	stdioif_t * pp = (stdioif_t*)if_private;
	if ( pp->tx_packet ) return false;
	pp->tx_packet = packet;
	return true;
}

static uint8_t get_info( void * if_private, uint8_t * data )	// data can be NULL, in this case the same length must be returned
{
	if ( if_private == NULL ) return 0;
	stdioif_t * pp = (stdioif_t*)if_private;
	uint8_t len = 0;
	if ( data != NULL )
	{
		memcpy( data + len, "USB", 3 );
	}
	len += 3;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( pp->stat_tx_cntr ), 4 );
	}
	len += 4;
	
	if ( data != NULL )
	{
		memcpy( data + len, &( pp->stat_rx_cntr ), 4 );
	}
	len += 4;
	
	return len;
}

// public
#if defined( LIBPQP_HAS_STDIOIF )
static stdioif_t stdioif;

void PQP_IF_OBJ_INITIALIZER_STDIOIF( pqp_interface_t * pqp_interface )
{
	memset(&stdioif, 0, sizeof(stdioif));
	stdioif.stdio_getchar = pqpf_stdio_if_getchar;
	stdioif.stdio_sendchar = pqpf_stdio_if_sendchar;
	stdioif.stdio_flush = pqpf_stdio_if_flush;
	stdioif.rxbuf_len = 0;
	stdioif.rxbuf_escape = false;
	stdioif.rx_packet_valid = false;
	stdioif.txbuf_len = 0;
	
	pqp_interface->if_private = &stdioif;
	pqp_interface->if_deinit = deinit;
	pqp_interface->if_process = process;
	pqp_interface->if_rx_available = rx_available;
	pqp_interface->if_rx_get_packet = rx_get_packet;
	pqp_interface->if_tx_enqueue_packet = tx_enqueue_packet;
	pqp_interface->if_info = get_info;
	
	pqpf_can_if1_init();
};
#endif

