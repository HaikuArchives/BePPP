#include <ClassInfo.h>
#include <ByteOrder.h>
#include <StopWatch.h>
#include <NetAddress.h>
#include <socket.h>
#include <netdb.h>
#include "pptp.h"

#define DEBUG_ON 1

uint32 seq = 0;

#if DEBUG_ON
FILE *f;
void DumpPacket(BNetPacket *pkt);

void DumpPacket(BNetPacket *pkt) {
	fprintf(f, "Length=%d, ", pkt->Size());
	fprintf(f, "Entire frame follows");
	for (int32 i = 0; i < pkt->Size(); i++) {
	    fprintf(f, "%s%02x", (i % 4) == 0 ? "\n\t" : " ",
				(uchar) (pkt->Data())[i]);
	}
    fprintf(f, "\n"); fflush(f);
}

#endif

extern "C" _EXPORT BNetDevice *open_device(const char *device) {
	#if DEBUG_ON
		f = fopen("/tmp/pptp.dump","w+");
		fprintf(f,"Starting up...\n");
		fflush(f);
	#endif
	do_nothing_sem = create_sem(0,"pptp_do_nothing");
	return new PPtPDevice;
}

BNetProtocol *open_protocol(const char *device) {
	#if DEBUG_ON
		fprintf(f,"Opening Protcol...\n");
		fflush(f);
	#endif
	return new PPtPProtocol;
}

extern "C" _EXPORT BNetConfig *open_config(const char *device) {
	return new PPtPConfig;
}

bool PPtPConfig::IsIpDevice(void) {
	return true;
}

status_t PPtPConfig::Config(const char *ifname,
					net_settings *ncw,
					BCallbackHandler *callback,
					bool autoconfig) {
	return B_OK;
}

int PPtPConfig::GetPrettyName(char *name, int len) {
	strcpy(name,"PPtP Adapter");
	return 12;
}

PPtPDevice::PPtPDevice(void) {
	ppp = new PPtP(this);
}

BNetPacket *PPtPDevice::ReceivePacket(void) {
	acquire_sem(do_nothing_sem); //------We don't receive packets
	return AllocPacket();
}

BNetPacket *PPtPDevice::AllocPacket(void) {
	return new BStandardPacket;
}

void PPtPDevice::SendPacket(BNetPacket *packet) {
	delete packet; //------We don't send packets either, IpDevice handles everything
}

status_t PPtPDevice::SendPacket(uint32 dst, BNetPacket *buf) {
	delete buf;
	return B_ERROR;
}

void PPtPDevice::Address(char *address) {
		//--------In case you can't tell yet, we are are rigging all these to do nothing
}

status_t PPtPDevice::AddMulticastAddress(const char *address) {
	return B_ERROR;
}

status_t PPtPDevice::RemoveMulticastAddress(const char *address) {
	return B_ERROR;
}

status_t PPtPDevice::SetPromiscuous(bool yes) {
	return B_ERROR;
}

unsigned PPtPDevice::MaxPacketSize(void) {
	return 32767; //----Why not?----
}

net_device_type PPtPDevice::Type(void) {
	return B_LOOP_NET_DEVICE;
}

void PPtPDevice::Close(void) {
	release_sem(do_nothing_sem); //-------Make ReceivePacket stop blocking
}
	
BIpDevice *PPtPDevice::OpenIP(void) {
	return this;
}

void PPtPDevice::Statistics(FILE *f) {
	return; //----------We don't do statistics
}

unsigned PPtPDevice::Flags(void) {
	return 0;
}

void PPtPDevice::Run(BIpHandler *ip) {
	net_server = ip;
}

void PPtPDevice::Send(IpPacket *packet) {
	net_server->PacketReceived(packet,this);
}

PPtPProtocol::PPtPProtocol(void) {
	ppp = NULL;
}

void PPtPProtocol::AddDevice(BNetDevice *dev, const char *name) {
	if (dev->Type() == B_ETHER_NET_DEVICE)
		register_packet_handler(this,dev);
	if (is_kind_of(dev,PPtPDevice)) {
		ppp = ((PPtPDevice *)(dev))->ppp;
		#if DEBUG_ON
			fprintf(f,"Found Device...\n");
			fflush(f);
		#endif
	}
}

bool PPtPProtocol::PacketReceived(BNetPacket *buf, BNetDevice *dev) {
	if ((buf == NULL) || (ppp == NULL))
		return false;
	
	uint16 protocol;
	buf->Read(12, (char*)&protocol, sizeof(protocol));
			
	if(ntohs(protocol) != 0x0800)
		return false;
	IpPacket packet;
	copy_packet(buf,14,&packet,0,buf->Size() - 14);
	
	if (packet.Protocol() != 47)
		return false;
	if (GREPacket(&packet).Protocol() != 0x880b)
		return false;
	PPtPPacket *pptp_packet = new PPtPPacket(&packet);
	ppp->SendPacketToPPP(pptp_packet);
	return false;
}

PPtP::PPtP(PPtPDevice *ip) : ppp_transport("pptp") {
	device = ip;
	ac_ip = 0;
	control = 0;
}

PPtP::~PPtP(void) {
	kill_thread(control);
	#if DEBUG_ON
		fclose(f);
	#endif
	//----Nothing we need to do here----
}

PPPPacket *PPtP::AllocPacket(void) {
	return (new PPtPPacket(ac_ip,our_ip,0,seq++));
}

void PPtP::SendPacketToNet(PPPPacket *send) {
	PPtPPacket *packet = (PPtPPacket *)(send);
	device->Send(packet);
}

void PPtP::Open(void) {
	uint16 ip[4];
	control = spawn_thread(&Run,"pptp_control_connection",10,this);
	resume_thread(control);
	BStopWatch timeout("pppoe_timeout");
	for (snooze(5000);(linkUp == false) && (timeout.ElapsedTime() < 10000000);snooze(5000)) {
		//----------Wait for connection to come up---
	}
}

void PPtP::Close(void) {
	PPtPControlPacket end(Stop_Control_Connection_Request);
	
	uint32 l = 0;
	void *pointer;
	unsigned len;
	int32 result;
	
	end.SetData(&l,4,0);
	
	//-----Send it off----------
		pointer = end.DataBlock(0,&len);
		control_socket->Send(pointer,len,0);
		
	wait_for_thread(control,&result);	//-------Let it finish up
	
	control = 0;
}

void close_it(void *sock) {
	if (*((int *)(sock)) != 0)
		closesocket(*((int *)(sock)));
}

int32 PPtP::Run(void *arg) {		//---------Handle Control Connection
	PPtP *us = (PPtP *)(arg);
	
	uint8 packet_data[4096]; //----More than enough room----
	int32 packet_offset = 0;
	size_t packet_len = 0;
	int32 stream_len = 0;
	
	uint16 peer_call_id;
	
	int size;
	uint8 b;	//---byte
	uint16 s;	//---short
	uint32 l;	//---long
	void *pointer;
	char other[64];
	unsigned len;
	int32 offset;
	
	PPtPControlPacket fiddle(0);
	us->control_socket = new BNetEndpoint();
	
	on_exit_thread(&close_it,&(us->control_socket));
	
	
	
	
	#if DEBUG_ON
		fprintf(f,"Number dialed: %s...\n",us->number_dialed);
		fprintf(f,"Opening Control Connection...\n");
		fflush(f);
	#endif
	
	if (us->control_socket->Connect(us->number_dialed,1723) != B_OK) {
		us->Terminate(false,true);
		return -1;
	}
	
	#if DEBUG_ON
		fprintf(f,"Control Opened Successfully...\n");
		fflush(f);
	#endif
	in_addr addr;
	us->control_socket->LocalAddr().GetAddr(addr);
	us->our_ip = addr.s_addr;
	
	{ //-------Establish control conn.--------
		PPtPControlPacket *start = new PPtPControlPacket(Start_Control_Connection_Request);
		
		offset = 0;
		//-----Version number-----
			s = htons(0x0100);
			start->SetData(&s,2,offset);
			offset += 2;
		//-----Reserved-----------
			s = 0;
			start->SetData(&s,2,offset);
			offset += 2;
		//-----Framing Capabilities-
			l = htonl(3); //----Synchronous only
			start->SetData(&l,4,offset);
			offset += 4;
		//-----Bearer Capablilities-
			l = htonl(3); //----This is stupid. It's a computer; of course it's digital!
			start->SetData(&l,4,offset);
			offset += 4;
		//-----Maximum Channels-----
			s = 0;			//-----Only PAC sets this
			start->SetData(&s,2,offset);
			offset += 2;
		//-----Firmware Revision----
			s = htons(0x0100);	//-------Version 1
			start->SetData(&s,2,offset);
			offset += 2;
		//-----Our host name--------
			memset(other,0,64);
			gethostname(other,64);
			start->SetData(other,64,offset);
			offset += 64;
		//-----This software--------
			memset(other,0,64);
			strcpy(other,"x-vnd.nwhitehorn/pptp0.9b1");
			start->SetData(other,64,offset);
			offset += 64;
		//-----Make sure it looks like it's supposed to
		#if DEBUG_ON
			fprintf(f,"Outgoing Control Packet follows:\n");
			DumpPacket(start);
		#endif
		//-----Send it off----------
			pointer = start->DataBlock(0,&len);
			us->control_socket->Send(pointer,len,0);
		
		delete start;
	}
	
	#if DEBUG_ON
		fprintf(f,"Open request sent...\n");
		fflush(f);
	#endif
	
	while (true) {
		packet_len = us->control_socket->Receive(&(packet_data[stream_len]),4096-stream_len,0) + packet_offset;
		#if DEBUG_ON
			fprintf(f,"Got a packet: len = %d...\n",packet_len);
			fflush(f);
		#endif
		if (packet_len <= 0)
			return -5;
		memcpy(&s,packet_data,2);
		s = ntohs(s);
		#if DEBUG_ON
			fprintf(f,"Packet's reported len = %d...\n",s);
			fflush(f);
		#endif
		if (packet_len < s)
			continue;
		fiddle.SetSize(s);
		fiddle.Write(0,(char *)packet_data,s);
		if (packet_len > s)
			memcpy(packet_data,&(packet_data[s]),packet_len-s);
		stream_len -= s;
		switch (fiddle.Message()) {
			case Start_Control_Connection_Reply:
				#if DEBUG_ON
					fprintf(f,"Got a Start_Control_Connection_Reply...\n");
					fflush(f);
				#endif
				fiddle.GetData(&b,1,2);
				if (b != 1) { //------------------Eek!
					us->Terminate(false,true);
					return -2;
				}
				//------Send an Outgoing Call Request
				fiddle.SetMessage(Outgoing_Call_Request);
				fiddle.SetSize(12);		//-------Just the header
				offset = 0;
				//-----Call ID-----------------------
					s = 0; //--------Can't have > 1 from here
					fiddle.SetData(&s,2,offset);
					offset += 2;
				//-----Call Serial--------------------
					s = 0; //--------Can't have > 1 from here
					fiddle.SetData(&s,2,offset);
					offset += 2;
				//-----Min BPS-----------------------
					l = htonl(2400); //--------We don't care about line speed
					fiddle.SetData(&l,4,offset);
					offset += 4;
				//-----Max BPS-----------------------
					l = 0xffffffff; //--------And high speeds are OK, and endianness doens't matter: same <-->
					fiddle.SetData(&l,4,offset);
					offset += 4;
				//-----Bearer Type-------------------
					l = htonl(3); //--------Analog or Digital doesn't matter
					fiddle.SetData(&l,4,offset);
					offset += 4;
				//-----Bearer Type-------------------
					l = htonl(3); //--------Analog or Digital doesn't matter
					fiddle.SetData(&l,4,offset);
					offset += 4;
				//-----Framing Type-------------------
					l = htonl(3); //--------Async or Sync doesn't matter, either
					fiddle.SetData(&l,4,offset);
					offset += 4;
				//-----Reserved-----------------------
					s = 0;
					fiddle.SetData(&s,2,offset);
					offset += 2;
				//-----Phone #------------------------
					memset(other,0,64); //-----We aren't dialing anywhere
					fiddle.SetData(other,64,offset);
					offset += 64;
				//-----Subaddres------------------------
					memset(other,0,64); //-----Subaddr == stupidity
					fiddle.SetData(other,64,offset);
					offset += 64;
					
				//-----Send it off----------
					pointer = fiddle.DataBlock(0,&len);
					us->control_socket->Send(pointer,len,0);
				break;
			case Stop_Control_Connection_Request:
				#if DEBUG_ON
					fprintf(f,"Got a Stop_Control_Connection_Request...\n");
					fflush(f);
				#endif
				fiddle.SetMessage(Stop_Control_Connection_Reply);
				//------Change reason to result OK
					b = 1;
					fiddle.SetData(&b,1,0);
				//-----Send it off----------
					pointer = fiddle.DataBlock(0,&len);
					us->control_socket->Send(pointer,len,0);
				//-----Terminate our End----
					us->Terminate(false,true);
				return 0;
			case Stop_Control_Connection_Reply:
				#if DEBUG_ON
					fprintf(f,"Got a Stop_Control_Connection_Reply...\n");
					fflush(f);
				#endif
				return 0; //------Do nothing more. If there was error, that's just too bad.
			case Echo_Request:
				#if DEBUG_ON
					fprintf(f,"Got a Echo_Request...\n");
					fflush(f);
				#endif
				fiddle.SetMessage(Echo_Reply);
				//-----Send it off----------
					pointer = fiddle.DataBlock(0,&len);
					us->control_socket->Send(pointer,len,0);
				break;
			case Outgoing_Call_Reply:
				#if DEBUG_ON
					fprintf(f,"Got a Outgoing_Call_Reply...\n");
					fflush(f);
				#endif
				fiddle.GetData(&b,1,4);
				if (b != 1) //-------Dang it!
					return -3;
				fiddle.GetData(&peer_call_id,2,2);
				us->linkUp = true;
				break;
			case Incoming_Call_Request:	//-------We should NEVER, NEVER get one of these
				#if DEBUG_ON
					fprintf(f,"Got a Incoming_Call_Request...\n");
					fflush(f);
				#endif
				fiddle.GetData(&peer_call_id,2,0);
				fiddle.SetMessage(Incoming_Call_Reply);
				fiddle.SetSize(12);		//-------Just the header
				offset = 0;
				//-----Call ID-----------------------
					s = 0; //--------Can't have > 1 from here
					fiddle.SetData(&s,2,offset);
					offset += 2;
				//-----Peer Call ID------------------
					fiddle.SetData(&peer_call_id,2,offset);
					offset += 2;
					peer_call_id = 0; //-------Not a real connection
				//-----Result Code-------------------
					b = 3; //--------Error------
					fiddle.SetData(&b,1,offset);
					offset += 1;
				//-----Error Code--------------------
					b = 0; //--------Error------
					fiddle.SetData(&b,1,offset);
					offset += 1;
				//-----More Zeros--------------------
					s = 0; //--------Error------
					fiddle.SetData(&s,2,offset);
					offset += 2;
				//-----More Zeros--------------------
					s = 0; //--------Error------
					fiddle.SetData(&s,2,offset);
					offset += 2;
				//-----More Zeros--------------------
					s = 0; //--------Error------
					fiddle.SetData(&s,2,offset);
					offset += 2;
				//-----Send it off----------
					pointer = fiddle.DataBlock(0,&len);
					us->control_socket->Send(pointer,len,0);
				break;
			case Call_Disconnect_Notify:
				#if DEBUG_ON
					fprintf(f,"Got a Call_Disconnect_Notify...\n");
					fflush(f);
				#endif
				us->Terminate(true,true);
				break;
		}
	}
}