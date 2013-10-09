#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "main.h"
#include "packet.h"

#define MAX_1_BYTE 0xd1
#define MAX_2_BYTE 0x87 // Freya

static int _seed;
static uchar _1bytetable[MAX_1_BYTE];
static uchar _2bytetable[MAX_2_BYTE];

static uchar _l2key[8] = { 0xC8, 0x27, 0x93, 0x01, 0xA1, 0x6C, 0x31, 0x97 };

static uchar _clkey[16];
static uchar _svkey[16];

static void reset_tables(int seed);
static int random(void);
static void deobfuscate(packet_t *pkt);

static void reset_keys(void)
{
	int i;
	for (i=0; i<8; ++i) _svkey[i] = 0x00;
	memcpy(_svkey+i, _l2key, 8);
}

// VersionCheck/KeyPacket (0x2e)
// uchar:    protocol (0 = wrong, 1 = ok)
// uchar(8): decryption key
// uint:     ???
// uint:     server id
// uint:     ???
// uint:     obfuscation key
int decrypt_packet(packet_t *pkt, int fromsv, int *first)
{
	uchar *key;
	uchar c1;
	uchar c2;
	int i;
	uchar *buf = pkt->data;
	uchar proto;
	uint svid;
	uint okey;

	if (*first) {
		if (fromsv) {
			*first = 0;

			if (pkt->id != 0x2e) {
				return -1;
			}

			if (pkt->size != 25) {
				log_msg("[S]", "error: malformed %.2X packet (size: %hu)\r\n", pkt->id, pkt->size);
				return -1;
			}

			reset_keys();

			proto = parse_uchar(&buf);
			for (i=0; i<8; ++i) {
				uchar c = parse_uchar(&buf);
				_svkey[i] = c;
			}
			memcpy(_clkey, _svkey, 16);
			parse_uint(&buf);        // ???
			svid = parse_uint(&buf); // server id
			parse_uchar(&buf);       // ???
			okey = parse_uint(&buf); // obfuscation key

			//log_msg("S", "[%.2X] proto: %u svid:%u okey: %u\r\n", pkt->id, proto, svid, okey);
			log_msg("S", "[%.2X] obfuscation key: %u\r\n", pkt->id, okey);
			reset_tables(okey);
		}
	} else {
		key = fromsv ? _svkey : _clkey;
		c1 = 0;
		c2 = pkt->id;
		pkt->id = c2 ^ key[0] ^ c1;
		c1 = c2;
		for (i=0; i<pkt->dsize; ++i) {
			c2 = pkt->data[i];
			pkt->data[i] = c2 ^ key[(i+1)&15] ^ c1;
			c1 = c2;
		}

		*((uint *)(key+8)) += (pkt->size-2);

		if (fromsv) {
			if (pkt->id == 0x0b) {
				okey = *(uint *)(pkt->data+(pkt->dsize-4));
				if (okey) {
					log_msg("S", "[%.2X] obfuscation key enabled in CharacterSelectedPacket (%u)\r\n", pkt->id, okey);
					reset_tables(okey);
				}
			}
		} else {
			deobfuscate(pkt);
		}
	}
	return 0;
}

static void reset_tables(int seed)
{
	int i;
	int j;
	int k;

	_seed = seed;

	for (i=0; i<MAX_1_BYTE; ++i) {
		_1bytetable[i] = i;
	}
	for (i=0; i<MAX_2_BYTE; ++i) {
		_2bytetable[i] = i;
	}

	for (i=2; i<(MAX_1_BYTE+1); ++i) {
		j = random() % i;
		k = _1bytetable[j];
		_1bytetable[j] = _1bytetable[i-1];
		_1bytetable[i-1] = k;
	}

	for (i=2; i<(MAX_2_BYTE+1); ++i) {
		j = random() % i;
		k = _2bytetable[j];
		_2bytetable[j] = _2bytetable[i-1];
		_2bytetable[i-1] = k;
	}

	for (i=0; i<MAX_1_BYTE; ++i) {
		if (_1bytetable[i] == 0x12) break;
	}
	j = _1bytetable[0x12];
	_1bytetable[0x12] = 0x12;
	_1bytetable[i] = j;

	for (i=0; i<MAX_1_BYTE; ++i) {
		if (_1bytetable[i] == 0xb1) break;
	}
	j = _1bytetable[0xb1];
	_1bytetable[0xb1] = 0xb1;
	_1bytetable[i] = j;
}

static int random(void)
{
	_seed = _seed * 0x343fd + 0x269ec3;
	return (_seed >> 0x10) & 0x7fff;
}

static void deobfuscate(packet_t *pkt)
{
	if (pkt->id >= MAX_1_BYTE) {
		log_msg("S", "warning: unkown 1 byte client packet: %.2X\r\n", pkt->id);
		return;
	}
	if (pkt->id == 0xd0) {
		if (pkt->data[0] >= MAX_2_BYTE) {
			log_msg("S", "warning: unknown 2 byte client packet: %.2X\r\n", pkt->data[0]);
			return;
		}
		pkt->data[0] = _2bytetable[pkt->data[0]];
	} else {
		pkt->id = _1bytetable[pkt->id];
	}
}
