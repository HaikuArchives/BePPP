#include "ip_packet.h"
#include <ByteOrder.h>

//----------This top part is from NAT---------------
/*----------------------------------------------------------------------------------------
	NAT
------------------------------------------------------------------------------------------*/
// Network Address Translation for the BeOS
// Richard Burgess <rickb@spy.net>

/*----------------------------------------------------------------------------------------
	IP Header
------------------------------------------------------------------------------------------*/
//	RFC 791
//	be sure to take into account endianess and alignment issues 
//	if you use these structs to read and write to datagrams.
#pragma align=packed
struct ip_header 
{
#if B_HOST_IS_BENDIAN
	uint8	version:4,		// version
	 	  	header_length:4;// header length (in words)
#else
	uint8	header_length:4,// header length (in words)
			version:4;		// version
#endif
	uint8 	type;		// type of service
	uint16	length;		// total length
	uint16	id;			// identification
	uint16	offset;		// fragment offset field 
	uint8 	ttl;		// time to live
	uint8  	protocol;	// protocol
	uint16	checksum;	// checksum
	int32 	src;		// source ip address
	int32	dst;		// destination ip address
};
#pragma align=reset

// header offsets, our preferred way to access header elements
#define IP_LEN_VERSION_OFFSET	0
#define IP_TYPE_OFFSET			1
#define IP_LENGTH_OFFSET		2
#define IP_ID_OFFSET			4
#define IP_OFFSET_OFFSET		6
#define IP_TTL_OFFSET			8
#define IP_PROTOCOL_OFFSET		9
#define IP_CHECKSUM_OFFSET		10
#define IP_SRC_IP_OFFSET		12
#define IP_DST_IP_OFFSET		16

// accessors (on net-ordered data)
#define IP_VERSION(x) 		(x >> 4)
#define IP_HEADER_LENGTH(x)	( (x & 0xF) * 4) // in bytes

// protocol types
#define	IP_PROTO_IP		0	// dummy for IP 
#define	IP_PROTO_ICMP	1		// control message protocol 
#define	IP_PROTO_IGMP	2		// group mgmt protocol 
#define	IP_PROTO_GGP	3		// gateway^2 (deprecated) 
#define	IP_PROTO_IPIP	4		// IP inside IP 
#define	IP_PROTO_TCP	6		// tcp 
#define	IP_PROTO_EGP	8		// exterior gateway protocol 
#define	IP_PROTO_PUP	12		// pup 
#define	IP_PROTO_UDP	17		// user datagram protocol 
#define	IP_PROTO_IDP	22		// xns idp 
#define	IP_PROTO_TP		29 		// tp-4 w/ class negotiation 
#define	IP_PROTO_ESP	50 		// encap. security payload 
#define	IP_PROTO_AH		51 		// authentication header 
#define	IP_PROTO_EON	80		// ISO cnlp 
#define	IP_PROTO_ENCAP	98		// encapsulation header 

#define	IP_PROTO_RAW	255		// raw IP packet 
#define	IP_PROTO_MAX	256

//-------Begin non-NAT----------

uint16 id = 0;

IpPacket::IpPacket(uint32 to_ip,uint32 from_ip,uint8 protocol,uint8 ttl,uint8 service,uint8 flags,uint16 frag_offset) {
	uint8 b = 0;
	uint16 s = 0;
	uint32 l = 0;
	
	SetSize(20); //-----Header--------
	b = (4 << 4) | 20;	//----Version + 20 bytes for header-----
	Write(IP_LEN_VERSION_OFFSET,(const char *)&b,1);
	Write(IP_TYPE_OFFSET,(const char *)&service,1);
	Write(IP_LENGTH_OFFSET,(const char *)&s,2);
	s = htons(id++);
	Write(IP_ID_OFFSET,(const char *)&s,2);
	s = htons((flags << 13) | frag_offset);
	Write(IP_OFFSET_OFFSET,(const char *)&s,2);
	Write(IP_TTL_OFFSET,(const char *)&ttl,1);
	Write(IP_PROTOCOL_OFFSET,(const char *)&protocol,1);
	Write(IP_SRC_IP_OFFSET,(const char *)&from_ip,4);
	Write(IP_DST_IP_OFFSET,(const char *)&to_ip,4);
	CalculateChecksum();
}
	
IpPacket::IpPacket(const void *data,size_t size) {
	SetSize(size);
	Write(0,(const char *)data,size);
}

IpPacket::IpPacket(void) {
	//-------Do nothing, and do it well
}

uint8 IpPacket::Protocol(void) {
	uint8 protocol;
	Read(IP_PROTOCOL_OFFSET,(char *)&protocol,1);
	return protocol;
}
	
uint32 IpPacket::Source(void) {
	uint32 source;
	Read(IP_SRC_IP_OFFSET,(char *)&source,4);
	return source;
}

uint32 IpPacket::Destination(void) {
	uint32 dest;
	Read(IP_DST_IP_OFFSET,(char *)&dest,4);
	return dest;
}

void IpPacket::SetSource(uint32 ip) {
	Write(IP_SRC_IP_OFFSET,(char *)&ip,4);
	CalculateChecksum();
}

void IpPacket::SetDestination(uint32 ip) {
	Write(IP_DST_IP_OFFSET,(char *)&ip,4);
	CalculateChecksum();
}

void IpPacket::SetData(const void *data,size_t length,uint16 offset) {
	uint16 total_length = htons((length+20+offset > Size()) ? (length+20+offset) : Size());
	if (length+20+offset > Size())
		SetSize(length+20+offset);
	Write(20+offset,(char *)data,length);
	Write(IP_LENGTH_OFFSET,(char *)&total_length,2);
}

size_t IpPacket::GetData(const void *buffer,size_t length,uint16 offset) {
	size_t size = Size() - 20 - offset;
	size = (length > size) ? size : length;
	Read(20+offset,(char *)buffer,size);
	return size;
}

bool IpPacket::MoreComing(void) {
	uint16 s;
	uint8 flags;
	Read(IP_OFFSET_OFFSET,(char *)&s,2);
	flags = ntohs(s) >> 13;
	return !(flags & more_fragments);
}

uint16 IpPacket::PacketID(void) {
	uint16 id;
	Read(IP_ID_OFFSET,(char *)&id,2);
	return ntohs(id);
}

//------The following function was stolen from NAT----------
/*----------------------------------------------------------------------------------------
	 checksum_adjust
------------------------------------------------------------------------------------------*/
// adapted from RFC 1631, which didn't work worth a shit originally
void IpPacket::CalculateChecksum(void)
/* assuming: unsigned char is 8 bits, long is 32 bits.
  - chksum points to the chksum in the packet
  - optr points to the old_val data in the packet
  - nptr points to the new data in the packet
*/
{
	uint8 chksum[2] = {0,0};
	Write(IP_CHECKSUM_OFFSET,(char *)chksum,2); //-------Set to zero--------
	unsigned nlen;
	uint8 *nptr = (uint8 *)DataBlock(0,&nlen);
	uint16 x, last, old_val, new_val;
	x = 0;
	x = ~x;

	last = x;
	while (nlen) 
	{
		if (nlen == 1) 
		{
			new_val = nptr[0] * 0x100 + nptr[1];
			x += new_val & 0xff00;
			if (x < last) x++;
			break;
		}
		else {
			new_val = nptr[0] * 0x100 + nptr[1]; 
			nptr+=2;
			x += new_val;
			if (x < last) x++;
			nlen -= 2;
		}
		last = x;
	}

	x = ~x;
	chksum[0]= ((x >> 8) & 0xff); chksum[1] = (x & 0xff);
	Write(IP_CHECKSUM_OFFSET,(char *)chksum,2);
}