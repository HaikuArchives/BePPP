#include "pptp_packet.h"
#include <ByteOrder.h>

GREPacket::GREPacket(uint32 dest,uint32 src,uint16 protocol,uint16 session,uint32 seq,bool has_seq,uint32 ack,bool has_ack) 
	: IpPacket(dest,src,47,60 /* a minute is a good ttl */,0 /*normal*/,0)
{
	uint8 b = 0;
	uint16 s = 0;
	uint32 l = 0;
	{
		unsigned new_size = Size() + 8;
		if (has_ack)
			new_size += 4;
		if (has_seq)
			new_size += 4;
		SetSize(new_size); //----Header
	}
	b = 0x20;
	if (has_seq)
		b |= (1 << 4); /*seq flag*/
	Write(0,(char *)&b,1);
	b = ((has_ack) ? 0x80 : 0x00) /* has an ack */ | 1 /*ver*/;
	Write(1,(char *)&b,1);
	s = htons(protocol);
	Write(2,(char *)&s,2);
	s = 0; /*Payload length now*/
	Write(4,(char *)&s,2);
	s = htons(session);
	Write(6,(char *)&s,2);
	if (has_seq) {
		l = htonl(seq);
		Write(8,(char *)&l,4);
	}
	if (has_ack) {
		l = htonl(ack);
		Write((has_seq) ? 12 : 8,(char *)&l,4);
	}
}

GREPacket::GREPacket(BNetPacket *packet) {
	SetSize(packet->Size());
	copy_packet(packet,0,this,0,packet->Size());
}

uint32 GREPacket::AckNum(void) {
	if (!HasAck())
		return 0;
	uint32 l;
	Read((HasSeq()) ? 12 : 8,(char *)&l,4);
	l = ntohl(l);
	return l;
}

uint32 GREPacket::SeqNum(void) {
	if (!HasSeq())
		return 0;
	uint32 l;
	Read(8,(char *)&l,4);
	l = ntohl(l);
	return l;
}
	
bool GREPacket::HasAck(void) {
	uint8 flags;
	
	Read(1,(char *)&flags,1);
	return (flags & 0x80);
}

bool GREPacket::HasSeq(void) {
	uint8 flags;
	Read(0,(char *)&flags,1);
	return (flags & (1 << 4)); /*seq flag*/
}

uint16 GREPacket::ProtocolValue(void) {
	uint16 protocol;
	Write(2,(char *)&protocol,2);
	return (ntohs(protocol));
}

void GREPacket::SetData(const void *data,size_t length,uint16 offset) {
	unsigned header_length = 8;
	if (HasAck())
		header_length += 4;
	if (HasSeq())
		header_length += 4;
	IpPacket::SetData(data,length,header_length + offset);
	uint16 s = htons(Size() - 20/*ip header length*/);
	Write(4,(char *)&s,2); /* fix our packet len */
}

size_t GREPacket::GetData(const void *buffer,size_t length,uint16 offset) {
	unsigned header_length = 8;
	if (HasAck())
		header_length += 4;
	if (HasSeq())
		header_length += 4; //--------Variable header lens really, really stink
	uint16 defined_len;
	Read(4,(char *)&defined_len,2);
	defined_len = ntohs(defined_len);
	length = (length > defined_len) ? defined_len : length; /*Reality check against length value*/
	return IpPacket::GetData(buffer,length,header_length + offset);
}

PPtPPacket::PPtPPacket(uint32 dest, uint32 src,uint16 call_id,uint32 seq,uint32 ack,bool has_ack)
	: GREPacket(dest,src,0x880b/*pptp id*/,call_id,seq,true,ack,has_ack)
{
	//--------No setup needed
}

PPtPPacket::PPtPPacket(BNetPacket *packet) 
	: GREPacket(packet)
{
	//--------No setup needed
}

void PPtPPacket::SetData(const void *data,size_t length,uint16 offset) {
	GREPacket::SetData(data,length,offset);
}
	
size_t PPtPPacket::GetData(const void *buffer,size_t length,uint16 offset) {
	return GREPacket::GetData(buffer,length,offset);
}

PPtPControlPacket::PPtPControlPacket(uint16 message) {
	uint16 s = 0;
	uint32 l = htonl(0x1a2b3c4d);
	SetSize(12);
	
	Write(0,(char *)&s,2);
	s = htons(0x01);
	Write(2,(char *)&s,2);
	Write(4,(char *)&l,4);
	s = htons(message);
	Write(8,(char *)&s,2);
	s = htons(0x00);
	Write(10,(char *)&s,2);
	
}
		
uint16 PPtPControlPacket::Message(void) {
	uint16 msg;
	Read(8,(char *)&msg,2);
	return ntohs(msg);
}

void PPtPControlPacket::SetMessage(uint16 msg) {
	msg = htons(msg);
	Write(8,(char *)&msg,2);
}

bool PPtPControlPacket::IsValid(void) {
	uint32 cookie;
	Read(4,(char *)&cookie,4);
	return (ntohl(cookie) == 0x1a2b3c4d);
}

void PPtPControlPacket::SetData(const void *data,size_t length,uint16 offset) {
	if (length+12+offset > Size())
		SetSize(length+12+offset);
	uint16 total_length = htons(Size());
	Write(12+offset,(char *)data,length);
	Write(0,(char *)&total_length,2);
}

size_t PPtPControlPacket::GetData(const void *buffer,size_t length,uint16 offset) {
	size_t size = Size() - 12 - offset;
	size = (length > size) ? size : length;
	Read(12+offset,(char *)buffer,size);
	return size;
}
