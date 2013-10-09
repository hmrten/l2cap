#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pcap.h>

#include "capture.h"
#include "main.h"
#include "config.h"
#include "types.h"
#include "packet.h"

struct ip_header {
	uchar ihl:4;
	uchar v:4;
	uchar ds;
	ushort len;
	ushort id;
	ushort off;
	uchar ttl;
	uchar prot;
	ushort chksum;
	ulong src;
	ulong dst;
};

struct tcp_header {
	ushort src;
	ushort dst;
	ulong seq;
	ulong ack;
	uchar res:4;
	uchar doff:4;
	uchar flags;
#define TH_SYN 0x02
	ushort win;
	ushort sum;
	ushort urp;
};

struct tcp_frag {
	ulong seq;
	ushort psize;
	uchar *payload;
	struct tcp_frag *next;
};

struct tcp_stream {
	struct tcp_frag *frags;
	ulong seq;
	int first;
};

#define BUFSIZE 16*1024
static uchar _svbuf[BUFSIZE];
static uchar _clbuf[BUFSIZE];
static int _svbufsize = 0;
static int _clbufsize = 0;

static struct tcp_stream *_svstream=0;
static struct tcp_stream *_clstream=0;

static int _fromsv;
static int _isfirst = 1;

static pcap_if_t *_devicelist = 0;
static const char *_servers[] = { "64.25.37.132", "0.0.0.0" };

int stopcapturing;

int get_packet(uchar *buf, const uchar *raw, ushort rawsize);
void handle_packet(const uchar *data, ushort size);
void handle_raw_packet(const struct pcap_pkthdr *pkt_header, const u_char *pkt_data);

void rebuild_stream(uchar *payload, ushort psize, ulong seq, int syn);
struct tcp_stream *open_stream(void);
void close_stream(struct tcp_stream *stream);

int get_device_name(char *buf, int index)
{
	pcap_if_t *d = _devicelist;
	while (index) {
		d = d->next;
		--index;
	}
	strcpy(buf, d->name);
	return 0;
}

int init_capture(void)
{
	pcap_if_t *d;
	char errbuf[PCAP_ERRBUF_SIZE];
	int i;
	int devsel = 0;

	if (_devicelist) {
		pcap_freealldevs(_devicelist);
	}

	if (pcap_findalldevs(&_devicelist, errbuf) == -1) {
		return -1;
	}

	for (d=_devicelist, i=0; d; d=d->next, ++i) {
		if (strcmp(config.device, d->name) == 0) {
			devsel = i;
		}
		add_device(d->name, d->description, i);
	}

	return devsel;
}

int do_capture(void)
{
	pcap_t *adhandle;
	char errbuf[PCAP_ERRBUF_SIZE];
	struct bpf_program fcode;
	char buf[512];
	struct pcap_pkthdr *header;
	const u_char *data;
	int res;
	int svindex=0;

	if ((adhandle=pcap_open_live(config.device, 65536, 1, 1000, errbuf)) == NULL) {
		//MessageBox(0, errbuf, "Error", MB_OK);
		log_msg("dbg", "pcap_open_live failed opening device: %s\r\n", config.device);
		//pcap_freealldevs(alldevs);
		return -2;
	}

	if (strcmp("Chronos", config.server) == 0) {
		svindex = 0;
	} else if (strcmp("Naia", config.server) == 0) {
		svindex = 1;
	}

	sprintf(buf, "tcp and ip host %s", _servers[svindex]);

	if (pcap_compile(adhandle, &fcode, buf, 1, 0xffffffff) < 0) {
		//MessageBox(0, "Error compiling pcap filter.", "Error", MB_OK);
		log_msg("dbg", "error compiling pcap filter.\r\n");
		pcap_close(adhandle);
		//pcap_freealldevs(alldevs);
		return -3;
	}

	if (pcap_setfilter(adhandle, &fcode) < 0) {
		//MessageBox(0, "Error setting pcap filter.", "Error", MB_OK);
		log_msg("dbg", "error setting pcap filter.\r\n");
		pcap_close(adhandle);
		//pcap_freealldevs(alldevs);
		return -4;		
	}

	log_msg("dbg", "Listening on %s...\r\n", config.device);

	//pcap_freealldevs(alldevs);

	stopcapturing = 0;

	_svstream = open_stream();
	_clstream = open_stream();

	_isfirst = 1;

	_svbufsize = 0;
	_clbufsize = 0;

	while ((res=pcap_next_ex(adhandle, &header, &data)) >= 0 && !stopcapturing) {
		if (res == 0) {
			continue;
		}
		
		handle_raw_packet(header, data); 
	}

	log_msg("dbg", "Stopped capture.\r\n");

	close_stream(_svstream);
	close_stream(_clstream);
	_svstream = 0;
	_clstream = 0;

	pcap_close(adhandle);

	return 0;
}

void handle_packet(const uchar *data, ushort size)
{
	uchar *buf;
	int *bufsize;
	ushort psize;
	packet_t *pkt;

	if (_fromsv) {
		buf = _svbuf;
		bufsize = &_svbufsize;
	} else {
		buf = _clbuf;
		bufsize = &_clbufsize;
	}

	if (size+*bufsize >= BUFSIZE) {
		log_msg("dbg", "warning: buffer size exceeded in handle_packet.\r\n");
		return;
	}

	memcpy(buf+*bufsize, data, size);
	*bufsize += size;
	psize = *(ushort *)buf;

	if (psize < 3) {
		log_msg("dbg", "warning: malformed packet (%.2X) incorrect size: %hu.\r\n", *(ushort *)data, psize);
		return;
	}

	if (*bufsize >= psize) {
		do {
			pkt = alloc_packet(buf, psize);
			if (decrypt_packet(pkt, _fromsv, &_isfirst) != 0) {
				log_msg("dbg", "error: decryption failed, missed key packet?\r\n");
			} else {
				if (_fromsv) {
					handle_sv_packet(pkt);
				} else {
					handle_cl_packet(pkt);
				}
			}
			free(pkt);
			*bufsize -= psize;
			memcpy(buf, buf+psize, *bufsize);
			psize = *(ushort *)buf;
		} while (*bufsize >= psize && *bufsize > 0);
	}
}

static ulong get_server_ip(const char *server)
{
	if (strcmp("Chronos", server) == 0) {
		return inet_addr("64.25.37.132");
	} else if (strcmp("Naia", server) == 0) {
		return inet_addr("0.0.0.0");
	}
	return 0;
}

void handle_raw_packet(const struct pcap_pkthdr *pkt_header, const u_char *pkt_data)
{
	struct ip_header *ip;
	struct tcp_header *tcp;
	ushort isize;
	ushort tsize;
	uchar *payload;
	ushort psize;
	ulong svip = get_server_ip(config.server);

	ip = (struct ip_header *)(pkt_data+14); // assume ethernet header is 14 bytes TODO: fix
	isize = ip->ihl*4;

	if (isize < 20) {
		log_msg("dbg", "error: incorrect IP header size.\r\n");
		return;
	}

	tcp = (struct tcp_header *)((uchar *)ip+isize);
	tsize = tcp->doff*4;

	if (tsize < 20) {
		log_msg("dbg", "error: incorrect TCP header size.\r\n");
		return;
	}

	_fromsv = ip->src == svip;

	psize = ntohs(ip->len) - isize - tsize;
	payload = (uchar *) ((uchar *)tcp+tsize);

	if (psize > 0) {
		rebuild_stream(payload, psize, ntohl(tcp->seq), tcp->flags & TH_SYN);
	}
}

static int check_fragments(struct tcp_stream *stream)
{
	struct tcp_frag *prev = 0, *cur = stream->frags;
	
	while (cur) {
		if (cur->seq == stream->seq) {
			if (cur->payload) {
				//stream->wpcb(cur->payload, cur->payload_size, stream->user);
				handle_packet(cur->payload, cur->psize);
			}
			stream->seq += cur->psize;
			if (prev) {
				prev->next = cur->next;
			} else {
				stream->frags = cur->next;
			}
			free(cur->payload);
			free(cur);
			return 1;
		}
		prev = cur;
		cur = cur->next;
	}
	return 0;
}

void rebuild_stream(uchar *payload, ushort psize, ulong seq, int syn)
{
	ulong newseq;
	ushort newpsize;
	struct tcp_frag *frag;
	struct tcp_stream *stream;

	stream = _fromsv ? _svstream : _clstream;

	if (stream->first) {
		stream->first = 0;
		stream->seq = seq + psize;
		if (syn) {
			++stream->seq;
		}
		//stream->wpcb(payload, payload_size, stream->user);
		handle_packet(payload, psize);
		return;
	}
	if (seq < stream->seq) {
		newseq = seq + psize;
		if (newseq > stream->seq) {
			newpsize = stream->seq - seq;
			if (newpsize > psize) {
				payload = 0;
				psize = 0;
			} else {
				payload = payload + newpsize;
				psize -= newpsize;
			}
			seq = stream->seq;
			psize = newseq - stream->seq;
		}
	}
	if (seq == stream->seq) {
		stream->seq += psize;
		if (syn) {
			++stream->seq;
		}
		if (payload) {
			//stream->wpcb(payload, payload_size, stream->user);
			handle_packet(payload, psize);
		}
		while (check_fragments(stream));
	} else {
		if (psize > 0 && seq > stream->seq) {
			frag = malloc(sizeof(struct tcp_frag));
			frag->payload = malloc(psize);
			frag->seq = seq;
			frag->psize = psize;
			memcpy(frag->payload, payload, psize);
			if (stream->frags) {
				frag->next = stream->frags;
			} else {
				frag->next = 0;
			}
			stream->frags = frag;
		}
	}
}

struct tcp_stream *open_stream(void)
{
	struct tcp_stream *stream = malloc(sizeof(struct tcp_stream));
	stream->frags = 0;
	stream->first = 1;
	//stream->seq = 0;
	return stream;
}

void close_stream(struct tcp_stream *stream)
{
	struct tcp_frag *frag;

	if (stream) {
		while (stream->frags) {
			frag = stream->frags;
			stream->frags = stream->frags->next;
			if (frag->payload) {
				free(frag->payload);
			}
			free(frag);
		}
		stream->frags = 0;
		free(stream);
	}
}
