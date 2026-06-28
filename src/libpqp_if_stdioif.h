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

#ifndef LIBPQP_IF_STDIOIF_H
#define LIBPQP_IF_STDIOIF_H

#include "libpqp.h"
#include "libpqp_private.h"
#include "libpqp_foreign.h"

#if defined( LIBPQP_HAS_STDIOIF )
extern void PQP_IF_OBJ_INITIALIZER_STDIOIF( pqp_interface_t * pqp_interface );
#endif

#endif //LIBPQP_IF_STDIOIF_H
