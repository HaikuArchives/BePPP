#ifndef _IP_PACKET_H
#define _IP_PACKET_H
#include <add-ons/net_server/NetPacket.h>

enum {
	//---delay
	normal_delay 		= 0x00,
	low_delay 			= 0x10,
	//---throughput
	normal_tp 			= 0x00,
	high_tp				= 0x08,
	//---reliability
	normal_reli			= 0x00,
	high_reli			= 0x04,
	//---precedence
	net_control			= 0xe0,
	internet_control	= 0xc0,
	CRITIC				= 0xa0,
	flash_override		= 0x80,
	flash				= 0x60,
	immediate			= 0x40,
	priority			= 0x20,
	routine				= 0x00
}; //---------Service types

enum {
	may_fragment 	= 0x00,
	dont_fragment 	= 0x20,
	last_fragment	= 0x00,
	more_fragments	= 0x40
}; //---------IP Flags


class IpPacket : public BStandardPacket {
	public:
		IpPacket(uint32 to_ip,uint32 from_ip,uint8 protocol,uint8 ttl,uint8 service,uint8 flags,uint16 frag_offset = 0);
		IpPacket(const void *data,size_t size);
		IpPacket(void);
		
		virtual uint8 Protocol(void);
		virtual uint16 PacketID(void);
		
		uint32 Source(void);
		uint32 Destination(void);
		
		void SetSource(uint32 ip);
		void SetDestination(uint32 ip);
		
		bool MoreComing(void);
		
		virtual void SetData(const void *data,size_t length,uint16 offset = 0);
		virtual size_t GetData(const void *buffer,size_t length,uint16 offset = 0);
	private:
		void CalculateChecksum(void);
};

#endif