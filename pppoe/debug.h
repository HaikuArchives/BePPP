/*
 * Copyright (C) 1997 Be, Inc.  All Rights Reserved
 */
 
//_----------This is part of net_dump

#define MAC_ADDR_LEN       6
#define MAX_PKT_N          300
#define NETDUMP_PRIO       1			 // default is 0
#define DUMPTO             "/tmp/pppoe.dump"
#define ETHDR_PROTOID_OFS  12
#define MAX_ETH_LEN        1536
#define PROTOID_IP         0x0800
#define PROTOID_ARP        0x0806
#define PROTOID_RARP       0x8035
#define PROTOID_APLTK      0x809b

#include <add-ons/net_server/NetDevice.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <byteorder.h>

void DumpPacket(BNetPacket *pkt);

static FILE *f;
static short just_started;

void DumpPacket(BNetPacket *pkt) {
    static double time0;
	static ulong pktn;
	ushort pkt_size, i;
	uchar mac_addr[MAC_ADDR_LEN];
	char protostr[12];
	ushort protoid;

    if (just_started) {
	    just_started = false;
	    pktn = 0;
		time0 = system_time();
    }
	pkt_size = pkt->Size();

    fprintf(f, "Frame# %u, Length=%d, ", pktn, pkt_size);
	fprintf(f, "deltaTime=%10.4f, ", (system_time() - time0) / 1000);

	// PID/len
	pkt->Read(ETHDR_PROTOID_OFS, (char *)&protoid, sizeof(protoid));
	protoid = B_BENDIAN_TO_HOST_INT16(protoid);  // net is big-endian
	// interpret it
	if (protoid > MAX_ETH_LEN) {
	  switch (protoid) {
	  case PROTOID_IP:
		strcpy(protostr, "IP");
		break;
	  case PROTOID_ARP:
		strcpy(protostr, "ARP");
		break;
	  case PROTOID_RARP:
		strcpy(protostr, "RARP");
		break;
	  case PROTOID_APLTK:
		strcpy(protostr, "AppleTalk");
		break;
	  case 0x8863:
		strcpy(protostr, "PPPoE Discovery");
		break;
	  case 0x8864:
		strcpy(protostr, "PPPoE Session");
		break;
	  default:
		strcpy(protostr, "#Unknown#");
		break;
	  }
	  fprintf(f, "Protocol=%s, ", protostr);
	} // else // This is IEEE 802.3 len. See RFC 894, 1042...

	// frame destination
	fprintf(f, "PID=0x%04x\nDst=[", protoid);
	pkt->Read(0, (char *) &mac_addr, MAC_ADDR_LEN);
	for (i = 0; i < MAC_ADDR_LEN; i++)
	    fprintf(f, "%02x%s", (uchar) mac_addr[i], 
				(i == MAC_ADDR_LEN -1) ? "], " : ":");

	// ...and source
	fprintf(f, "Src=[");
	pkt->Read(MAC_ADDR_LEN, (char *) &mac_addr, MAC_ADDR_LEN);
	for (i = 0; i < MAC_ADDR_LEN; i++)
	    fprintf(f, "%02x%s", (uchar) mac_addr[i],
				(i == MAC_ADDR_LEN -1) ? "], " : ":");

	fprintf(f, "Entire frame follows");
	for (i = 0; i < pkt_size; i++) {
	    fprintf(f, "%s%02x", (i % 0x10) == 0 ? "\n\t" : " ",
				(uchar) (pkt->Data())[i]);
	}
    fprintf(f, "\n"); fflush(f);
	time0 = system_time();		// there's always next time()
}