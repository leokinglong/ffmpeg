#include <stdio.h>
#include <stdint.h>

#ifndef _AGC_API_HEADER
#define _AGC_API_HEADER
#ifdef __cplusplus
extern "C" {
#endif


	void * init_AGC_API(uint32_t sampleRate,int chan_num,int targetDBFS);
	int16_t * runAGC_API(void * obj,int16_t* in_buffer,int in_len, int* out_len);
	void terminate_AGC_API(void * obj);
#ifdef __cplusplus
}
#endif
#endif