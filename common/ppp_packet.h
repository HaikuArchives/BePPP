#ifndef _PPP_PACKET_H
#define _PPP_PACKET_H

class PPPPacket {
	public: 
		virtual void SetData(const void *data,size_t length,uint16 offset = 0) = 0; //---Less than 1522 bytes
		virtual size_t GetData(const void *buffer,size_t length,uint16 offset = 0) = 0;
		
		void SetToAsyncFrame(const void *frame,size_t length,int32 clamp = -1);
		size_t GetAsyncFrame(const void *frame,size_t length,int32 clamp = -1);
};
#endif