/* Copyright (C) 2026 szlldm
 * 
 * LibPQP interface numbering translator.
 * This file is part of libpqp.
 * 
 * LibPQP is dual-licensed: you may use it under the terms of the
 * GNU Affero General Public License version 3 (AGPLv3), or alternatively
 * under a commercial license.
 * You should have received a copy of the AGPLv3 license along with this
 * program. If not, see <https://www.gnu.org/licenses/>.
 * For commercial licensing, please contact.
 */

	RICE_CAN1
	#ifndef LIBPQP_HAS_CAN1
	 = RICE_NULL
	#endif
	,
	RICE_CAN2
	#ifndef LIBPQP_HAS_CAN2
	 = RICE_CAN1
	#endif
	,
	RICE_CAN3
	#ifndef LIBPQP_HAS_CAN3
	 = RICE_CAN2
	#endif
	,
	RICE_CAN4
	#ifndef LIBPQP_HAS_CAN4
	 = RICE_CAN3
	#endif
	,
	RICE_CAN5
	#ifndef LIBPQP_HAS_CAN5
	 = RICE_CAN4
	#endif
	,
	RICE_CAN6
	#ifndef LIBPQP_HAS_CAN6
	 = RICE_CAN5
	#endif
	,
	RICE_CAN7
	#ifndef LIBPQP_HAS_CAN7
	 = RICE_CAN6
	#endif
	,
	RICE_CAN8
	#ifndef LIBPQP_HAS_CAN8
	 = RICE_CAN7
	#endif
	,
	RICE_HDUPLEX_UART1
	#ifndef LIBPQP_HAS_HDUPLEX_UART1
	 = RICE_CAN8
	#endif
	,
	RICE_HDUPLEX_UART2
	#ifndef LIBPQP_HAS_HDUPLEX_UART2
	 = RICE_HDUPLEX_UART1
	#endif
	,
	RICE_HDUPLEX_UART3
	#ifndef LIBPQP_HAS_HDUPLEX_UART3
	 = RICE_HDUPLEX_UART2
	#endif
	,
	RICE_HDUPLEX_UART4
	#ifndef LIBPQP_HAS_HDUPLEX_UART4
	 = RICE_HDUPLEX_UART3
	#endif
	,
	RICE_HDUPLEX_UART5
	#ifndef LIBPQP_HAS_HDUPLEX_UART5
	 = RICE_HDUPLEX_UART4
	#endif
	,
	RICE_HDUPLEX_UART6
	#ifndef LIBPQP_HAS_HDUPLEX_UART6
	 = RICE_HDUPLEX_UART5
	#endif
	,
	RICE_HDUPLEX_UART7
	#ifndef LIBPQP_HAS_HDUPLEX_UART7
	 = RICE_HDUPLEX_UART6
	#endif
	,
	RICE_HDUPLEX_UART8
	#ifndef LIBPQP_HAS_HDUPLEX_UART8
	 = RICE_HDUPLEX_UART7
	#endif
	,
	RICE_I2C1
	#ifndef LIBPQP_HAS_I2C1
	 = RICE_HDUPLEX_UART8
	#endif
	,
	RICE_I2C2
	#ifndef LIBPQP_HAS_I2C2
	 = RICE_I2C1
	#endif
	,
	RICE_I2C3
	#ifndef LIBPQP_HAS_I2C3
	 = RICE_I2C2
	#endif
	,
	RICE_I2C4
	#ifndef LIBPQP_HAS_I2C4
	 = RICE_I2C3
	#endif
	,
	RICE_I2C5
	#ifndef LIBPQP_HAS_I2C5
	 = RICE_I2C4
	#endif
	,
	RICE_I2C6
	#ifndef LIBPQP_HAS_I2C6
	 = RICE_I2C5
	#endif
	,
	RICE_I2C7
	#ifndef LIBPQP_HAS_I2C7
	 = RICE_I2C6
	#endif
	,
	RICE_I2C8
	#ifndef LIBPQP_HAS_I2C8
	 = RICE_I2C7
	#endif
	,
	RICE_FDUPLEX_UART1
	#ifndef LIBPQP_HAS_FDUPLEX_UART1
	 = RICE_I2C8
	#endif
	,
	RICE_FDUPLEX_UART2
	#ifndef LIBPQP_HAS_FDUPLEX_UART2
	 = RICE_FDUPLEX_UART1
	#endif
	,
	RICE_FDUPLEX_UART3
	#ifndef LIBPQP_HAS_FDUPLEX_UART3
	 = RICE_FDUPLEX_UART2
	#endif
	,
	RICE_FDUPLEX_UART4
	#ifndef LIBPQP_HAS_FDUPLEX_UART4
	 = RICE_FDUPLEX_UART3
	#endif
	,
	RICE_FDUPLEX_UART5
	#ifndef LIBPQP_HAS_FDUPLEX_UART5
	 = RICE_FDUPLEX_UART4
	#endif
	,
	RICE_FDUPLEX_UART6
	#ifndef LIBPQP_HAS_FDUPLEX_UART6
	 = RICE_FDUPLEX_UART5
	#endif
	,
	RICE_FDUPLEX_UART7
	#ifndef LIBPQP_HAS_FDUPLEX_UART7
	 = RICE_FDUPLEX_UART6
	#endif
	,
	RICE_FDUPLEX_UART8
	#ifndef LIBPQP_HAS_FDUPLEX_UART8
	 = RICE_FDUPLEX_UART7
	#endif
	,
	RICE_RAW1
	#ifndef LIBPQP_HAS_RAW1
	 = RICE_FDUPLEX_UART8
	#endif
	,
	RICE_RAW2
	#ifndef LIBPQP_HAS_RAW2
	 = RICE_RAW1
	#endif
	,
	RICE_RAW3
	#ifndef LIBPQP_HAS_RAW3
	 = RICE_RAW2
	#endif
	,
	RICE_RAW4
	#ifndef LIBPQP_HAS_RAW4
	 = RICE_RAW3
	#endif
	,
	RICE_RAW5
	#ifndef LIBPQP_HAS_RAW5
	 = RICE_RAW4
	#endif
	,
	RICE_RAW6
	#ifndef LIBPQP_HAS_RAW6
	 = RICE_RAW5
	#endif
	,
	RICE_RAW7
	#ifndef LIBPQP_HAS_RAW7
	 = RICE_RAW6
	#endif
	,
	RICE_RAW8
	#ifndef LIBPQP_HAS_RAW8
	 = RICE_RAW7
	#endif
	,
	RICE_PIPE1
	#ifndef LIBPQP_HAS_PIPE1
	 = RICE_RAW8
	#endif
	,
	RICE_PIPE2
	#ifndef LIBPQP_HAS_PIPE2
	 = RICE_PIPE1
	#endif
	,
	RICE_PIPE3
	#ifndef LIBPQP_HAS_PIPE3
	 = RICE_PIPE2
	#endif
	,
	RICE_PIPE4
	#ifndef LIBPQP_HAS_PIPE4
	 = RICE_PIPE3
	#endif
	,
	RICE_PIPE5
	#ifndef LIBPQP_HAS_PIPE5
	 = RICE_PIPE4
	#endif
	,
	RICE_PIPE6
	#ifndef LIBPQP_HAS_PIPE6
	 = RICE_PIPE5
	#endif
	,
	RICE_PIPE7
	#ifndef LIBPQP_HAS_PIPE7
	 = RICE_PIPE6
	#endif
	,
	RICE_PIPE8
	#ifndef LIBPQP_HAS_PIPE8
	 = RICE_PIPE7
	#endif
	,
	RICE_STDIOIF
	#ifndef LIBPQP_HAS_STDIOIF
	 = RICE_PIPE8
	#endif
	,
};

const uint8_t routing_interface_translator_table[256] = 
{
	0,

	#ifdef LIBPQP_HAS_CAN1
	RICE_CAN1
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_CAN2
	RICE_CAN2
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_CAN3
	RICE_CAN3
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_CAN4
	RICE_CAN4
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_CAN5
	RICE_CAN5
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_CAN6
	RICE_CAN6
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_CAN7
	RICE_CAN7
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_CAN8
	RICE_CAN8
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_HDUPLEX_UART1
	RICE_HDUPLEX_UART1
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_HDUPLEX_UART2
	RICE_HDUPLEX_UART2
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_HDUPLEX_UART3
	RICE_HDUPLEX_UART3
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_HDUPLEX_UART4
	RICE_HDUPLEX_UART4
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_HDUPLEX_UART5
	RICE_HDUPLEX_UART5
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_HDUPLEX_UART6
	RICE_HDUPLEX_UART6
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_HDUPLEX_UART7
	RICE_HDUPLEX_UART7
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_HDUPLEX_UART8
	RICE_HDUPLEX_UART8
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_I2C1
	RICE_I2C1
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_I2C2
	RICE_I2C2
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_I2C3
	RICE_I2C3
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_I2C4
	RICE_I2C4
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_I2C5
	RICE_I2C5
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_I2C6
	RICE_I2C6
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_I2C7
	RICE_I2C7
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_I2C8
	RICE_I2C8
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_FDUPLEX_UART1
	RICE_FDUPLEX_UART1
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_FDUPLEX_UART2
	RICE_FDUPLEX_UART2
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_FDUPLEX_UART3
	RICE_FDUPLEX_UART3
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_FDUPLEX_UART4
	RICE_FDUPLEX_UART4
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_FDUPLEX_UART5
	RICE_FDUPLEX_UART5
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_FDUPLEX_UART6
	RICE_FDUPLEX_UART6
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_FDUPLEX_UART7
	RICE_FDUPLEX_UART7
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_FDUPLEX_UART8
	RICE_FDUPLEX_UART8
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_RAW1
	RICE_RAW1
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_RAW2
	RICE_RAW2
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_RAW3
	RICE_RAW3
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_RAW4
	RICE_RAW4
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_RAW5
	RICE_RAW5
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_RAW6
	RICE_RAW6
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_RAW7
	RICE_RAW7
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_RAW8
	RICE_RAW8
	#else
	0
	#endif
	,
	
	0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,  0,0,0,0,0,
	
	
	#ifdef LIBPQP_HAS_PIPE1
	RICE_PIPE1
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_PIPE2
	RICE_PIPE2
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_PIPE3
	RICE_PIPE3
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_PIPE4
	RICE_PIPE4
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_PIPE5
	RICE_PIPE5
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_PIPE6
	RICE_PIPE6
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_PIPE7
	RICE_PIPE7
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_PIPE8
	RICE_PIPE8
	#else
	0
	#endif
	,
	#ifdef LIBPQP_HAS_STDIOIF
	RICE_STDIOIF
	#else
	0
	#endif
	,
	
	0
