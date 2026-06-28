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

#ifndef LIBPQP_IF_RAW_H
#define LIBPQP_IF_RAW_H

#include "libpqp.h"
#include "libpqp_private.h"
#include "libpqp_foreign.h"

#define RAW_PACKET_HEADER_SIZE		( 10 )
#define RAW_PACKET_MAX_SIZE		( RAW_PACKET_HEADER_SIZE + PQP_MAX_PAYLOAD )

extern void PQP_IF_OBJ_INITIALIZER_RAW1( pqp_interface_t * pqp_interface );

int pqp_if_raw_get_packet( uint8_t * data );			// returns the number of bytes of the raw packet, or -1 on error / no packet
int pqp_if_raw_add_packet( uint8_t * data, int data_len );	// returns 0 if raw packet taken, or -1 if cannot take at the moment (try later)

#endif //LIBPQP_IF_RAW_H
