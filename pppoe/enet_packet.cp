#include "enet_packet.h"
#include <string.h>
#include <malloc.h>

EthernetPacket::EthernetPacket(enet_address to,enet_address from,uint16 protoid,size_t size) : BNetPacket() {
	base = 0;
	this->size = size + 14;
	data = malloc(this->size);
	
	if (to == NULL)
		to = (uint8 *)broadcast_address;
	Write(0,(const char *)to,MAC_ADDR_LEN);
	Write(MAC_ADDR_LEN,(const char *)from,MAC_ADDR_LEN);
	NET_ENDIAN(protoid);
	Write(ETHDR_PROTOID_OFS, (char *)(&protoid),sizeof(protoid));
}

EthernetPacket::EthernetPacket(enet_address to,BNetDevice *from,uint16 protoid,size_t size) : BNetPacket(){
	base = 0;
	this->size = size + 14;
	data = malloc(this->size);
	
	enet_address us;
	DeviceAddress(from,&us);
	if (to == NULL)
		to = (uint8 *)broadcast_address;
	Write(0,(const char *)to,MAC_ADDR_LEN);
	Write(MAC_ADDR_LEN,(const char *)us,MAC_ADDR_LEN);
	NET_ENDIAN(protoid);
	Write(ETHDR_PROTOID_OFS, (char *)(&protoid),sizeof(protoid));
}

EthernetPacket::EthernetPacket(BNetPacket *packet) : BNetPacket() {
	base = 0;
	size = packet->Size();
	data = malloc(size);
	
	copy_packet(packet, 0, this, 0, packet->Size());
}

void EthernetPacket::Destination(enet_address *addr) {
	Read(0,(char *)(*addr),MAC_ADDR_LEN);
}

void EthernetPacket::Source(enet_address *addr) {
	Read(MAC_ADDR_LEN,(char *)(*addr),MAC_ADDR_LEN);
}


uint16 EthernetPacket::Protocol() {
	uint16 protocol;
	Read(ETHDR_PROTOID_OFS,(char *)&protocol,sizeof(protocol));
	FROM_NET_ENDIAN(protocol);
	return protocol;
}

void EthernetPacket::SetData(const void *data,size_t length,uint16 offset) {
	if (length > 1522)
		return;
	SetSize(length+14+offset);
	Write(14+offset,(char *)data,length);
}

size_t EthernetPacket::GetData(const void *buffer,size_t length,uint16 offset) {
	size_t size = this->size - 14 - offset;
	size = (length > size) ? size : length;
	Read(14+offset,(char *)buffer,size);
	return size;
}

void DeviceAddress(BNetDevice *dev,enet_address *addr) {
	char us[255];
	dev->Address(us);
	memcpy(*addr,us,MAC_ADDR_LEN);
}

void EthernetPacket::Finalize(void) {
	ssize_t bytesToFill = 46 - (size - 14);
	if (bytesToFill <= 0)
		return;
	uint8 *bytes = new uint8[bytesToFill];
	memset(bytes,0,bytesToFill);
	size_t oldSize = size;
	SetSize(oldSize + bytesToFill);
	Write(oldSize,(const char *)bytes,bytesToFill);
	delete [] bytes;
}

unsigned EthernetPacket::Size(void) {
	return size - base;
}

void EthernetPacket::SetSize(unsigned size) {
	this->size = size + base;
	data = realloc(data,this->size);
}

unsigned EthernetPacket::Base(void) {
	return base;
}

void EthernetPacket::SetBase(int offset) {
	base = offset;
}

char *EthernetPacket::DataBlock(unsigned offset, unsigned *size) {
	void *to_return;
	*size = ((this->size - base - offset) > *size) ? *size : (this->size - base - offset);
	to_return = (void *)(uint32(data) + base + offset);
	if ((this->size - base - offset) <= 0) {
		*size = 1 /* never return 0 */;
		return NULL;
	}
	return (char *)to_return;
}
	

EthernetPacket::~EthernetPacket(void) {
	free(data);
}