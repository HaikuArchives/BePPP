#include "ppp_packet.h"
#include <OS.h>
#include <limits.h>
#include <SerialPort.h>

class ppp_transport {
	public:
		ppp_transport(const char *port);
		
		void SendPacketToPPP(PPPPacket *recv);
		virtual void SendPacketToNet(PPPPacket *send) = 0;
		virtual PPPPacket *AllocPacket(void) = 0;
		
		virtual void Open(void) = 0;
		virtual void Close(void) = 0;
		
		void Terminate(bool kill_net = true,bool kill_serial = true);
		
		~ppp_transport(void);
	protected:
		volatile bool linkUp;
		char number_dialed[255];
	private:
		size_t WriteBuffer(void *data,size_t length);
		void SlideBuffer(off_t distance,off_t offset = 0);
		void DataReceived(off_t offset);
		
		static int32 watch_port(void *us);
		int32 Modem(void);
		
		status_t alloc_pty(char *buffer);
		
		void *out_buffer_data;
		ssize_t out_buffer_size, out_buf_max;
		
		bool drop_second_ipcp;
		
		thread_id watcher;
		
		BSerialPort serial;
		const char *port_name;
		
		int32 mtu;
};