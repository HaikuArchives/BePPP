#include <add-ons/net_server/NetPacket.h>
#include <SupportDefs.h>
#include "ip_packet.h"
#include "ppp_packet.h"

enum {
	Start_Control_Connection_Request	= 1,
	Start_Control_Connection_Reply		= 2,
	Stop_Control_Connection_Request		= 3,
	Stop_Control_Connection_Reply		= 4,
	Echo_Request						= 5,
	Echo_Reply							= 6,

	Outgoing_Call_Request				= 7,
	Outgoing_Call_Reply					= 8,
	Incoming_Call_Request				= 9,
	Incoming_Call_Reply					= 10,
	Incoming_Call_Connected				= 11,
	Call_Clear_Request					= 12,
	Call_Disconnect_Notify				= 13,

	WAN_Error_Notify					= 14,
	
	Set_Link_Info						= 15
};	//--------message arg for PPtPControlPacket

class PPtPControlPacket : public BStandardPacket {
	public:
		PPtPControlPacket(uint16 message);
		
		uint16 Message(void);
		void SetMessage(uint16 msg);

		bool IsValid(void);
		
		virtual void SetData(const void *data,size_t length,uint16 offset = 0);
		virtual size_t GetData(const void *buffer,size_t length,uint16 offset = 0);
};

class GREPacket : public IpPacket {
	public:
		GREPacket(uint32 dest,uint32 src,uint16 protocol,uint16 session,uint32 seq = 0,bool has_seq = false,uint32 ack = 0,bool has_ack = false);
		GREPacket(BNetPacket *packet);
		
		uint32 AckNum(void);
		uint32 SeqNum(void);
		
		bool HasAck(void);
		bool HasSeq(void);
		
		uint16 ProtocolValue(void);
		
		virtual void SetData(const void *data,size_t length,uint16 offset = 0);
		virtual size_t GetData(const void *buffer,size_t length,uint16 offset = 0);
};

class PPtPPacket : public GREPacket, public PPPPacket {
	public:
		PPtPPacket(uint32 dest, uint32 src,uint16 call_id,uint32 seq,uint32 ack = 0,bool has_ack = false);
		PPtPPacket(BNetPacket *packet);
		
		void SetData(const void *data,size_t length,uint16 offset = 0);
		size_t GetData(const void *buffer,size_t length,uint16 offset = 0);
};
