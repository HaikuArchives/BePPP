#include <add-ons/net_server/NetPacket.h>

enum {
	challenge = 1,
	response,
	success,
	failure
}; //----Codes

class CHAPPacket : public BStandardPacket {
	public:
		CHAPPacket(uint8 code,uint8 id);
		CHAPPacket(BNetPacket *packet);
		
		uint8 Code(void);
		uint8 ID(void);
		
		virtual void SetData(const void *data,size_t length,uint16 offset = 0);
		virtual size_t GetData(const void *buffer,size_t length,uint16 offset = 0);
};