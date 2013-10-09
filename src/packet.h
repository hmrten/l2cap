#ifndef PACKETS_H
#define PACKETS_H

#include "types.h"

typedef struct {
	ushort size;
	uchar id;
	ushort dsize;
	uchar data[1];
} packet_t;

packet_t *alloc_packet(const uchar *data, ushort size);
void dbg_dump_packet(const packet_t *pkt);

int decrypt_packet(packet_t *pkt, int fromsv, int *first);

void handle_sv_packet(const packet_t *pkt);
void handle_cl_packet(const packet_t *pkt);

#endif
