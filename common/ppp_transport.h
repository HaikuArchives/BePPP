#include "ppp_packet.h"
#include <OS.h>
#include <limits.h>

typedef enum {
	in_buffer,
	out_buffer
} buffer_type;

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
		size_t ReadBuffer(void *data,size_t length,buffer_type buffer,off_t offset = 0,bool slide = true);
		size_t WriteBuffer(void *data,size_t length,buffer_type buffer);
		void SlideBuffer(off_t distance,off_t offset = 0,buffer_type buffer = out_buffer);
		void MessageReceived(const void *data,size_t length);
		
		static int32 watch_port(void *us);
		static int32 Modem(void *pointer);
		
		void *out_buffer_data;
		ssize_t out_buffer_size, out_buf_max;
		
		bool drop_second_ipcp;
		
		thread_id watcher,modem;
		sem_id read_write;
		int pty_fd;
		const char *port_name;
		
		int32 mtu;
};