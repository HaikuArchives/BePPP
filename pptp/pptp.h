
#include <add-ons/net_server/IpDevice.h>
#include <add-ons/net_server/NetDevice.h>
#include <add-ons/net_server/NetProtocol.h>
#include <NetEndpoint.h>
#include <string.h>
#include "ip_packet.h"
#include "ppp_packet.h"
#include "pptp_packet.h"
#include "ppp_transport.h"

sem_id do_nothing_sem;

class PPtP;
class PPtPProtocol;

//---Now if this is isn't called jumping through hoops, I don't know what is
//---I only want to make a measly little one-byte change in the IP header, but I have to do all this. Ick.

class PPtPDevice : public BNetDevice, BIpDevice {
	public:
		//--------Talk to net_server, do absolutely nothing
		PPtPDevice(void);
		
		BNetPacket *ReceivePacket(void);
		BNetPacket *AllocPacket(void);
		void SendPacket(BNetPacket *packet);
		status_t SendPacket(uint32 dst, BNetPacket *buf);
		void Address(char *address);
		
		status_t AddMulticastAddress(const char *address);
		status_t RemoveMulticastAddress(const char *address);
		status_t SetPromiscuous(bool yes);
		unsigned MaxPacketSize(void);
		net_device_type Type(void);
		void Close(void);
	
		BIpDevice *OpenIP(void);
		void Statistics(FILE *f);
		unsigned Flags(void);
		void Run(BIpHandler *ip);
		//--------Talk to ppp_transport
		void Send(IpPacket *packet);
	private:
		friend class PPtPProtocol;
		PPtP *ppp;
		BIpHandler *net_server;
};

class PPtPConfig : public BNetConfig {
	public:
		bool IsIpDevice(void);

		status_t Config(const char *ifname,
							net_settings *ncw,
							BCallbackHandler *callback,
							bool autoconfig = false);

		int GetPrettyName(char *name, int len);
};

class PPtPProtocol : public BNetProtocol, BPacketHandler {
	public:
		PPtPProtocol(void);
		void AddDevice(BNetDevice *dev, const char *name);
		bool PacketReceived(BNetPacket *buf, BNetDevice *dev);
	private:
		PPtP *ppp;
};

class PPtP : public ppp_transport{
	public:
		//------Housekeeping
		PPtP(PPtPDevice *ip);
		~PPtP(void);
		//------ppp_tranport funcs
		PPPPacket *AllocPacket(void);
		void SendPacketToNet(PPPPacket *send);
		void Open(void);
		void Close(void);
		
		static int32 Run(void *arg);
		
	private:
		friend class PPtPDevice;
		BNetEndpoint *control_socket;
		PPtPDevice *device;
		uint32 ac_ip;
		uint32 our_ip;
		thread_id control;
};