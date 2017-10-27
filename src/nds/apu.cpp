// GB Enhanced Copyright Daniel Baxter 2015
// Licensed under the GPLv2
// See LICENSE.txt for full license text

// File : apu.cpp
// Date : September 10, 2017
// Description : NDS APU emulation
//
// Sets up SDL audio for mixing
// Generates and mixes samples for the NDS's 16 sound channels  

#include <cmath>

#include "apu.h"

/****** APU Constructor ******/
NTR_APU::NTR_APU()
{
	reset();
}

/****** APU Destructor ******/
NTR_APU::~NTR_APU()
{
	SDL_CloseAudio();
	std::cout<<"APU::Shutdown\n";
}

/****** Reset APU ******/
void NTR_APU::reset()
{
	SDL_CloseAudio();

	apu_stat.sound_on = false;
	apu_stat.stereo = false;

	apu_stat.sample_rate = config::sample_rate;
	apu_stat.main_volume = 4;

	apu_stat.channel_master_volume = config::volume;

	//Reset Channel 1-16 data
	for(int x = 0; x < 16; x++)
	{
		apu_stat.channel[x].output_frequency = 0.0;
		apu_stat.channel[x].data_src = 0;
		apu_stat.channel[x].data_pos = 0;
		apu_stat.channel[x].loop_start = 0;
		apu_stat.channel[x].length = 0;
		apu_stat.channel[x].samples = 0;
		apu_stat.channel[x].cnt = 0;
		apu_stat.channel[x].volume = 0;

		apu_stat.channel[x].playing = false;
		apu_stat.channel[x].enable = false;

		apu_stat.channel[x].adpcm_header = 0;
		apu_stat.channel[x].adpcm_index = 0;
		apu_stat.channel[x].adpcm_val = 0;
	}

	//Setup IMA-ADPCM table
	for(u32 x = 0x776D2, a = 0; a < 128; a++)
	{
		if(a == 3) { apu_stat.adpcm_table[a] = 0xA; }
		else if(a == 4) { apu_stat.adpcm_table[a] = 0xB; }
		else if(a == 88) { apu_stat.adpcm_table[a] = 0x7FFF; }
		else if(a >= 89) { apu_stat.adpcm_table[a] = 0; }
		else { apu_stat.adpcm_table[a] = (x >> 16); }
		
		x = x + (x / 10);
	}
}

/****** Initialize APU with SDL ******/
bool NTR_APU::init()
{
	//Initialize audio subsystem
	if(SDL_InitSubSystem(SDL_INIT_AUDIO) == -1)
	{
		std::cout<<"APU::Error - Could not initialize SDL audio\n";
		return false;
	}

	//Setup the desired audio specifications
    	desired_spec.freq = apu_stat.sample_rate;
	desired_spec.format = AUDIO_S16SYS;
    	desired_spec.channels = 1;
    	desired_spec.samples = 4096;
    	desired_spec.callback = ntr_audio_callback;
    	desired_spec.userdata = this;

    	//Open SDL audio for desired specifications
	if(SDL_OpenAudio(&desired_spec, &obtained_spec) < 0) 
	{ 
		std::cout<<"APU::Failed to open audio\n";
		return false; 
	}
	else if(desired_spec.format != obtained_spec.format) 
	{ 
		std::cout<<"APU::Could not obtain desired audio format\n";
		return false;
	}

	else
	{
		apu_stat.channel_master_volume = config::volume;

		SDL_PauseAudio(0);
		std::cout<<"APU::Initialized\n";
		return true;
	}
}

/****** Generates samples for NDS sound channels ******/
void NTR_APU::generate_channel_samples(s32* stream, int length, u8 id)
{
	double sample_ratio = (apu_stat.channel[id].output_frequency / apu_stat.sample_rate);
	u32 sample_pos = apu_stat.channel[id].data_pos;
	u8 format = ((apu_stat.channel[id].cnt >> 29) & 0x3);
	u8 loop_mode = ((apu_stat.channel[id].cnt >> 27) & 0x3);

	//Calculate volume
	float vol = (apu_stat.channel[id].volume != 0) ? (apu_stat.channel[id].volume / 127.0) : 0;
	vol *= (apu_stat.main_volume / 127.0);
	vol *= (config::volume / 128.0);

	s8 nds_sample_8 = 0;
	s16 nds_sample_16 = 0;
	u32 samples_played = 0;

	for(u32 x = 0; x < length; x++)
	{
		//Channel 0 should set default data
		if(id == 0) { stream[x] = 0; }

		//Pull data from NDS memory
		if((apu_stat.channel[id].samples) && (apu_stat.channel[id].playing))
		{
			//PCM8
			if(format == 0)
			{
				u32 data_addr = (sample_pos + (sample_ratio * x));
				nds_sample_8 = mem->memory_map[sample_pos + (sample_ratio * x)];

				//Scale S8 audio to S16
				stream[x] += (nds_sample_8 * 256);

				//Adjust volume level
				stream[x] *= vol;

				if(data_addr >= (apu_stat.channel[id].data_src + apu_stat.channel[id].samples))
				{
					//Loop sound
					if(loop_mode == 1)
					{
						apu_stat.channel[id].data_src += (apu_stat.channel[id].loop_start * 4);
						apu_stat.channel[id].data_pos = apu_stat.channel[id].data_src;
						apu_stat.channel[id].samples = (apu_stat.channel[id].length * 4);
					}
					
					//Stop sound
					else if(loop_mode == 2)
					{
						apu_stat.channel[id].playing = false;
						apu_stat.channel[id].cnt &= ~0x80000000;
					}
				}	
			}

			//PCM16
			else if(format == 1)
			{
				u32 data_addr = (sample_pos + (sample_ratio * x));
				data_addr &= ~0x1;
				nds_sample_16 = mem->read_u16_fast(data_addr);

				stream[x] += nds_sample_16;

				//Adjust volume level
				stream[x] *= vol;

				if(data_addr >= (apu_stat.channel[id].data_src + apu_stat.channel[id].samples))
				{
					//Loop sound
					if(loop_mode == 1)
					{
						apu_stat.channel[id].data_src += (apu_stat.channel[id].loop_start * 2);
						apu_stat.channel[id].data_pos = apu_stat.channel[id].data_src;
						apu_stat.channel[id].samples = (apu_stat.channel[id].length * 2);
					}
					
					//Stop sound
					else if(loop_mode == 2)
					{	
						apu_stat.channel[id].playing = false;
						apu_stat.channel[id].cnt &= ~0x80000000;
					}
				}	
			}

			else { stream[x] += (-32768 * vol); }

			samples_played++;
		}

		//Generate silence if sound has run out of samples or is not playing
		else { stream[x] += (-32768 * vol); }
	}

	//Advance data pointer to sound samples
	switch(format)
	{
		case 0x0:
			apu_stat.channel[id].data_pos += (sample_ratio * samples_played);
			break;

		case 0x1:
			apu_stat.channel[id].data_pos += (sample_ratio * samples_played);
			apu_stat.channel[id].data_pos &= ~0x1;
			break;
	} 
}

/****** SDL Audio Callback ******/ 
void ntr_audio_callback(void* _apu, u8 *_stream, int _length)
{
	s16* stream = (s16*) _stream;
	int length = _length/2;
	s32 channel_stream[length];

	NTR_APU* apu_link = (NTR_APU*) _apu;

	//Generate samples
	for(u32 x = 0; x < 16; x++) { apu_link->generate_channel_samples(channel_stream, length, x); }

	//Custom software mixing
	for(u32 x = 0; x < length; x++)
	{
		channel_stream[x] /= 16;
		stream[x] = channel_stream[x];
	}
}
