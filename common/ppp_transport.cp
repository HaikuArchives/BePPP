#include <add-ons/net_server/NetProtocol.h>
#include <support/List.h>
#include <support/String.h>
#include <support/StopWatch.h>
#include <string.h>
#include <unistd.h>
#include <net_settings.h>
#include <TypeConstants.h>
#include <ByteOrder.h>
#include "ppp_transport.h"

#define NET_ENDIAN(a)  swap_data(B_UINT16_TYPE,&a,sizeof(a),B_SWAP_HOST_TO_BENDIAN)
#define FROM_NET_ENDIAN(a)  swap_data(B_UINT16_TYPE,&a,sizeof(a),B_SWAP_BENDIAN_TO_HOST)

#define WATCH_PORT_PRIORITY 10


/*#define TIMED 1
#define DEBUG_ON 1*/

#if DEBUG_ON
#include "debug.h"
#endif

#if TIMED
int total_recv_packets = 0;
int total_sent_packets = 0;
FILE *t;
#endif
status_t alloc_pty (char *buffer, int *fd);

status_t alloc_pty (char *buffer, int *fd) {
	char *letters = "pqrs";
	char *numbers = "0123456789abcdef";
	char *i, *j;
	int handle;
	
	for (i = letters; i[0]; i++) {
		for (j = numbers; j[0]; j++) {
			sprintf(buffer,"/dev/pt/%c%c",*i,*j);
			handle = open(buffer, O_RDWR, 0000);
			if (handle > 0) {
				*fd = handle;
				return B_OK;
			}
		}
	}
	
	return B_ERROR;
}

int32 ppp_transport::watch_port(void *us) {
	ppp_transport *obj = (ppp_transport *)(us);
	
	ssize_t bufferSize;
	uint8 buffer[B_PAGE_SIZE];
	char port_path[64];
	sprintf(port_path,"/dev/ports/%s",obj->port_name);
		
	while (true) {
		bufferSize = read(obj->pty_fd,buffer,B_PAGE_SIZE);
		if (bufferSize <= 0) {			//--------We got closed. Phooey.
			char pty_name[15];
			alloc_pty(pty_name,&(obj->pty_fd));
			pty_name[5] = 't';
			unlink(port_path);
			symlink(pty_name,port_path);
			continue;
		}
		obj->WriteBuffer(buffer,bufferSize,out_buffer);
		obj->MessageReceived(buffer,bufferSize);
	}
	return 0;
}

int32 ppp_transport::Modem(void *pointer) {							//---------Our lovely fake modem-------
	ppp_transport *obj = (ppp_transport *)(pointer);
	if (obj->out_buffer_size == 0)
		return 0;
	char last_char;
	obj->ReadBuffer(&last_char,1,out_buffer,obj->out_buffer_size - 1,false);
	if ((last_char != '\r') && (last_char != '\n'))
		return 0;
	char *com = new char[obj->out_buffer_size+1];
	size_t term = obj->ReadBuffer(com,obj->out_buffer_size,out_buffer);
	com[term] = 0;
	BString command(com);
	delete [] com;
	char *response;
	if (command.IFindFirst("ATDT") != B_ERROR) {
		command.CopyInto(obj->number_dialed,4,command.FindFirst('\r') - 4);
		obj->Open();
		if (!(obj->linkUp))
			response = /*(last_char == '\n') ? */"NO CARRIER\n\r"/* : "NO CARRIER\r\n"*/;
		else
			response = /*(last_char == '\n') ? */"CONNECT 256000\n\r"/* : "CONNECT 256000\r\n"*/;
	} else {
		response = /*(last_char == '\n') ? */"OK\n\r"/* : "OK\r\n"*/;
	}
	#if DEBUG_ON
		fprintf(f,"\n\nResponse: %s",response);
		fflush(f);
	#endif
	obj->WriteBuffer(response,strlen(response),in_buffer);
	obj->modem = 0;
	return 0;
}

void ppp_transport::SlideBuffer(off_t distance,off_t offset,buffer_type buffer) {
	distance = (distance > out_buffer_size) ? out_buffer_size : distance;
	
	if (out_buffer_size > distance) {
		memcpy((void *)((uint32)(out_buffer_data) + (uint32)(offset)),(void *)((uint32)(out_buffer_data) + (uint32)(offset) + (uint32)(distance)),out_buffer_size - distance);
		out_buffer_size -= distance;
	} else
		out_buffer_size = 0;
}
	

size_t ppp_transport::ReadBuffer(void *data,size_t length,buffer_type buffer,off_t offset,bool slide) {  //-------Implement sliding buffer
	acquire_sem(read_write);
	
	length = (length > out_buffer_size) ? out_buffer_size : length;
	memcpy(data,(void *)((uint32)(out_buffer_data) + (uint32)(offset)),length);
	
	if (slide)
		SlideBuffer(length,offset,buffer);
	
	release_sem(read_write);
	return length;
}

size_t ppp_transport::WriteBuffer(void *data,size_t length,buffer_type buffer) {  //-------Implement sliding buffer
	acquire_sem(read_write);
	
	if (buffer == out_buffer) {
		if (length != 0)
			memcpy((void *)((uint32)(out_buffer_data) + (uint32)out_buffer_size),data,length);
		
		out_buffer_size += length;
	} else {
		write(pty_fd,data,length);
	}
	
	release_sem(read_write);
	return length;
}

ppp_transport::ppp_transport (const char *port) {
	out_buffer_data = new uint8[B_PAGE_SIZE];
	drop_second_ipcp = false;
	char text_to_fiddle[64];
	out_buffer_size = 0;
	modem = 0;
	mtu = -1;
	linkUp = false;
	port_name = port;
	
	char pty_name[15];
	sprintf(text_to_fiddle,"%s_read_write",port);
	read_write = create_sem(1,text_to_fiddle);
	sprintf(text_to_fiddle,"%s_watcher",port);
	watcher = spawn_thread(&watch_port,text_to_fiddle,WATCH_PORT_PRIORITY,this);
	alloc_pty(pty_name,&pty_fd);
	pty_name[5] = 't';
	sprintf(text_to_fiddle,"/dev/ports/%s",port);
	unlink(text_to_fiddle);
	symlink(pty_name,text_to_fiddle);
	
	/* Read setting for drop_second_ipcp */
	find_net_setting(NULL,port,"drop_second_ipcp",text_to_fiddle,64);
	drop_second_ipcp = (BString(text_to_fiddle).IFindFirst("true") != -1);
	/* Read setting for mtu */
	text_to_fiddle[0] = 0; //---find_net_setting won't change it if there's nothing
	find_net_setting(NULL,port,"mtu",text_to_fiddle,64);
	if (text_to_fiddle[0] != 0)
		sscanf(text_to_fiddle,"%d",&mtu);
	
	resume_thread(watcher);
	#if DEBUG_ON
		just_started = true;
		f = fopen(DUMPTO, "w+");
		fprintf(f,"Allocated pty: %s\n",pty_name);
		fflush(f);
	#endif
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
	if (kill_serial)
		close(pty_fd);
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
	WriteBuffer(data,size,in_buffer);
}

void ppp_transport::MessageReceived(const void *data,size_t length) {
	#if TIMED
		fprintf(t,"Entering MessageReceived...");
		fflush(t);
		BStopWatch watch("MessageReceived_watch");
	#endif
	if ((linkUp == false) || ((out_buffer_size >= 3) && (memcmp(out_buffer_data,"+++",3) == 0))) {
		if ((out_buffer_size >= 3) && (memcmp(out_buffer_data,"+++",3) == 0)) {
			acquire_sem(read_write);
			SlideBuffer(3);
			release_sem(read_write);
			#if DEBUG_ON
				fprintf(f,"Terminating..\n");
				fflush(f);
			#endif
			Terminate(true,true);
			return;
		}
		WriteBuffer((void *)data,length,in_buffer); //---Echo back
		char fiddle[64];
		sprintf(fiddle,"%s_modem_emulation",port_name);
		modem = spawn_thread(&Modem,fiddle,B_NORMAL_PRIORITY,this);
		resume_thread(modem);
		return;
	}
	
	uint8 end;
	ReadBuffer(&end,1,out_buffer,out_buffer_size-1,false);
	if (end == 0x7e) {
		acquire_sem(read_write); //-----------We're playing with the buffer here...
		off_t offset;
		ssize_t sub_length;
		uint8 *buffer = (uint8 *)(out_buffer_data);
		bool is_first_tilde = false;
		for (int32 i = 0; i < out_buffer_size;i++) {
			if (buffer[i] == 0x7e) {
				is_first_tilde = !is_first_tilde;
				if (is_first_tilde) {
					#if TIMED
						fprintf(t,"Found frame start!\n");
						fflush(t);
					#endif
					offset = i;
					continue;
				} else {
					sub_length = (i-offset)+1;
					PPPPacket *to_send = AllocPacket();
					to_send->SetToAsyncFrame((void *)(uint32(buffer) + offset),sub_length,mtu);
					uint16 ppp_protocol;
					to_send->GetData(&ppp_protocol,2);
					FROM_NET_ENDIAN(ppp_protocol);
					SendPacketToNet(to_send);
					SlideBuffer(sub_length,offset);
					i -= sub_length;
					if ((ppp_protocol == 0x8021) && (drop_second_ipcp)) {
						SlideBuffer(out_buffer_size,0);
						break;
					}
					#if DEBUG_ON
						fprintf(f,"Contents of out_buffer: \n");
						fwrite(out_buffer_data,1,out_buffer_size,f);
						#if TIMED
							fprintf(f,"\nSending packet number: %d\n",total_sent_packets);
						#endif
						fprintf(f,"\n");
						fflush(f);
					#endif
					#if TIMED
						++total_sent_packets;
						fprintf(t,"Sent a packet! PPP protocol: %04x Length: %d\n",ppp_protocol,sub_length);
						fflush(t);
					#endif
				}
			}
		}
		release_sem(read_write);
	}
	#if TIMED
		fprintf(t,"Elapsed time = %f secs\nTotal session packets: s:%d,r:%d\nBuffer length on exit: %d\n",watch.ElapsedTime() / float(1e6),total_sent_packets,total_recv_packets,out_buffer_size);
		fflush(t);
	#endif
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
	close(pty_fd);
	unlink(fiddle);
	delete [] out_buffer_data;
	delete_sem(read_write);
}
