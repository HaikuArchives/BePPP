#include <string.h>
#include "pppoe_packet.h"

PPPoEPacket::PPPoEPacket(uint16 protoid,uint8 code,uint16 session_id,enet_address ac,BNetDevice *us) : EthernetPacket(ac,us,protoid,6) {
	uint8 magicCookie = 0x11;
	uint16 pointer = 14;
	Write(pointer,(char *)&magicCookie,sizeof(magicCookie));
	pointer += sizeof(magicCookie);
	Write(pointer,(char *)&code,sizeof(code));
	pointer++;
	NET_ENDIAN(session_id);
	Write(pointer,(char *)&session_id,sizeof(session_id));
	pointer += 2;
	uint16 cur_length = 0;
	Write(pointer,(char *)&cur_length,sizeof(cur_length));
}

PPPoEPacket::PPPoEPacket(BNetPacket *packet) : EthernetPacket(packet) {}

size_t PPPoEPacket::Length(void) {
	uint16 length;
	Read(18,(char *)&length,sizeof(length));
	FROM_NET_ENDIAN(length);
	return (size_t)length;
}

uint8 PPPoEPacket::Code(void) {
	uint8 code;
	Read(15,(char *)&code,sizeof(code));
	return code;
}

uint16 PPPoEPacket::SessionID(void) {
	uint16 sid;
	Read(16,(char *)&sid,sizeof(sid));
	FROM_NET_ENDIAN(sid);
	return sid;
}

void PPPoEPacket::SetData(const void *data,size_t length,uint16 offset) {
	EthernetPacket::SetData(data,length,offset + 6);
	uint16 packet_length = length;
	NET_ENDIAN(packet_length);
	Write(18,(char *)(&packet_length),2);
}

size_t PPPoEPacket::GetData(const void *buffer,size_t length,uint16 offset) {
	length = (length > Length()) ? Length() : length;
	return EthernetPacket::GetData(buffer,length,offset + 6);
}

DiscoveryPacket::DiscoveryPacket(uint8 code,BNetDevice *us,enet_address ac,uint16 session) : PPPoEPacket(0x8863,code,session,ac,us) {}

DiscoveryPacket::DiscoveryPacket(BNetPacket *packet) : PPPoEPacket(packet) {}

void DiscoveryPacket::PushTag(uint16 tag_type, uint16 tag_length, const void *tag_value) {
	off_t pointer = Size();
	SetSize(Size() + tag_length + 4);
	size_t length = tag_length;
	NET_ENDIAN(tag_length);
	NET_ENDIAN(tag_type);
	Write(pointer,(char *)&tag_type,2);
	Write(pointer + 2,(char *)&tag_length,2);
	if ((length > 0) && (tag_value != NULL)) {
		Write(pointer + 4,(char *)tag_value,length);
	}
	length += 4 + Length();	//-----Make PPPoE length value consistent
	tag_length = length;
	NET_ENDIAN(tag_length);
	Write(18,(char *)(&tag_length),2);
}

status_t DiscoveryPacket::GetTag(int32 index,uint16 *tag_type,uint16 *tag_length,const void **tag_value) {
	unsigned char buffer[MAX_ETH_LEN];
	size_t length = GetData(buffer,MAX_ETH_LEN,0);
	int32 tags = 0;
	uint16 temp_tag_length;
	for (int32 i = 0; i < length; ) {
		if (tags == index) {
			memcpy(tag_type,(unsigned char *)(uint32(buffer) + i),2);
			swap_data(B_UINT16_TYPE,tag_type,2,B_SWAP_BENDIAN_TO_HOST);
			i += 2;
			memcpy(tag_length,(unsigned char *)(uint32(buffer) + i),2);
			swap_data(B_UINT16_TYPE,tag_length,2,B_SWAP_BENDIAN_TO_HOST);
			i += 2;
			*tag_value = new uint8[*tag_length];
			memcpy((void *)*tag_value,(unsigned char *)(uint32(buffer) + i),*tag_length);
			return B_OK;
		}
		tags++;
		i += 2;
		memcpy(&temp_tag_length,(unsigned char *)(uint32(buffer) + i),2);
		FROM_NET_ENDIAN(temp_tag_length);
		i += 2 + temp_tag_length;
	}
	return B_ERROR;
}

SessionPacket::SessionPacket(BNetDevice *us,enet_address ac,uint16 session) : PPPoEPacket(0x8864,0x00,session,ac,us) {}

SessionPacket::SessionPacket(BNetPacket *packet) : PPPoEPacket(packet) {}

void SessionPacket::SetData(const void *data,size_t length,uint16 offset) {
	PPPoEPacket::SetData(data,length,offset);
}

uint8 *SessionPacket::RawData(size_t *size) {
	unsigned length = 4096;
	uint8 *data = (uint8 *)DataBlock(20,&length);
	*size = length;
	return data;
}

size_t SessionPacket::GetData(const void *buffer,size_t length,uint16 offset) {
	return PPPoEPacket::GetData(buffer,length,offset);
}