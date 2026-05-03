#ifndef PICOPPP_H
#define PICOPPP_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef void ppp_state_t;

ppp_state_t *ppp_alloc_state();
void ppp_init_state(ppp_state_t *state, int(*output_func)(void *, const uint8_t *, size_t), void *output_user, FILE *debug_file);
int ppp_send_outgoing(ppp_state_t *state, const uint8_t *packet, size_t packetsz);
int ppp_handle_incoming(ppp_state_t *state, const uint8_t *packet, size_t packetsz);

#endif // PICOPPP_H
