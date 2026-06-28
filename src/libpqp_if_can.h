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

// CAN interface

#ifndef LIBPQP_IF_CAN_H
#define LIBPQP_IF_CAN_H

#include "libpqp.h"
#include "libpqp_private.h"
#include "libpqp_foreign.h"

#if defined( LIBPQP_HAS_CAN1 )
extern void PQP_IF_OBJ_INITIALIZER_CAN1( pqp_interface_t * pqp_interface );
#endif

#if defined( LIBPQP_HAS_CAN2 )
extern void PQP_IF_OBJ_INITIALIZER_CAN2( pqp_interface_t * pqp_interface );
#endif

#endif //LIBPQP_IF_CAN_H

