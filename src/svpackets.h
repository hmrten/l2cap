#ifndef SVPACKETS_H
#define SVPACKETS_H

#include "packet.h"

/* 0x14 */void sv_itemlist(const packet_t *pkt);
/* 0x4a */void sv_creaturesay(const packet_t *pkt);

#endif
