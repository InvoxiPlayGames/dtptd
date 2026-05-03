#ifndef PPP_DEFS_H
#define PPP_DEFS_H

// Protocol types for PPP (non-complete)
#define PPP_PROTO_IPV4 0x0021 // IPv4
#define PPP_PROTO_IPV4_VJC 0x002E // IPv4, VJ compressed
#define PPP_PROTO_IPV4_VJU 0x002F // IPv4, VJ uncompressed
#define PPP_PROTO_COMP 0x00FD // Compressed packet
#define PPP_PROTO_LCP 0xC021 // Link Control Protocol
#define PPP_PROTO_IPCP 0x8021 // IP Control Protocol
#define PPP_PROTO_CCP 0x80FD // Compression Control Protocol

// "Code" values for PPP Control Protocols
#define PPP_CTRL_CONFIG_REQ 0x01 // Configure-Request
#define PPP_CTRL_CONFIG_ACK 0x02 // Configure-Ack
#define PPP_CTRL_CONFIG_NAK 0x03 // Configure-Nak
#define PPP_CTRL_CONFIG_REJ 0x04 // Configure-Reject
#define PPP_CTRL_TERM_REQ 0x05 // Terminate-Request
#define PPP_CTRL_TERM_ACK 0x06 // Terminate-Ack
#define PPP_CTRL_CODE_REJ 0x07 // Code-Reject
#define PPP_CTRL_PROTO_REJ 0x08 // Protocol-Reject
#define PPP_CTRL_ECHO_REQ 0x09 // Echo-Request
#define PPP_CTRL_ECHO_REP 0x0A // Echo-Reply
#define PPP_CTRL_DISCARD_REQ 0x0B // Discard-Request

// Options for the PPP Link Control Protocol
#define PPP_LCP_MRU 1 // Maximum-Reserve-Unit
#define PPP_LCP_ACCM 2 // Async-Character-Control-Map
#define PPP_LCP_AP 3 // Authentication-Protocol
#define PPP_LCP_QP 4 // Quality-Protocol
#define PPP_LCP_MN 5 // Magic-Number
#define PPP_LCP_PFC 7 // Protocol-Field-Compression
#define PPP_LCP_ACFC 8 // Address-and-Control-Field-Compression

#endif // PPP_DEFS_H
