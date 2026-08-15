#include <3ds.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../doomdef.h"
#include "../doomtype.h"
#include "../d_main.h"
#include "../i_system.h"
#include "../i_time.h"

FILE *logstream = NULL;
UINT8 graphics_started = 0;
UINT8 keyboard_started = 0;

static quitfuncptr quits[MAX_QUIT_FUNCS];
static int nquits;
static int started;
static int shuttingdown;
static ticcmd_t nothing[2];

static void cleartop(void)
{
	int n;
	for (n=0;n<2;n++)
	{
		u16 *fb=(u16 *)gfxGetFramebuffer(GFX_TOP,GFX_LEFT,NULL,NULL);
		if (fb) memset(fb,0,400*240*2);
		gfxFlushBuffers();
		gfxScreenSwapBuffers(GFX_TOP,false);
		gspWaitForVBlank();
	}
}

size_t I_GetFreeMem(size_t *total)
{
	*total=osGetMemRegionSize(MEMREGION_APPLICATION);
	return osGetMemRegionFree(MEMREGION_APPLICATION);
}

void I_Sleep(UINT32 ms){ svcSleepThread((s64)ms*1000000LL); }
void I_SleepDuration(precise_t d)
{
	s64 ns=(s64)((d*1000000000ULL)/SYSCLOCK_ARM11);
	if (ns>0) svcSleepThread(ns);
}
precise_t I_GetPreciseTime(void){ return svcGetSystemTick(); }
UINT64 I_GetPrecisePrecision(void){ return SYSCLOCK_ARM11; }

void I_GetEvent(void)
{
	if (!aptMainLoop()) I_Quit();
}

void I_OsPolling(void){}
ticcmd_t *I_BaseTiccmd(void){ memset(nothing,0,sizeof(nothing[0]));return &nothing[0]; }
ticcmd_t *I_BaseTiccmd2(void){ memset(&nothing[1],0,sizeof(nothing[1]));return &nothing[1]; }

void I_Quit(void)
{
	I_ShutdownSystem();
	exit(0);
}

void I_Error(const char *error,...)
{
	va_list a;
	printf("\nit died:\n");
	va_start(a,error); vprintf(error,a); va_end(a);
	I_ShutdownSystem();
	exit(-1);
}

void I_Tactile(FFType Type,const JoyFF_t *Effect){(void)Type;(void)Effect;}
void I_Tactile2(FFType Type,const JoyFF_t *Effect){(void)Type;(void)Effect;}
void I_JoyScale(void){}
void I_JoyScale2(void){}
void I_InitJoystick(void){}
void I_InitJoystick2(void){}
INT32 I_NumJoys(void){return 0;}
const char *I_GetJoyName(INT32 n){(void)n;return NULL;}

#ifndef NOMUMBLE
void I_UpdateMumble(const mobj_t *m,const listener_t l){(void)m;(void)l;}
#endif

void I_OutputMsg(const char *s,...)
{
	va_list a;
	va_start(a,s);vprintf(s,a);va_end(a);
}

void I_StartupMouse(void){}
void I_StartupMouse2(void){}
INT32 I_GetKey(void){return 0;}
void I_StartupTimer(void){}

void I_AddExitFunc(void (*f)())
{
	if(nquits<MAX_QUIT_FUNCS) quits[nquits++]=f;
}

void I_RemoveExitFunc(void (*f)())
{
	int i;
	for(i=0;i<nquits;i++) if(quits[i]==f)
	{
		memmove(quits+i,quits+i+1,(nquits-i-1)*sizeof(quits[0]));
		nquits--;
		break;
	}
}

INT32 I_StartupSystem(void)
{
	if(started) return 0;
	gfxInit(GSP_RGB565_OES,GSP_BGR8_OES,false);
	gfxSetDoubleBuffering(GFX_TOP,true);
	gfxSetDoubleBuffering(GFX_BOTTOM,false);
	consoleInit(GFX_BOTTOM,NULL);
	cleartop();
	started=1;
	printf("srb2 2.2.15 o3ds test\n");
	printf("if this works im not touching it\n\n");
		return 0;
}

void I_ShutdownSystem(void)
{
	int i;
	if(!started||shuttingdown) return;
	shuttingdown=1;
	for(i=nquits-1;i>=0;i--) if(quits[i]) quits[i]();
	nquits=0;
	gfxExit();
	started=0;
}

void I_GetDiskFreeSpace(INT64 *f){*f=INT_MAX;}
char *I_GetUserName(void){return "3ds";}
INT32 I_mkdir(const char *d,INT32 rights)
{
	int r=mkdir(d,rights?rights:0755);
	if(r==-1&&errno==EEXIST)return 0;
	return r;
}
const CPUInfoFlags *I_CPUInfo(void){return NULL;}
const char *I_LocateWad(void){return "sdmc:/3ds/srb2";}
void I_GetJoystickEvents(void){}
void I_GetJoystick2Events(void){}
void I_GetMouseEvents(void){}
void I_UpdateMouseGrab(void){}
char *I_GetEnv(const char *n){return getenv(n);}
INT32 I_PutEnv(char *v){return putenv(v);}
INT32 I_ClipboardCopy(const char *d,size_t n){(void)d;(void)n;return -1;}
const char *I_ClipboardPaste(void){return NULL;}

size_t I_GetRandomBytes(char *p,size_t n)
{
	u32 r=(u32)svcGetSystemTick();
	size_t i;
	for(i=0;i<n;i++)
	{
		r=r*1664525u+1013904223u;
		p[i]=(char)(r>>24);
	}
	return n;
}

void I_RegisterSysCommands(void){}
void I_GetCursorPosition(INT32 *x,INT32 *y){if(x)*x=0;if(y)*y=0;}
void I_SetMouseGrab(boolean g){(void)g;}
const char *I_GetSysName(void){return "Nintendo 3DS";}
void I_SetTextInputMode(boolean a){(void)a;}
boolean I_GetTextInputMode(void){return false;}

#include "../sdl/dosstr.c"
