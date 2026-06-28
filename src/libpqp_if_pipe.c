/* Copyright (C) 2026 szlldm
 * 
 * Pipe interface.
 * This file is part of libpqp.
 * 
 * LibPQP is dual-licensed: you may use it under the terms of the
 * GNU Affero General Public License version 3 (AGPLv3), or alternatively
 * under a commercial license.
 * You should have received a copy of the AGPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 * For commercial licensing, please contact.
 */

#include "libpqp_if_pipe.h"
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

// private

typedef struct
{
	FILE * fs_in;
	FILE * fs_out;

	uint8_t rxbuf[1024];
	int rxbuf_len;
	bool rxbuf_escape;
	bool rx_packet_valid;
	pqp_packet_t rx_packet;
	pqp_packet_t * tx_packet;

	uint8_t txbuf[1024];
	int txbuf_len;
	int txbuf_ptr;
} pipe_priv_t;

static pipe_priv_t pp1;
static pipe_priv_t pp2;

static void add_tx_char(pipe_priv_t * pp, uint8_t data)
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
	pipe_priv_t * pp = (pipe_priv_t*)if_private;
	char rbuf[2];
	while (fread(rbuf, 1, 1, pp->fs_in) > 0)
	//while(read(fileno(pp->fs_in), rbuf, 1)>0)
	{
		if ( pp->rxbuf_escape )
		{
			if (pp->rxbuf_len<1024)
			{
				if ((uint8_t)rbuf[0]==0xFD)
				{
					pp->rxbuf[pp->rxbuf_len] = '\n';
					pp->rxbuf_len++;
				} else
				if ((uint8_t)rbuf[0]==0xFC)
				{
					pp->rxbuf[pp->rxbuf_len] = 0xFE;
					pp->rxbuf_len++;
				}
			}
			pp->rxbuf_escape = false;
		}
		else
		{
			if (rbuf[0]=='\n')
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
						}
						pp->rxbuf_len = 0;
						break;
					}
				}
				pp->rxbuf_len = 0;
			} else
			if ((uint8_t)rbuf[0]==0xFE)
			{
				pp->rxbuf_escape = true;
			} else
			{
				if (pp->rxbuf_len<1024)
				{
					pp->rxbuf[pp->rxbuf_len] = rbuf[0];
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
			if (pp->txbuf_ptr==pp->txbuf_len)
			{
				pp->txbuf_len = 0;
				pp->tx_packet->packet_status = FREE;
				pp->tx_packet = NULL;
			}
			else
			{
				putc(pp->txbuf[pp->txbuf_ptr], pp->fs_out);
				fflush(pp->fs_out);
				pp->txbuf_ptr++;
			}
		}
	}
}

static bool rx_available( void * if_private, pqp_prio_t priority )
{
	pipe_priv_t * pp = (pipe_priv_t*)if_private;
	if (!pp->rx_packet_valid) return false;
	if (pp->rx_packet.priority != priority) return false;
	return true;
}

static bool rx_get_packet( void * if_private, pqp_prio_t priority, pqp_packet_t * packet )
{
	pipe_priv_t * pp = (pipe_priv_t*)if_private;
	if (!pp->rx_packet_valid) return false;
	if (pp->rx_packet.priority != priority) return false;
	memcpy(packet, &pp->rx_packet, sizeof(pp->rx_packet));
	pp->rx_packet_valid = false;
	return true;
}

static bool tx_enqueue_packet( void * if_private, pqp_packet_t * packet )
{
	pipe_priv_t * pp = (pipe_priv_t*)if_private;
	if ( pp->tx_packet ) return false;
	pp->tx_packet = packet;
	return true;
}



// public

void PQP_IF_OBJ_INITIALIZER_PIPE1( pqp_interface_t * pqp_interface )
{
	memset(&pp1, 0, sizeof(pp1));
	pp1.fs_in = stdin;
	pp1.fs_out = stdout;
	pqp_interface->if_private = &pp1;
	pqp_interface->if_deinit = deinit;
	pqp_interface->if_process = process;
	pqp_interface->if_rx_available = rx_available;
	pqp_interface->if_rx_get_packet = rx_get_packet;
	pqp_interface->if_tx_enqueue_packet = tx_enqueue_packet;
	
	fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
};

void PQP_IF_OBJ_INITIALIZER_PIPE2( pqp_interface_t * pqp_interface )
{
	memset(&pp2, 0, sizeof(pp2));
	pp2.fs_in = fopen("f21", "rb");
	pp2.fs_out = fopen("f12", "wb");
	pqp_interface->if_private = &pp2;
	pqp_interface->if_deinit = deinit;
	pqp_interface->if_process = process;
	pqp_interface->if_rx_available = rx_available;
	pqp_interface->if_rx_get_packet = rx_get_packet;
	pqp_interface->if_tx_enqueue_packet = tx_enqueue_packet;
	int fd = fileno(pp2.fs_in);
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
};

