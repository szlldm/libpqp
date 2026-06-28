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

#ifndef LIBPQP_IF_PIPE_H
#define LIBPQP_IF_PIPE_H

#include "libpqp.h"
#include "libpqp_private.h"
#include "libpqp_foreign.h"

extern void PQP_IF_OBJ_INITIALIZER_PIPE1( pqp_interface_t * pqp_interface );
extern void PQP_IF_OBJ_INITIALIZER_PIPE2( pqp_interface_t * pqp_interface );

#endif //LIBPQP_IF_PIPE_H
