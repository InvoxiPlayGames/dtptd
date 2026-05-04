#ifndef TUN_H
#define TUN_H

#include <stddef.h>
#include <stdint.h>

int tun_start_interface();
const char *tun_get_ifname();
uint32_t tun_get_host_ip();
uint32_t tun_get_guest_ip();
int tun_close_interface();
int tun_dispatch_packet(const uint8_t *packet, size_t packet_sz);
int tun_recv_packet(uint8_t *packet, size_t buf_sz);

#endif // TUN_H
