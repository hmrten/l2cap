#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#include "types.h"
#include "svpackets.h"
#include "main.h"

// uint: object id
// uint: chat type (shout, trade, party, etc)
// wstr: name
// wstr: text
void sv_creaturesay(const packet_t *pkt)
{
	uint oid;
	uint ttyp;
	wchar_t *name;
	wchar_t *text;
	uchar *buf = (uchar *)pkt->data;
	int c;

	oid  = parse_uint(&buf);
	ttyp = parse_uint(&buf);
	name = parse_wstr(&buf);
	text = parse_wstr(&buf);

	switch (ttyp) {
		case 0:  c = ' '; break; // general
		case 1:  c = '!'; break; // shout
		case 2:  c = '"'; break; // whisper
		case 3:  c = '#'; break; // party
		case 8:  c = '+'; break; // trade
		case 17: c = '%'; break; // hero
		default: c = -1;
	}

	if (c == -1) {
		log_msg("S", "[chat] (%u) %ls: %ls\r\n", ttyp, name, text);
	} else {
		log_msg("S", "[chat] (%c) %ls: %ls\r\n", c, name, text);
	}

	free(name);
	free(text);
}
