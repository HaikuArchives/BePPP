#include <add-ons/net_server/NetPacket.h>
#include <add-ons/net_server/NetDevice.h>
#include <support/ByteOrder.h>

#define MAC_ADDR_LEN       6
#define ETHDR_PROTOID_OFS  12
#define MAX_ETH_LEN        1536

#define NET_ENDIAN(a)  swap_data(B_UINT16_TYPE,&a,sizeof(a),B_SWAP_HOST_TO_BENDIAN)
#define FROM_NET_ENDIAN(a)  swap_data(B_UINT16_TYPE,&a,sizeof(a),B_SWAP_BENDIAN_TO_HOST)

enum {
	PROTOID_IP     = 0x0800,
	PROTOID_ARP    = 0x0806,
	PROTOID_RARP   = 0x8035,
	PROTOID_APLTK  = 0x809b
};

typedef uint8 enet_address[MAC_ADDR_LEN];

const enet_address broadcast_address = {0xff,0xff,0xff,0xff,0xff,0xff};

class EthernetPacket : public BNetPacket /* BStandardPacket is a piece of crap, so do it our own way */ {
	public:
		/* E-net specific funcs */
		EthernetPacket(enet_address to,enet_address from,uint16 protoid,size_t size = 0);
		EthernetPacket(enet_address to,BNetDevice *from,uint16 protoid,size_t size = 0);
		
		EthernetPacket(BNetPacket *packet);
		
		void Destination(enet_address *addr);
		void Source(enet_address *addr);
		uint16 Protocol(void);
		
		virtual void SetData(const void *data,size_t length,uint16 offset = 0); //---Less than 1522 bytes
		virtual size_t GetData(const void *buffer,size_t length,uint16 offset = 0);
		
		virtual void Finalize(void);
		
		/* BNetPacket funcs */
		virtual unsigned Size(void);
		virtual void SetSize(unsigned size);
		virtual unsigned Base(void);		
		virtual void SetBase(int offset);	
		virtual char *DataBlock(unsigned offset, unsigned *size);
		
		virtual ~EthernetPacket(void);
		
	private:
		void *data;
		size_t size;
		int32 base;
};
		
void DeviceAddress(BNetDevice *dev,enet_address *addr);