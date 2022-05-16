#include <stdio.h>
#include <stdint.h>

#ifndef _NS_API_HEADER
#define _NS_API_HEADER
#ifdef __cplusplus
extern "C" {
#endif

enum nsLevel {
    kLow,
    kModerate,
    kHigh,
    kVeryHigh
};

	void * init_NS_API(uint32_t channels,uint32_t sampleRate, enum nsLevel level);
	int16_t * runNS_API(void * obj,int16_t* in_buffer,int in_len, int* out_len);
	void terminate_NS_API(void * obj);
#ifdef __cplusplus
}
#endif
#endif