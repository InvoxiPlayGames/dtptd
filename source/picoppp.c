/*
    picoppp.c - a very small "PPP" server for dtptd

    This server only aims to implement as much as is necessary for WP7's DTPT
    service to make connections through it - plenty of corners are cut, plenty
    of hacks are made.

    THIS IS NOT A SECURE, RELIABLE, PRODUCTION-GRADE PPP SERVER!
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include "ppp_defs.h"

// TODO(Emma): for future improvements, we should seperate IP stuff out of here
#include "tun.h"

typedef struct _ppp_state_t {
    bool link_up; // whether the link should be considered "up" or not
    bool link_done; // after phone gives us an LCP Config-Ack
    bool net_done; // after we give the phone an IPCP Config-Ack
    // output
    int(*output_func)(void *var, const uint8_t *packet, size_t packetsz);
    void *output_user;
    // debugging
    FILE *pppd_log_file;
} ppp_state_t;

// forward declare this
int ppp_send_outgoing(ppp_state_t *state, const uint8_t *packet, size_t packetsz);

// thread for receiving packets from the tunnel and sending to the device
// TODO(Emma): move this out of picoppp and make it more generic
static void *ppp_temp_tun_thread(void *arg)
{
    ppp_state_t *state = (ppp_state_t *)arg;
    printf("starting network thread\n");
    while (state->link_up && state->net_done) {
        uint8_t packetbuf[2048];
        packetbuf[0] = PPP_PROTO_IPV4;
        int r = tun_recv_packet(packetbuf + 1, sizeof(packetbuf) - 1);
        if (r > 0 && state->net_done) {
            // TODO(Emma): check IP headers to see if the packet is valid and to see if it's
            //  destined to the device
            ppp_send_outgoing(state, packetbuf, r + 1);
        }
    }
    printf("ending network thread\n");
    pthread_exit(NULL);
}

// FCS calculation table and code from https://datatracker.ietf.org/doc/html/rfc1662#appendix-C.2
static uint16_t fcstab[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};
#define PPPINITFCS16 0xffff  /* Initial FCS value */
static inline uint16_t pppfcs16(uint16_t fcs, const uint8_t *buf, int len)
{
    while (len--)
        fcs = (fcs >> 8) ^ fcstab[(fcs ^ *buf++) & 0xff];
    return (fcs);
}

#define HDLC_ENCODE_MAX(n) ((n * 2) + 6) // the absolute maximum encoding can be, this is wasteful
// TODO: small function to determine the length of the encoded frame with a given control map

// Implementation of PPP HDLC-like encoding (RFC 1662)
// Given an input buffer, populates an output buffer no bigger than HDLC_ENCODE_MAX(len) with an encoded
//  copy of the input buffer, based on the Async Character Control Map in the provided state.
// Returns the length of the encoded data, can't error out.
static int hdlc_framing_encode(uint32_t ctrl_char_map, const uint8_t *input, int len, uint8_t *output)
{
    // TODO(Emma): add output buffer size and bounds checks
    uint32_t accm_map = ctrl_char_map;
    int o = 0;
    // small macro helper function to tidy code up a little bit
#define ADD_TO_OUTPUT(c) { \
    if (((uint8_t)(c) < 0x20 && (accm_map & (1 << (c))) != 0) || (uint8_t)(c) == 0x7D || (uint8_t)(c) == 0x7E) { \
        output[o++] = 0x7D; \
        output[o++] = (uint8_t)(c) ^ 0x20; \
    } else { \
        output[o++] = (uint8_t)(c); \
    } \
}
    // add the flag byte and add encoded input data
    output[o++] = 0x7E;
    for (int i = 0; i < len; i++)
        ADD_TO_OUTPUT(input[i]);
    // calculate the FCS over the input data and append it here, also encoded
    uint16_t fcs = pppfcs16(PPPINITFCS16, input, len) ^ 0xFFFF;
    ADD_TO_OUTPUT(fcs & 0xFF);
    ADD_TO_OUTPUT((fcs >> 8) & 0xFF);
    // add the final flag byte
    output[o++] = 0x7E;
#undef ADD_TO_OUTPUT
    return o;
}

// Implementation of PPP HDLC-like decoding (RFC 1662)
// Given an input buffer, populates an output buffer no bigger than len with a decoded copy of the
//  input buffer; returns output length on success, -1 on invalid input and -2 on checksum error.
static int hdlc_framing_decode(uint32_t ctrl_char_map, const uint8_t *input, int len, uint8_t *output)
{
    // TODO(Emma): add output buffer size and bounds checks
    // unless the link state is completed, all characters must be escaped
    uint32_t accm_map = ctrl_char_map;
    int o = 0;
    // make sure we have flag bytes at the start and end of the data
    if (input[0] != 0x7E || input[len-1] != 0x7E) {
        printf("invalid flag byte\n");
        return -1; // invalid flag byte, drop msg
    }
    // go through and decode each encoded character
    for (int i = 1; i < len - 1; i++) {
        uint8_t c = input[i];
        if (c < 0x20 && (accm_map & (1 << c)) != 0) {
            continue; // unescaped special character, drop the character
        } else if (c == 0x7E) {
            break; // flag character, exit the message
        } else if (c == 0x7D) {
            output[o++] = input[++i] ^ 0x20; // escaped character
        } else {
            output[o++] = c; // unescaped character
        }
    }
    // calculate and verify the FCS
    uint16_t calc_fcs = pppfcs16(PPPINITFCS16, output, o - 2);
    uint16_t data_fcs = ((output[o - 1] << 8) | (output[o - 2])) ^ 0xFFFF;
    if (calc_fcs != data_fcs) {
        printf("invalid FCS byte\n");
        return -2; // invalid FCS, drop the packet
    }
    return o - 2; // don't include FCS in the length of data...
}

// Allocates a state for a PPP peer, free-able with free()
ppp_state_t *ppp_alloc_state()
{
    return (ppp_state_t *)malloc(sizeof(ppp_state_t));
}

// Initialises a PPP state structure
void ppp_init_state(ppp_state_t *state, int(*output_func)(void *, const uint8_t *, size_t), void *output_user, FILE *debug_file)
{
    memset(state, 0, sizeof(ppp_state_t));
    state->output_func = output_func;
    state->output_user = output_user;
    state->pppd_log_file = debug_file;
    // TODO(Emma): init the log file
}

static int ppp_handle_lcp_msg(ppp_state_t *state, const uint8_t *msg, size_t len)
{
    uint8_t code = msg[0];
    uint8_t identifier = msg[1];
    switch (code) {
        case PPP_CTRL_CONFIG_REQ: {
            printf("LCS: recv PPP_CTRL_CONFIG_REQ\n");
            state->link_up = true;
            // HACK(Emma): just return exactly what the device expects to recieve
            // hardcoded reply, assumes exactly what the device asked for
            uint8_t reply[] = {
                0xc0, 0x21, // PPP_PROTO_LCP
                PPP_CTRL_CONFIG_ACK,
                identifier,
                0x0, 0xe, // length
                0x2, 0x6, 0x0, 0x0, 0x0, 0x0, // ACCM 0
                0x7, 0x2, // PFC enable
                0x8, 0x2 // ACFC enable
            };
            ppp_send_outgoing(state, reply, sizeof(reply));
            printf("LCP: sent PPP_CTRL_CONFIG_ACK\n");
            // HACK(Emma): now just send exactly what the Zune server sends
            // hardcoded again; now we're the ones assuming what we need
            uint8_t request[] = {
                0xc0, 0x21, // PPP_PROTO_LCP
                PPP_CTRL_CONFIG_REQ,
                identifier + 1,
                0x0, 0xe, // length
                0x2, 0x6, 0x0, 0x0, 0x0, 0x0, // ACCM 0
                0x7, 0x2, // PFC enable
                0x8, 0x2 // ACFC enable
            };
            ppp_send_outgoing(state, request, sizeof(request));
            printf("LCP: sent PPP_CTRL_CONFIG_REQ\n");
            break;
        }
        case PPP_CTRL_CONFIG_ACK: {
            printf("LCS: recv PPP_CTRL_CONFIG_ACK\n");
            // HACK(Emma): for now, we just assume this is for ours, and we're leaving link config state
            state->link_done = true;
            break;
        }
        case PPP_CTRL_TERM_REQ: {
            printf("LCS: recv PPP_CTRL_TERM_REQ\n");
            state->link_done = false;
            state->net_done = false;
            uint8_t ack[] = {
                0xc0, 0x21, // PPP_PROTO_LCP
                PPP_CTRL_TERM_ACK,
                identifier,
                0x00, 0x04 // length
            };
            ppp_send_outgoing(state, ack, sizeof(ack));
            printf("LCP: sent PPP_CTRL_TERM_ACK\n");
            uint8_t req[] = {
                0xc0, 0x21, // PPP_PROTO_LCP
                PPP_CTRL_TERM_REQ,
                identifier + 1,
                0x00, 0x04 // length
            };
            ppp_send_outgoing(state, req, sizeof(req));
            printf("LCP: sent PPP_CTRL_TERM_REQ\n");
        }
        case PPP_CTRL_TERM_ACK: {
            printf("LCS: recv PPP_CTRL_TERM_ACK\n");
            state->link_done = false;
            state->net_done = false;
            state->link_up = false;
            break;
        }
        default:
            printf("Unhandled LCP message (%02x,%02x)!\n", code, identifier);
            break;
    }
    return 0;
}

static int ppp_handle_ccp_msg(ppp_state_t *state, const uint8_t *msg, size_t len)
{
    uint8_t code = msg[0];
    uint8_t identifier = msg[1];
    switch (code) {
        case PPP_CTRL_CONFIG_REQ: {
            printf("CCP: recv PPP_CTRL_CONFIG_REQ\n");
            // HACK(Emma): hardcoded, just like before...
            //  another hack is that we check if the configuration is asking for MPPC enable or disabled
            //  and if it's enabled, we NAK it to ask for something less cringe
            // (should probably implement MPPC now that it's all "free")
            if (msg[9] == 0x01) {
                uint8_t nak[] = {
                    0x80, 0xfd,
                    PPP_CTRL_CONFIG_NAK,
                    identifier,
                    0x0, 0xa, // length
                    0x12, // MPPC
                    0x6, // length of MPPC config
                    0x0, 0x0, 0x0, 0x0 // 0 bits (dont establish compression)
                };
                ppp_send_outgoing(state, nak, sizeof(nak));
                printf("CCP: sent PPP_CTRL_CONFIG_NAK\n");
            } else {
                // not establishing compression? great! continue and provide one ourselves
                uint8_t ack[] = {
                    0x80, 0xfd,
                    PPP_CTRL_CONFIG_ACK,
                    identifier,
                    0x0, 0xa, // length
                    0x12, // MPPC
                    0x6, // length of MPPC config
                    0x0, 0x0, 0x0, 0x0 // 0 bits (dont establish compression)
                };
                ppp_send_outgoing(state, ack, sizeof(ack));
                printf("CCP: sent PPP_CTRL_CONFIG_ACK\n");
                // not establishing compression? great! continue and provide one ourselves
                uint8_t request[] = {
                    0x80, 0xfd,
                    PPP_CTRL_CONFIG_REQ,
                    identifier + 1,
                    0x0, 0xa, // length
                    0x12, // MPPC
                    0x6, // length of MPPC config
                    0x0, 0x0, 0x0, 0x0 // 0 bits (dont establish compression)
                };
                ppp_send_outgoing(state, request, sizeof(request));
                printf("CCP: sent PPP_CTRL_CONFIG_REQ\n");
            }
            break;
        }
        case PPP_CTRL_CONFIG_ACK: {
            printf("CCP: recv PPP_CTRL_CONFIG_ACK\n");
            // just ignore it, we don't care until we actually enable compression
            break;
        }
        default:
            printf("Unhandled CCP message (%02x,%02x)!\n", code, identifier);
            break;
    }
    return 0;
}

static int ppp_handle_ipcp_msg(ppp_state_t *state, const uint8_t *msg, size_t len)
{
    uint8_t code = msg[0];
    uint8_t identifier = msg[1];
    switch (code) {
        case PPP_CTRL_CONFIG_REQ: {
            printf("IPCP: recv PPP_CTRL_CONFIG_REQ\n");
            // HACK(Emma): if the length is 40 bytes, this is the original one sent by the phone
            //  WE MUST NAK THIS ONE! this is where we send the guest's true IP
            if (msg[3] == 0x28) {
                // we should let the device know about our own IP
                uint8_t req[] = {
                    0x80, 0x21,
                    PPP_CTRL_CONFIG_REQ,
                    identifier + 1,
                    0x0, 0x0A, // length
                    0x3, // IP address
                    0x6, // length of IP config
                    0x0, 0x0, 0x0, 0x0, // IPv4 address (fill this in)
                };
                uint32_t host_ipv4 = tun_get_host_ip();
                memcpy(&req[8], &host_ipv4, sizeof(uint32_t));
                ppp_send_outgoing(state, req, sizeof(req));
                printf("IPCP: sent PPP_CTRL_CONFIG_REQ\n");
                // and then send our rejection
                uint8_t nak[] = {
                    0x80, 0x21,
                    PPP_CTRL_CONFIG_NAK,
                    identifier,
                    0x0, 0x10, // length
                    0x3, // IP address
                    0x6, // length of IP config
                    0x0, 0x0, 0x0, 0x0, // IPv4 address (fill this in)
                    0x81, // DNS server
                    0x6, // length of DNS config
                    0x01, 0x01, 0x01, 0x01 // hardcode 1.1.1.1 just to see what happens
                    // TODO(Emma): have configurable DNS
                };
                uint32_t guest_ipv4 = tun_get_guest_ip();
                memcpy(&nak[8], &guest_ipv4, sizeof(uint32_t));
                ppp_send_outgoing(state, nak, sizeof(nak));
                printf("IPCP: sent PPP_CTRL_CONFIG_NAK\n");
            } else if (msg[3] == 0x16) {
                // device is likely requesting what we just sent, let's just blindly ACK it
                uint8_t ack[0x18] = {
                    0x80, 0x21,
                    PPP_CTRL_CONFIG_ACK,
                    identifier,
                };
                memcpy(ack + 0x4, msg + 0x2, 0x14); // TODO(Emma): aaagh hardcoded nooooo
                ppp_send_outgoing(state, ack, sizeof(ack));
                printf("IPCP: sent PPP_CTRL_CONFIG_ACK\n");
                state->net_done = true;
                pthread_t tunlisten;
                pthread_create(&tunlisten, NULL, ppp_temp_tun_thread, (void *)state);
            } else {
                printf("device sent an unknown config");
            }
            break;
        }
        case PPP_CTRL_CONFIG_ACK: {
            printf("IPCP: recv PPP_CTRL_CONFIG_ACK\n");
            break;
        }
        default:
            printf("Unhandled IPCP message (%02x,%02x)!\n", code, identifier);
            break;
    }
    return 0;
}

// Sends an outgoing packet to the other end of the PPP link.
int ppp_send_outgoing(ppp_state_t *state, const uint8_t *packet, size_t packetsz)
{
    int r = 0;
    // TODO(Emma): put packet in state log file output
    const uint8_t *inbuf = packet;
    int insz = packetsz;
    // detect if LCP packet
    uint16_t msg_proto = (packet[0] & 0x01) != 0 ? packet[0] : ((packet[0] << 8) | packet[1]);
    if (msg_proto == PPP_PROTO_LCP) {
        // stupid, prepend with address and control field by allocating whole new buffer
        uint8_t *newbuf = malloc(packetsz + 2);
        if (newbuf == NULL) {
            printf("failed to allocate new buffer for LCP ACF header\n");
            r = -1;
            goto finish;
        }
        newbuf[0] = 0xFF;
        newbuf[1] = 0x03;
        memcpy(newbuf + 2, packet, packetsz);
        inbuf = newbuf;
        insz += 2;
    }
    // encode HDLC framing
    uint8_t *hdlcbuf = malloc(HDLC_ENCODE_MAX(insz));
    if (hdlcbuf == NULL) {
        printf("failed to allocate new buffer for HDLC framing\n");
        r = -1;
        goto finish;
    }
    int hdlcr = hdlc_framing_encode(state->link_done ? 0 : 0xFFFFFFFF, inbuf, insz, hdlcbuf);
    if (hdlcr < 0) {
        printf("error %i when encoding HDLC framing\n", hdlcr);
        r = -2;
        goto finish;
    }
    int devr = state->output_func(state->output_user, hdlcbuf, hdlcr);
    if (r < 0) {
        printf("error %i when sending to device", devr);
        r = -3;
        goto finish;
    }
finish:
    if (hdlcbuf != NULL) {
        free(hdlcbuf);
    }
    if (inbuf != packet) {
        // we likely allocated this earlier
        free((void *)inbuf);
    }
    return r;
}

// Handles an incoming packet from the wire and dispatches it to the correct handler.
int ppp_handle_incoming(ppp_state_t *state, const uint8_t *packet, size_t packetsz)
{
    int r = 0;
    // decode HDLC-like framing
    uint8_t *data_buf = malloc(packetsz); // decoding will never increase packet size
    if (data_buf == NULL) {
        printf("Couldn't allocate buffer for the decoded packet! (OOM?)\n");
        r = -1;
        goto finish;
    }
    int hdlcr = hdlc_framing_decode(state->link_done ? 0 : 0xFFFFFFFF, packet, packetsz, data_buf);
    if (hdlcr < 0) {
        printf("Couldn't properly decode HDLC framing of packet.\n");
        r = -2;
        goto finish;
    }
    // skip past the address and control fields
    // we don't check to see ACF compression is enabled, we just assume it is
    uint8_t *data_ptr = data_buf;
    if (data_ptr[0] == 0xFF && data_ptr[1] == 0x03)
        data_ptr += 2;
    // get the protocol of the message
    uint16_t msg_proto = 0;
    // we assume protocol type compression is enabled as well
    if ((data_ptr[0] & 0x01) != 0) {
        msg_proto = data_ptr[0];
        data_ptr += 1;
    } else {
        msg_proto = (data_ptr[0] << 8) | data_ptr[1];
        data_ptr += 2;
    }
    size_t msg_sz = packetsz - (data_ptr - data_buf);
    // dispatch the message to the correct handler for the protocol
    switch (msg_proto) {
        case PPP_PROTO_LCP:
            ppp_handle_lcp_msg(state, data_ptr, msg_sz);
            break;
        case PPP_PROTO_CCP:
            ppp_handle_ccp_msg(state, data_ptr, msg_sz);
            break;
        case PPP_PROTO_IPCP:
            ppp_handle_ipcp_msg(state, data_ptr, msg_sz);
            break;
        case PPP_PROTO_IPV4:
            if (state->net_done) // ignore all IPv4 packets unless network link is set up
                tun_dispatch_packet(data_ptr, msg_sz);
            break;
        default:
            printf("Unhandled PPP protocol (%04x)!\n", msg_proto);
            break;
    }

finish:
    if (data_buf != NULL)
        free(data_buf);
    return r;
}
