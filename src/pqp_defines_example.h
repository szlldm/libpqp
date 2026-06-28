/* Copyright (C) 2026 szlldm
 * 
 * LibPQP user managed defines (example)
 * This file is part of libpqp.
 * 
 * LibPQP is dual-licensed: you may use it under the terms of the
 * GNU Affero General Public License version 3 (AGPLv3), or alternatively
 * under a commercial license.
 * You should have received a copy of the AGPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 * For commercial licensing, please contact.
 */

// pqp_defines.h

#ifndef PQP_DEFINES
#define PQP_DEFINES

#if defined(LIBPQP_IS_BOOTLOADER) + defined(LIBPQP_IS_FIRMWARE) != 1


// only one can remain:		(or LIBPQP_IS_xx can be globally defined)
#define LIBPQP_IS_BOOTLOADER
#define LIBPQP_IS_FIRMWARE


#endif


// optional parameter override:
// #define PQP_NUMBER_OF_TX_BUFFERS		(2)
// #define PQP_NUMBER_OF_RX_BUFFERS		(4)
// #define PQP_SEND_TTL				(6)



// default interface is NULL
// additional interfaces:
#define LIBPQP_HAS_CAN1
#define LIBPQP_HAS_HDUPLEX_UART1
//#define LIBPQP_HAS_CAN2
//#define LIBPQP_HAS_HDUPLEX_UART2
#define LIBPQP_HAS_PIPE

#endif // PQP_DEFINES

