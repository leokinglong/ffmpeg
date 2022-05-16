#ifndef SOXWRAPPER_H
#define SOXWRAPPER_H /**< Client API: This macro is defined if sox.h has been included. */


#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct audioProfile_t
{
	int sampleRate; //define the sample rate, unit as Hz
	int chanNum; // define the number of the channels
	int bitsDepth;  // Define the number of the bits per sample used
	int frameLen;
	int bigEndian; 
	int wavlen;
	int msg_snd_id;
	int msg_rcv_id;
	int opt_rcv_id;
	int eq_pattern;
	int reverb_pattern;
	int eq_profile[10];
	int total_gain;
	int revb_percent;
	int revb_hfdamping;
	int revb_roomscale;
	int revb_depth;
	int revb_predelay;	
}AudioProfile;
//default reverb pattern
#define MINIMUM_REVERB_EFFECT 1

//Meeting room pattern
#define MEETING_ROOM_EFFECT 2

//Concert Hall pattern
#define CONCERT_HALL_EFFECT 3
//3d Surround pattern
#define IMMERSIVE_SURROUND_EFFECT 4

 //This is invoked to dispatch the parameters such as sample rate, bit depth to sox
void* init_AW_API(AudioProfile * profile);

//send data from java to native layer
void sendMsg(int * buf, int len,int msg_snd_id);

void terminate_process(void *obj,int flag);
void trigger_process(void *obj,AudioProfile * profile);
	void runAW_API(void * obj,int16_t* in_buffer,int in_len, int* out_buffer);
	void terminate_AW_API(void * obj);
	int setEqOption(void * obj,int bandIndex, int gainValue);
	int switch_channel(void *obj,int flag,int16_t* in_buffer,int in_len);
	int switch_channel_32(void *obj,int flag,int* in_buffer,int in_len);
#ifdef __cplusplus
}
#endif
#endif