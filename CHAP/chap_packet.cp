#include "chap_packet.h"
#include <string.h>
#include <ByteOrder.h>

CHAPPacket::CHAPPacket(uint8 code,uint8 id) : BStandardPacket(4) {
	uint16 start = htons(0xff03/*start of PPP packet, see RFC 1662*/);
	Write(0,(char *)&start,2);
	
	Write(2,(char *)&code,1);
	Write(3,(char *)&id,1);
}

CHAPPacket::CHAPPacket(BNetPacket *pkt) : BStandardPacket(pkt->Size()) {
	copy_packet(pkt,0,this,0,pkt->Size());
}

uint8 CHAPPacket::Code(void) {
	uint8 code;
	Read(2,(char *)&code,1);
	return code;
}

uint8 CHAPPacket::ID(void) {
	uint8 id;
	Read(3,(char *)&id,1);
	return id;
}

void CHAPPacket::SetData(const void *data,size_t length,uint16 offset) {
	uint16 s;
	SetSize(length+6+offset);
	Write(6+offset,(char *)data,length);
	s = htons(uint16(length) + offset);
	Write(4,(char *)(&s),2);
}

size_t CHAPPacket::GetData(const void *buffer,size_t length,uint16 offset) {
	size_t size;
	Read(4,(char *)&size,2);
	size = (Size() - 4 - offset > ntohs(size)) ? ntohs(size) : (Size() - 4 - offset);
	size = (length > size) ? size : length;
	Read(6+offset,(char *)buffer,size);
	return size;
}

