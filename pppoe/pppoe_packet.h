#include "enet_packet.h"
#include "ppp_packet.h"

enum { //-----------Tag_types
	END_OF_LIST 		= 0x0000,
	SERVICE_NAME 		= 0x0101,
	AC_NAME				= 0x0102,
	HOST_UNIQ			= 0x0103,
	AC_COOKIE			= 0x0104,
	VENDOR_SPECIFIC		= 0x0105,
	RELAY_SESSION_ID	= 0x0110,
	SERVICE_NAME_ERROR	= 0x0201,
	AC_SYSTEM_ERROR		= 0x0202,
	GENERIC_ERROR		= 0x0203
};

enum { //-----------CODEs
	PADI	= 0x09,		//--------Discovery Initiation
	PADO	= 0x07,		//--------AC Offer
	PADR	= 0x19,		//--------Discovery Request
	PADS	= 0x65,		//--------Request Confirmation
	PADT	= 0xa7		//--------Session Termination
};

class PPPoEPacket : public EthernetPacket {
	public:
		PPPoEPacket(uint16 protoid,uint8 code,uint16 session_id,enet_address ac,BNetDevice *us);
		PPPoEPacket(BNetPacket *packet);
		
		size_t Length(void);
		
		uint8 Code(void);
		uint16 SessionID(void);
		
		virtual void SetData(const void *data,size_t length,uint16 offset = 0); //---Less than 1522 bytes
		virtual size_t GetData(const void *buffer,size_t length,uint16 offset = 0);
};

class DiscoveryPacket : public PPPoEPacket {
	public:
		DiscoveryPacket(uint8 code,BNetDevice *us,enet_address ac = NULL,uint16 session = 0x0000);
		DiscoveryPacket(BNetPacket *packet);
		
		void PushTag(uint16 tag_type, uint16 tag_length, const void *tag_value);
		status_t GetTag(int32 index, uint16 *tag_type, uint16 *tag_length, const void **tag_value); //----delete[] this
};

class SessionPacket : public PPPPacket, public PPPoEPacket{
	public:
		SessionPacket(BNetDevice *us,enet_address ac,uint16 session);
		SessionPacket(BNetPacket *packet);
		void SetData(const void *data,size_t length,uint16 offset = 0); //---Less than 1522 bytes
		size_t GetData(const void *buffer,size_t length,uint16 offset = 0);
};