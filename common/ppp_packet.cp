#include <string.h>
#include <debugger.h>
#include <stdio.h>
#include <SupportDefs.h>
#include "ppp_packet.h"
#include "fcs.h"
#include "mtu.h"

#define verbose_assert(test,var)\
	if (test) {\
		char message[255];\
		sprintf(message,"Test %s failed on line %d of file %s. Value of %s is %d.",#test,__LINE__,__FILE__,#var,var);\
		debugger(message);\
	}

void PPPPacket::SetToAsyncFrame(const void *frame,size_t length,int32 clamp) {
	static uint8 edit_buffer[4096];
	int32 g = 0;
	
	//verbose_assert(length < 5/*minimum PPP packet len*/,length);
	
	for (int32 i = 4; i < length; i++) {         //----------Unescape----------
		if (((uint8 *)(frame))[i] == 0x7e) {
			if (g < 2)
				return;
			g -= 2;
			break;
		}
		if (((uint8 *)(frame))[i] == 0x7d) {
			i++;
			edit_buffer[g] = ((uint8 *)(frame))[i] ^ 0x20;
		} else
			edit_buffer[g] = ((uint8 *)(frame))[i];
		g++;
	}
	//verbose_assert((g < 2) || (g > 4096),g);
	
	if (clamp > 0)
		clampMSS(edit_buffer,g,clamp);
		
	SetData(edit_buffer,g,0);
}

size_t PPPPacket::GetAsyncFrame(const void *frame,size_t length,int32 clamp) {
	uint8 *data = (uint8 *)(frame);
	static uint8 edit_buffer[4096];
	size_t size = GetData((const void *)(uint32(data) + 3),4096);
	
	if (clamp > 0)
		clampMSS((uint8 *)(uint32(data) + 3),size,clamp);
	
		data[0] = 0x7e;			//-----MagicCookies
		data[1] = 0xff;			//-----|
		data[2] = 0x03;			//-----|
								//-----Data
		data[size+3] = 0x7e;	//-----Frame termination
	size += 4;
	memcpy(edit_buffer,data,size);
	uint16 fcs = pppfcs16(PPPINITFCS16,&(edit_buffer[1]),size-2);
	fcs ^= 0xffff;
  	edit_buffer[size-1] = fcs & 0x00ff;
   	edit_buffer[size] = (fcs >> 8) & 0x00ff;
	size += 2;
	data[size-1] = 0x7e;	//-----Frame termination
	int32 g = 2;
	for (int32 i = 2; i < (size-1); i++) {
		if ((edit_buffer[i] == 0x7e) || (edit_buffer[i] == 0x7d) || (edit_buffer[i] < 0x20)) {
			data[g] = 0x7d;
			g++;
			data[g] = edit_buffer[i] ^ 0x20;
		} else {
			data[g] = edit_buffer[i];
		}
		g++;
	}
	size = g+1;
	data[g] = 0x7e;	//-----Frame termination
	return size;
}