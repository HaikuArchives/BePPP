#include <add-ons/net_server/NetProtocol.h>
#include <support/List.h>
#include <support/String.h>
#include <support/StopWatch.h>
#include <string.h>
#include <unistd.h>
#include "pppoe_packet.h"
#include "ppp_transport.h"

#if DEBUG_ON
	extern FILE *f;
	#include "debug.h"
#endif

#define DISCOVERY_TIMEOUT 10000000//µs

class PPPoE : public BNetProtocol, public BPacketHandler, public ppp_transport  {
	public:
		PPPoE(void);
		void AddDevice(BNetDevice *dev, const char *name);
		bool PacketReceived(BNetPacket *recv, BNetDevice *from);
		PPPPacket *AllocPacket(void);
		void SendPacketToNet(PPPPacket *send);
		void Discover(BNetPacket *last = NULL,bool sync = true);
		void Open(void);
		void Close(void);
		~PPPoE(void);
	private:
		BList devices;
		enet_address ac;
		uint16 session_id;
		BNetDevice *active;
};

BNetProtocol *open_protocol(const char *device) {
	PPPoE *prot = new PPPoE;
	return (BNetProtocol *)(prot);
}

PPPoE::PPPoE (void) : BNetProtocol() , ppp_transport("pppoe") {
	active = NULL;
	session_id = 0;
	#if DEBUG_ON
		just_started = true;
		f = fopen(DUMPTO, "w+");
	#endif
}

void PPPoE::AddDevice(BNetDevice *dev, const char *name) {
	if (dev->Type() != B_ETHER_NET_DEVICE)
		return;
	register_packet_handler(this,dev,10);
	devices.AddItem(dev);
}

void PPPoE::Open(void) {
	Discover();
}

void PPPoE::Close(void) {
	if (active == NULL)
		return;
	DiscoveryPacket *term = new DiscoveryPacket(PADT,active,ac,session_id);
	term->Finalize();
	active->SendPacket(term);
	active = NULL;
	session_id = 0;
}

void PPPoE::SendPacketToNet(PPPPacket *send) {
	if (active == NULL)
		return;
	SessionPacket *packet = (SessionPacket *)(send);
	packet->Finalize();
	#if DEBUG_ON
		fprintf(f,"Outgoing Packet:\n");
		DumpPacket(packet);
	#endif
	active->SendPacket(packet);
}

PPPPacket *PPPoE::AllocPacket(void) {
	return new SessionPacket(active,ac,session_id);
}

bool PPPoE::PacketReceived(BNetPacket *recv, BNetDevice *from) {
	if (recv == NULL)
		return false;
	SessionPacket generic(recv);
	if ((generic.Protocol() != 0x8863) && (generic.Protocol() != 0x8864))
		return false;
	#if DEBUG_ON
		fprintf(f,"Incoming Packet:\n");
		DumpPacket(recv);
	#endif
	if (generic.Protocol() == 0x8863) {
		uint8 code = generic.Code();
		if ((code == PADO) || (code == PADS)) {
			Discover(recv,false);
			delete recv;
			return true;
		}
		if ((code == PADT) && (generic.SessionID() == session_id)) {
			Terminate(false,true);
			delete recv;
			return true;
		}
		return false;
	}
	if ((generic.SessionID() != session_id) || (active == NULL))
		return false;
	
	SendPacketToPPP(&generic);
	delete recv;
	return true;
}

void PPPoE::Discover(BNetPacket *last,bool sync) {
	if (devices.CountItems() == 0)
		return;
	if (last == NULL) {
		DiscoveryPacket *init;
		active = NULL;
		for (int32 i = 0;i < devices.CountItems();i++) {
			init = new DiscoveryPacket(PADI,(BNetDevice *)(devices.ItemAt(i)),(uint8 *)broadcast_address);
			init->PushTag(SERVICE_NAME,0,NULL);
			init->PushTag(END_OF_LIST,0,NULL);
			init->Finalize();
			#if DEBUG_ON
				fprintf(f,"\nDumping our PADI\n");
				fflush(f);
				DumpPacket(init);
				fprintf(f,"\nThat's the PADI\n");
				fflush(f);
			#endif
			((BNetDevice *)(devices.ItemAt(i)))->SendPacket(init);
			#if DEBUG_ON
				fprintf(f,"\nPADI Sent");
				fflush(f);
			#endif
		}
		if (sync) {
			BStopWatch timeout("pppoe_timeout");
			for (snooze(5000);(linkUp == false) && (timeout.ElapsedTime() < DISCOVERY_TIMEOUT);snooze(5000)) {}
		}
		return;
	}
	DiscoveryPacket * old, * to_send;
	old = new DiscoveryPacket(last);
	uint8 code = old->Code();
	if ((code == PADO) && (active == NULL)){
		#if DEBUG_ON
			fprintf(f,"Snagged a PADO\n\tCode: %02x");
			fflush(f);
		#endif
		
		enet_address test,real_us;
		old->Destination(&real_us);
		old->Source(&ac);
		for (int32 i = 0;i < devices.CountItems();i++) {
			DeviceAddress((BNetDevice *)(devices.ItemAt(i)),&test);
			if (memcmp(test,real_us,MAC_ADDR_LEN) == 0) {
				active = (BNetDevice *)(devices.ItemAt(i));
				break;
			}
		}
		
		#if DEBUG_ON
			fprintf(f, "\n\tAC: [");
			for (int i = 0; i < MAC_ADDR_LEN; i++)
				fprintf(f, "%02x%s", (uchar) ac[i],(i == MAC_ADDR_LEN -1) ? "], " : ":");

			fprintf(f, "\n\tActive: [");
			for (int i = 0; i < MAC_ADDR_LEN; i++)
				fprintf(f, "%02x%s", (uchar) test[i],(i == MAC_ADDR_LEN -1) ? "], " : ":");
			fprintf(f,"\n");
			fflush(f);
		#endif
		to_send = new DiscoveryPacket(PADR,active,ac);
		uint8 *tag_buffer;
		uint16 type;
		uint16 tag_length;
		int services = 0;
		for (int32 i = 0;old->GetTag(i,&type,&tag_length,(const void **)&tag_buffer) == B_OK;i++) {
			if ((type == SERVICE_NAME) && (services != 0))
				continue;
			if (type == SERVICE_NAME)
				services++;
			if ((type == AC_COOKIE) || (type == RELAY_SESSION_ID) || (type == SERVICE_NAME)) {
				#if DEBUG_ON
					fprintf(f,"Got a resend tag of length %d at tag %d of type %04x\n",tag_length,i,type);
					fflush(f);
				#endif
				to_send->PushTag(type,tag_length,tag_buffer);
			}
			delete [] tag_buffer;
		}
		to_send->PushTag(END_OF_LIST,0,NULL);
		to_send->Finalize();
		#if DEBUG_ON
			fprintf(f,"Sending a PADR:\n");
			DumpPacket(to_send);
		#endif
		active->SendPacket(to_send);
		if (sync) {
			BStopWatch timeout("pppoe_timeout");
			for (snooze(5000);(linkUp == false) && (timeout.ElapsedTime() < DISCOVERY_TIMEOUT);snooze(5000)) {}
		}
		return;
	}
	if (code == PADS) {
		session_id = old->SessionID();
		#if DEBUG_ON
			fprintf(f,"Got a PADS\nsession_id: %04x",session_id);
			fflush(f);
		#endif
		if (session_id != 0)
			linkUp = true;
	}
	if (sync) {
		BStopWatch timeout("pppoe_timeout");
		for (snooze(5000);(linkUp == false) && (timeout.ElapsedTime() < DISCOVERY_TIMEOUT);snooze(5000)) {}
	}
}

PPPoE::~PPPoE(void) {
	#if DEBUG_ON
		fprintf(f,"We've been deleted! Phooey.");
		fflush(f);
		fclose(f);
	#endif
	for (int32 i = 0; i < devices.CountItems(); i++) {
		unregister_packet_handler(this,(BNetDevice *)devices.ItemAt(i));
	}
}
