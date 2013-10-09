#include "types.h"
#include "clpackets.h"
#include "main.h"

void cl_attack(const packet_t *pkt)
{
	log_msg("C", "[%.2X] - Attack\r\n", pkt->id);
}
