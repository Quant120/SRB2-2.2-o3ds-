#include <SDL.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../doomdef.h"
#include "../i_sound.h"
#include "../s_sound.h"
#include "../w_wad.h"
#include "../z_zone.h"
#include "../byteptr.h"

#define SAMPLERATE 22050
#define BUFFERSIZE 1024
#define MUSICFILE "sdmc:/3ds/srb2/.srb2music.ogg"

UINT8 sound_started=false;

static Mix_Music *music;
static UINT8 music_volume=16;
static UINT8 sfx_volume=16;
static UINT8 internal_volume=100;
static double loop_point;
static boolean songpaused;
static boolean is_looping;

void I_StartupSound(void)
{
	if(sound_started) return;

	if(SDL_Init(SDL_INIT_AUDIO)<0)
	{
		printf("SDL audio: %s\n",SDL_GetError());
		return;
	}

	if(Mix_OpenAudio(SAMPLERATE,AUDIO_U8,1,BUFFERSIZE)<0)
	{
		printf("SDL mixer: %s\n",Mix_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return;
	}

	Mix_AllocateChannels(16);
	sound_started=true;
}

void I_ShutdownSound(void)
{
	I_UnloadSong();
	if(!sound_started) return;
	sound_started=false;
	Mix_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void I_UpdateSound(void){}

static Mix_Chunk *ds2chunk(void *stream)
{
	UINT16 ver,freq;
	UINT32 samples,newsamples,i;
	UINT8 *sound,*s,*d;

	ver=READUINT16(stream);
	if(ver!=3) return NULL;

	freq=READUINT16(stream);
	samples=READUINT32(stream);
	if(!freq||!samples) return NULL;

	newsamples=(UINT32)(((UINT64)samples*SAMPLERATE+freq-1)/freq);
	if(!newsamples) return NULL;

	sound=Z_Malloc(newsamples,PU_SOUND,NULL);
	s=(UINT8 *)stream;
	d=sound;

	for(i=0;i<newsamples;i++)
	{
		UINT32 n=(UINT32)(((UINT64)i*freq)/SAMPLERATE);
		if(n>=samples) n=samples-1;
		*d++=s[n];
	}

	return Mix_QuickLoad_RAW(sound,(Uint32)(d-sound));
}

void *I_GetSfx(sfxinfo_t *sfx)
{
	void *lump;
	Mix_Chunk *chunk;
	SDL_RWops *rw;

	if(!sound_started) return NULL;

	if(sfx->lumpnum==LUMPERROR)
		sfx->lumpnum=S_GetSfxLumpNum(sfx);
	sfx->length=W_LumpLength(sfx->lumpnum);
	lump=W_CacheLumpNum(sfx->lumpnum,PU_SOUND);

	chunk=ds2chunk(lump);
	if(chunk)
	{
		Z_Free(lump);
		return chunk;
	}

	rw=SDL_RWFromMem(lump,sfx->length);
	if(!rw)
	{
		Z_Free(lump);
		return NULL;
	}

	chunk=Mix_LoadWAV_RW(rw,1);
	Z_Free(lump);
	return chunk;
}

void I_FreeSfx(sfxinfo_t *sfx)
{
	if(sfx->data)
	{
		Mix_Chunk *chunk=(Mix_Chunk *)sfx->data;
		UINT8 *abufdata=NULL;

		if(!chunk->allocated)
			abufdata=chunk->abuf;

		Mix_FreeChunk(chunk);
		if(abufdata) Z_Free(abufdata);
	}

	sfx->data=NULL;
	sfx->lumpnum=LUMPERROR;
}

INT32 I_StartSound(sfxenum_t id,UINT8 vol,UINT8 sep,UINT8 pitch,UINT8 priority,INT32 channel)
{
	UINT8 volume;
	INT32 handle;

	if(!sound_started||!S_sfx[id].data) return -1;

	volume=(((UINT16)vol+1)*(UINT16)sfx_volume)/62;
	handle=Mix_PlayChannel(channel,S_sfx[id].data,0);
	if(handle>=0)
		Mix_Volume(handle,volume);

	(void)sep;
	(void)pitch;
	(void)priority;
	return handle;
}

void I_StopSound(INT32 handle)
{
	if(sound_started&&handle>=0) Mix_HaltChannel(handle);
}

boolean I_SoundIsPlaying(INT32 handle)
{
	return sound_started&&handle>=0&&Mix_Playing(handle);
}

void I_UpdateSoundParams(INT32 handle,UINT8 vol,UINT8 sep,UINT8 pitch)
{
	UINT8 volume;

	if(!sound_started||handle<0) return;

	volume=(((UINT16)vol+1)*(UINT16)sfx_volume)/62;
	Mix_Volume(handle,volume);
	(void)sep;
	(void)pitch;
}

void I_SetSfxVolume(UINT8 volume){sfx_volume=volume;}

void I_InitMusic(void){}
void I_ShutdownMusic(void){I_UnloadSong();}

static UINT32 get_real_volume(UINT8 volume)
{
	UINT32 v=((UINT32)volume*128)/31;
	v=(v*(UINT32)internal_volume)/100;
	if(v>128) v=128;
	return v;
}

static unsigned long readnum(const char *p,const char *end)
{
	unsigned long n=0;
	while(p<end&&*p>='0'&&*p<='9')
	{
		n=n*10+(unsigned long)(*p-'0');
		p++;
	}
	return n;
}

static void findloop(const char *data,size_t len)
{
	const char *p=data;
	const char *end=data+len;
	const char *point="LOOPPOINT=";
	const char *ms="LOOPMS=";
	size_t pointlen=strlen(point),mslen=strlen(ms);

	loop_point=0.0;
	while(p<end)
	{
		if((size_t)(end-p)>=pointlen&&!memcmp(p,point,pointlen))
		{
			loop_point=(double)readnum(p+pointlen,end)/44100.0;
			return;
		}
		if((size_t)(end-p)>=mslen&&!memcmp(p,ms,mslen))
		{
			loop_point=(double)readnum(p+mslen,end)/1000.0;
			return;
		}
		p++;
	}
}

static void music_loop(void)
{
	if(!music||!is_looping) return;
	if(Mix_PlayMusic(music,0)<0) return;
	if(loop_point>0.0) Mix_SetMusicPosition(loop_point);
}

musictype_t I_SongType(void)
{
	if(!music) return MU_NONE;
	return MU_OGG;
}

boolean I_SongPlaying(void){return music!=NULL;}
boolean I_SongPaused(void){return songpaused;}
boolean I_SetSongSpeed(float speed){(void)speed;return false;}
UINT32 I_GetSongLength(void){return 0;}

boolean I_SetSongLoopPoint(UINT32 p)
{
	loop_point=(double)p/1000.0;
	if(music&&is_looping)
		Mix_HookMusicFinished(loop_point>0.0?music_loop:NULL);
	return true;
}

UINT32 I_GetSongLoopPoint(void){return (UINT32)(loop_point*1000.0);}
boolean I_SetSongPosition(UINT32 p){return music&&Mix_SetMusicPosition((double)p/1000.0)==0;}
UINT32 I_GetSongPosition(void){return 0;}

boolean I_LoadSong(char *data,size_t len)
{
	FILE *f;

	if(!data||len<4||memcmp(data,"OggS",4)) return false;
	if(!sound_started) I_StartupSound();
	if(!sound_started) return false;

	I_UnloadSong();
	findloop(data,len);

	f=fopen(MUSICFILE,"wb");
	if(!f) return false;
	if(fwrite(data,1,len,f)!=len)
	{
		fclose(f);
		remove(MUSICFILE);
		return false;
	}
	fclose(f);

	music=Mix_LoadMUS(MUSICFILE);
	if(!music)
	{
		printf("Mix_LoadMUS: %s\n",Mix_GetError());
		remove(MUSICFILE);
		return false;
	}

	return true;
}

void I_UnloadSong(void)
{
	Mix_HookMusicFinished(NULL);
	if(music)
	{
		Mix_HaltMusic();
		Mix_FreeMusic(music);
		music=NULL;
	}
	remove(MUSICFILE);
	songpaused=false;
	is_looping=false;
}

boolean I_PlaySong(boolean looping)
{
	int loops;

	if(!music) return false;

	Mix_VolumeMusic(get_real_volume(music_volume));
	is_looping=looping;
	Mix_HookMusicFinished(NULL);
	loops=looping&&loop_point==0.0?-1:0;
	if(Mix_PlayMusic(music,loops)<0)
	{
		printf("Mix_PlayMusic: %s\n",Mix_GetError());
		return false;
	}
	if(looping&&loop_point>0.0) Mix_HookMusicFinished(music_loop);
	songpaused=false;
	return true;
}

void I_StopSong(void)
{
	if(!music) return;
	Mix_HookMusicFinished(NULL);
	Mix_HaltMusic();
	songpaused=false;
}

void I_PauseSong(void)
{
	if(!music) return;
	Mix_PauseMusic();
	songpaused=true;
}

void I_ResumeSong(void)
{
	if(!music) return;
	Mix_ResumeMusic();
	songpaused=false;
}

void I_SetMusicVolume(UINT8 volume)
{
	music_volume=volume;
	if(music) Mix_VolumeMusic(get_real_volume(music_volume));
}

boolean I_SetSongTrack(INT32 track){(void)track;return false;}

void I_SetInternalMusicVolume(UINT8 volume)
{
	internal_volume=volume;
	if(music) Mix_VolumeMusic(get_real_volume(music_volume));
}

void I_StopFadingSong(void){}

boolean I_FadeSongFromVolume(UINT8 target,UINT8 source,UINT32 ms,void (*callback)(void))
{
	(void)source;
	(void)ms;
	I_SetInternalMusicVolume(target);
	if(callback) callback();
	return true;
}

boolean I_FadeSong(UINT8 target,UINT32 ms,void (*callback)(void))
{
	(void)ms;
	I_SetInternalMusicVolume(target);
	if(callback) callback();
	return true;
}

boolean I_FadeOutStopSong(UINT32 ms)
{
	if(!music) return false;
	return Mix_FadeOutMusic(ms)!=0;
}

boolean I_FadeInPlaySong(UINT32 ms,boolean looping)
{
	int loops;

	if(!music) return false;

	Mix_VolumeMusic(get_real_volume(music_volume));
	is_looping=looping;
	Mix_HookMusicFinished(NULL);
	loops=looping&&loop_point==0.0?-1:0;
	if(Mix_FadeInMusic(music,loops,ms)<0) return false;
	if(looping&&loop_point>0.0) Mix_HookMusicFinished(music_loop);
	songpaused=false;
	return true;
}
