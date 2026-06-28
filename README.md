# LibPQP

The PocketQube Protocol library is a flight proven (TRL9) networking stack for pico-satellites.

It is designed to provide a hardware independent solution for onboard and remote inter-subsystem communication and remote satellite management.

The protocol is based on packet switching, with routing support.

## Licensing

LibPQP is dual-licensed: you may use it under the terms of the GNU Affero General Public License version 3 (AGPLv3), or alternatively under a commercial license.

## Packet format

### Header

The logical PQP frame is the following (bytes are in network order / big endian):

```
Byte order |         1st byte       |        2nd byte        |         3rd byte       |       4th byte         |
Bit offset |   31..30   |   29..24  | 23..22|21..20|  19..16 |          15..8         |           7..0         |
-----------+------------+-----------+-------+------+---------+------------------------+------------------------+
  0        | Priority   |   111110  | FLAGS | HLEN |   TTL   |       SRC Address      |       DST Address      |
-----------+------------+-----------+-------+------+---------+------------------------+------------------------+
 32        +        SRC Port        |        DST Port        |       Payload[0]       |       Payload[1]       |
-----------+------------------------+------------------------+------------------------+------------------------+
           +   Payload[2] .......
           +-----------------------
             ...
             CRC32
           ------------------------
```

#### Priority

| Level | Value | Bits |
| -------- | - | --------- |
| Critical | 0 | [31:30] == 00 |
| High | 1 | [31:30] == 01 |
| Normal | 2 | [31:30] == 10 |
| Low | 3 | [31:30] == 11 |

#### 111110

Fixed bit pattern

#### Flags

| Function | Value | Bit |
| -------- | - | --------- |
| PQP packet | 0 | [22] |
| Legacy CSP packet | 1 | [22] |
| Plain packet | 0 | [23] |
| Encrypted packet | 1 | [23] |

There is a limited support to transfer CSP packets.

#### HLEN
 
Header length in bytes = 2^HLEN

| Header length | Value | Bits | Header format |
| -------- | - | --------- | ----------------- |
| 2 bytes | 1 | [21:20] == 01 | 16 bit header extension |
| 4 bytes | 2 | [21:20] == 10 | 8 bit address/port fields |
| 8 bytes | 3 | [21:20] == 11 | 16 bit address/port fields |
| 16 bytes | 4 | [21:20] == 00 | 32 bit address/port fields |

Currently only HLEN == 2 is supported.

#### TTL

 - on packet forwarding the TTL field must be decremented by 1

 - if the received packet's TTL == 0, then it must not be forwarded

#### SRC/DST Address

With HLEN == 2 the addresses are 8 bit

 - 254: reserved address
 
 - 255: broadcast address ( do not reply ! )

#### SRC/DST Port
With HLEN == 2 the ports are 8 bit
 - 0.. 15: reserved (including CSP compatibility)
 - 16.. 31: PQP management ports
 - 32..127: server ports
 - 128..254: client ports
 - 255     : reserved

Server ports can be interpreted as individual commands, using the packet payload as parameters.

Commands ought to be originated from a client port.

Command responses shall come from the receiving server port.

#### Payload

 - arbitrary data, no endianness requirement
 
 - length: 0 .. PQP_MAX_PAYLOAD bytes ( PQP_MAX_PAYLOAD currently is 264; zero payload permitted)

#### CRC32

 - the checksum is CRC-32C (Castagnoli)
 
 - crc initialized to 0xFFFFFFFF
 
 - crc is calculated on both the header (NOT INCLUDING THE PREAMBLE) and the payload (starting at the 3rd byte, A.K.A. packet[2] )

 - the crc is appended to the end of the packet in network order (MSB first), without padding

### PQP management ports

#### 16: PING

It is not a traditional echo, the response provides a minimal status report: bootloader or firmware is running, uptime in seconds

#### 17: TRANSFER_TEST

This is the traditional ping functionality, a simple data echo.

#### 18: REBOOT

#### 19: BOOTLOADER

Reboot and remain in bootloader mode. Optionally triggers a local firmware cloning (copying fw from the "clone area" to the firmware area)

#### 20: CHECKSUM

Various full or partial checksum requests.

#### 21: RUN

Starts the firmware in bootlader mode.

#### 22: ERASE

Various full or partial but controlled firmware erase.

#### 23: CLEAR_MAP

Clears the progmap. The program is a 64 byte (512 bits) map, that aid remote firmware uploading.

#### 24: GET_MAP

Get the progmap.

#### 25: FW_WRITE

Writes a single firmware block. The firmware block is contained in he payload.

#### 26: FW_CLONE

Writes a single firmware block. The firmware block is read from the source device.

#### 27: FW_CLONE_CTRL

Controls / monitors the firmware cloning process.

#### 28: CLEAR_ROUTING_TABLE

Clears the routing table.

#### 29: ROUTING_TABLE_ENTRY

Writes some entries in the routing table.

#### 30: EXTENSIONS

#### 31: TTY

Optional interactive teletype terminal

### ROUTING

If a node receives a packet, it checks the destination address.

If the destination address is it's own (or broadcast), the packet has been arrived, and processing of the packet begins.

Otherwise both the source and destination address is checked in the routing table, that the packet has to be routed through an interface or not.



