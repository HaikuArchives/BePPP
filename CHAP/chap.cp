#include <add-ons/net_server/NetProtocol.h>
#include <ByteOrder.h>
#include <string.h>
#include <File.h>
#include <Message.h>

#include "chap_packet.h"
#include "mhash.h"

#define DEBUG_ON 1

#if DEBUG_ON
FILE *f;
#endif

class CHAP : public BNetProtocol, public BPacketHandler {
	public:
		CHAP(void);
		
		void AddDevice(BNetDevice *dev, const char *name);
		bool PacketReceived(BNetPacket *buf, BNetDevice *dev);
		
	private:
		BNetDevice *ppp;
		char *username;
		char *passwd;
};

BNetProtocol *open_protocol(const char *device) {
	return new CHAP;
}

CHAP::CHAP(void) {
	ppp = NULL;
	
	#if DEBUG_ON
		f = fopen("/tmp/chap.dump", "w+");
	#endif
	
	char path[256];
	BFile data("/boot/home/config/settings/DUN/DUN_data",B_READ_ONLY);
	BMessage *archive = new BMessage;
	archive->Unflatten(&data);
	sprintf(path,"/boot/home/config/settings/DUN/DUNCONNECT/%s",archive->FindString("curr_profile"));
	data.SetTo(path,B_READ_ONLY);
	delete archive;
	archive = new BMessage;
	archive->Unflatten(&data);
	archive->FindString("username",&username);
	archive->FindString("password",&passwd);
	delete archive;
	
	#if DEBUG_ON
		fprintf(f,"Path: %s\n",path);
		fprintf(f,"Username: %s\n",username);
		fprintf(f,"Password: %s\n",passwd);
		fflush(f);
	#endif
}

void CHAP::AddDevice(BNetDevice *dev, const char *name) {
	if (dev->Type() == B_PPP_NET_DEVICE) {
		ppp = dev;
		register_packet_handler(this,dev,10/*ahead of LCP*/);
	}
}

bool CHAP::PacketReceived(BNetPacket *buf, BNetDevice *dev) {
	CHAPPacket *pkt;
	
	uint16 protocol;
	buf->Read(2,(char *)&protocol,2);
	
	switch (ntohs(protocol)) {
		case 0xc021:
			{
				uint8 code, type, opt_length, algthrm;
				uint16 packet_length, auth_proto;
				void *end;
				
				buf->Read(4,(char *)&code,1);
				if (code != 1)
					return false;
				buf->Read(6,(char *)&packet_length,2);
				packet_length = ntohs(packet_length);
				
				for (int32 i = 8; i < packet_length; i++) { //----Iterate through packet for CHAP config option
					buf->Read(i,(char *)&type,1);
					buf->Read(i+1,(char *)&opt_length,1);
					if (type != 3)
						goto next_option;
					buf->Read(i+2,(char *)&auth_proto,2);
					if (ntohs(auth_proto) != 0xc223)
						goto next_option;
					buf->Read(i+4,(char *)&algthrm,1);
					if (algthrm != 5)
						goto next_option;
					else {
						uint16 old_length;
						
						#if DEBUG_ON
							fprintf(f,"Snipped Option\n");
							fflush(f);
						#endif
						
						/*snip option, so PPP will confirm*/
						end = new uint8[packet_length - i - opt_length];
						buf->Read(i+opt_length,(char *)end,packet_length - i - opt_length);
						buf->SetSize(buf->Size() - opt_length);
						buf->Write(i,(char *)end,packet_length - i - opt_length);
						buf->Read(6,(char *)&old_length,2);
						old_length = htons(ntohs(old_length) - opt_length);
						buf->Write(6,(char *)&old_length,2);
						delete [] end;
						return false; /* pass it on to LCP*/
					}
					
				next_option:
					i += opt_length -1/*We're about to increment*/;
				}
			}
			break;
		case 0xc223:
			pkt = new CHAPPacket(buf);
			switch (pkt->Code()) {
				case challenge:
					{
						uint8 value_size;
						uint8 *value;
						MHASH thread;
						CHAPPacket *response_pkt;
						
						pkt->GetData(&value_size,1,0);
						value = new uint8[value_size+1+strlen(passwd)];
						
						pkt->GetData((void *)(uint32(value)+1+strlen(passwd)),value_size,1);
						value[0] = pkt->ID();
						memcpy(&(value[1]),passwd,strlen(passwd));
						
						thread = mhash_init(MHASH_MD5);
						mhash(thread,value,value_size+1+strlen(passwd));
						response_pkt = new CHAPPacket(response,pkt->ID());
						value_size = 16;
						response_pkt->SetData(&value_size,1,0);
						response_pkt->SetData(mhash_end(thread),16,1);
						response_pkt->SetData(username,strlen(username),17);
						ppp->SendPacket(response_pkt);
						delete [] value;
					}
					break;
				case success:
					/*Yay! But do nothing*/
					break;
				case failure:
					/*should notify here*/
					break;
			}
			delete pkt;
			return true;
		}
		return false;
	}