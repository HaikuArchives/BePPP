#include <add-ons/net_server/NetProtocol.h>
#include <support/List.h>
#include <support/String.h>
#include <support/StopWatch.h>
#include <string.h>
#include <unistd.h>
#include <net_settings.h>
#include <TypeConstants.h>
#include <ByteOrder.h>
#include <malloc.h>
#include "ppp_transport.h"

#if DEBUG_ON
	extern FILE *f;
#endif

#if ASSERTIONS
	#include <assert.h>
#endif

#define NET_ENDIAN(a)  swap_data(B_UINT16_TYPE,&a,sizeof(a),B_SWAP_HOST_TO_BENDIAN)
#define FROM_NET_ENDIAN(a)  swap_data(B_UINT16_TYPE,&a,sizeof(a),B_SWAP_BENDIAN_TO_HOST)

#define WATCH_PORT_PRIORITY 10

#if TIMED
int total_recv_packets = 0;
int total_sent_packets = 0;
FILE *t;
#endif

status_t ppp_transport::alloc_pty (char *buffer) {
	char *letters = "pqrs";
	char *numbers = "0123456789abcdef";
	char *i, *j;
	
	for (i = letters; i[0]; i++) {
		for (j = numbers; j[0]; j++) {
			sprintf(buffer,"../pt/%c%c",*i,*j);
			if (serial.Open(buffer) > 0) {
				sprintf(buffer,"/dev/pt/%c%c",*i,*j);
				return B_OK;
			}
		}
	}
	
	return B_ERROR;
}

int32 ppp_transport::watch_port(void *us) {
	ppp_transport *obj = (ppp_transport *)(us);
	
	ssize_t bufferSize;
	off_t old_length;
	char port_path[64];
	obj->serial.SetTimeout(B_INFINITE_TIMEOUT);
	sprintf(port_path,"/dev/ports/%s",obj->port_name);
		
	while (true) {
		bufferSize = obj->serial.WaitForInput();
		if (bufferSize < 0) {			//--------We got closed. Phooey.
			char pty_name[15];
			obj->alloc_pty(pty_name);
			pty_name[5] = 't';
			unlink(port_path);
			symlink(pty_name,port_path);
			continue;
		}
		
		if (bufferSize == 0)
			continue;
		
		old_length = obj->out_buffer_size;
		if (obj->out_buffer_data == NULL)
			obj->out_buffer_data = malloc(bufferSize); //---realloc(NULL) only works on PPC
		 else
		 	obj->out_buffer_data = realloc(obj->out_buffer_data,obj->out_buffer_size + bufferSize);

		obj->serial.Read((void *)((uint32)(obj->out_buffer_data) + (uint32)obj->out_buffer_size),bufferSize);
		
		obj->out_buffer_size += bufferSize;
	
		obj->DataReceived(old_length);
		
		if (obj->out_buffer_size == 0) {
			free(obj->out_buffer_data);
			obj->out_buffer_data = NULL;
		}
	}
	return 0;
}

void ppp_transport::Modem(void) {							//---------Our lovely fake modem-------
	if (out_buffer_size <= 2)
		return;
	char last_char = ((uint8 *)(out_buffer_data))[out_buffer_size - 1];
	if ((last_char != '\r') && (last_char != '\n'))
		return;
	
	char *response;
	switch (((uint8 *)out_buffer_data)[2]) {
		case 'D': //--Dial
			Open();
			if (!(linkUp))
				response = "NO CARRIER\n\r";
			else
				response = "CONNECT 256000\n\r";
			break;
		case 'H':	//--Hang up
			serial.Write("OK\n\r",4);
			snooze(500);
			serial.Close();
			out_buffer_size = 0;
			return;
		default: //--Init strings
			response = "OK\n\r";
			break;
	}
	serial.Write(response,strlen(response));
	out_buffer_size = 0;
}

ppp_transport::ppp_transport (const char *port) {
	out_buffer_data = NULL;
	out_buffer_size = 0;
	
	drop_second_ipcp = false;
	char text_to_fiddle[64];
	mtu = -1;
	linkUp = false;
	port_name = port;
	char pty_name[15];
	alloc_pty(pty_name);
	sprintf(text_to_fiddle,"%s_watcher",port);
	watcher = spawn_thread(&watch_port,text_to_fiddle,WATCH_PORT_PRIORITY,this);
	pty_name[5] = 't';
	sprintf(text_to_fiddle,"/dev/ports/%s",port);
	unlink(text_to_fiddle);
	symlink(pty_name,text_to_fiddle);
	
	/* Read setting for drop_second_ipcp */
	find_net_setting(NULL,port,"drop_second_ipcp",text_to_fiddle,64);
	drop_second_ipcp = !(BString(text_to_fiddle).IFindFirst("false") != -1);
	/* Read setting for mtu */
	text_to_fiddle[0] = 0; //---find_net_setting won't change it if there's nothing
	find_net_setting(NULL,port,"mtu",text_to_fiddle,64);
	if (text_to_fiddle[0] != 0)
		sscanf(text_to_fiddle,"%d",&mtu);
	
	resume_thread(watcher);
	#if TIMED
		sprintf(text_to_fiddle,"/tmp/%s_timers.txt",port);
		t = fopen(text_to_fiddle, "w+");
	#endif
}

void ppp_transport::Terminate(bool kill_net,bool kill_serial) {
	if (linkUp == false)
		return;
	if (kill_net)
		Close();
	if (kill_serial) {
		serial.Write("NO CARRIER\r\n",12);
		snooze(500);
		serial.Close();
	}
	out_buffer_size = 0;
	linkUp = false;
	#if TIMED
		total_recv_packets = 0;
		total_sent_packets = 0;
	#endif
}

void ppp_transport::SendPacketToPPP(PPPPacket *recv) {
	static uint8 data[B_PAGE_SIZE] /* don't keep reallocating */;
	if (recv == NULL)
		return;
	size_t size = recv->GetAsyncFrame(data,B_PAGE_SIZE,mtu);
	#if ASSERTIONS
		assert(size < 4096);
	#endif
	serial.Write(data,size);
}

void ppp_transport::DataReceived(off_t offset) {
	if ((linkUp == false) || ((out_buffer_size >= 3) && (memcmp(out_buffer_data,"+++",3) == 0))) {
		if ((out_buffer_size >= 3) && (memcmp(out_buffer_data,"+++",3) == 0)) {
			Terminate(true,false);
			out_buffer_size = 0;
			return;
		}
		serial.Write((void *)(uint32(out_buffer_data) + uint32(offset)),out_buffer_size - offset); //---Echo back
		Modem();
		return;
	}
	
	if (((uint8 *)(out_buffer_data))[out_buffer_size - 1] == 0x7e) {
		off_t offset;
		ssize_t sub_length;
		uint8 *buffer = (uint8 *)(out_buffer_data);
		bool is_first_tilde = false;
		uint16 ppp_protocol;
		for (int32 i = 0; i < out_buffer_size;i++) {
			if (buffer[i] == 0x7e) {
				is_first_tilde = !is_first_tilde;
				if (is_first_tilde) {
					offset = i;
					continue;
				} else {
					sub_length = (i-offset)+1;
					PPPPacket *to_send = AllocPacket();
					to_send->SetToAsyncFrame((void *)(uint32(buffer) + offset),sub_length,mtu);
					to_send->GetData(&ppp_protocol,2);
					FROM_NET_ENDIAN(ppp_protocol);
					SendPacketToNet(to_send);
					if ((ppp_protocol == 0x8021) && (drop_second_ipcp))
						break;
				}
			}
		}
		#if ASSERTIONS
			assert ((offset + sub_length >= out_buffer_size) || ((ppp_protocol == 0x8021) && (drop_second_ipcp)));
		#endif
		out_buffer_size = 0;
	}
}

ppp_transport::~ppp_transport(void) {
	char fiddle[64];
	sprintf(fiddle,"/dev/ports/%s",port_name);
	#if DEBUG_ON
		fprintf(f,"We've been deleted! Phooey.");
		fflush(f);
		fclose(f);
	#endif
	if (linkUp)
		Terminate(true,true);
	kill_thread(watcher);
	unlink(fiddle);
	if (out_buffer_data != NULL)
		free(out_buffer_data);
}
