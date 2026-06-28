/* Copyright (C) 2026 szlldm
 * 
 * Null interface.
 * This file is part of libpqp.
 * 
 * LibPQP is dual-licensed: you may use it under the terms of the
 * GNU Affero General Public License version 3 (AGPLv3), or alternatively
 * under a commercial license.
 * You should have received a copy of the AGPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 * For commercial licensing, please contact.
 */

#include "libpqp_if_null.h"
#include <stddef.h>

// private

static void deinit( void * if_private )
{
}

static void process( void * if_private )
{
}

static bool rx_available( void * if_private, pqp_prio_t priority )
{
	return false;
}

static bool rx_get_packet( void * if_private, pqp_prio_t priority, pqp_packet_t * packet )
{
	return false;
}

static bool tx_enqueue_packet( void * if_private, pqp_packet_t * packet )
{
	packet->packet_status = FREE;
	return true;
}



// public

void PQP_IF_OBJ_INITIALIZER_NULL( pqp_interface_t * pqp_interface )
{
	pqp_interface->if_private = NULL;
	pqp_interface->if_deinit = deinit;
	pqp_interface->if_process = process;
	pqp_interface->if_rx_available = rx_available;
	pqp_interface->if_rx_get_packet = rx_get_packet;
	pqp_interface->if_tx_enqueue_packet = tx_enqueue_packet;
};

