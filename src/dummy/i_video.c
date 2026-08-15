#include <3ds.h>
#include <stdlib.h>
#include <string.h>

#include "../doomdef.h"
#include "../command.h"
#include "../i_system.h"
#include "../i_video.h"
#include "../screen.h"
#include "../v_video.h"

rendermode_t rendermode = render_soft;
rendermode_t chosenrendermode = render_soft;
boolean allow_fullscreen = false;
consvar_t cv_vidwait = CVAR_INIT ("vid_wait", "On", CV_SAVE, CV_OnOff, NULL);

static u16 pal[256];

static void blit(void)
{
	UINT8 *s = screens[0] ? screens[0] : vid.buffer;
	u16 *fb;
	int x,y;

	if (!s) return;
	fb = (u16 *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	if (!fb) return;

	// 3ds framebuffer is sideways because of course it is
	for (x=0;x<320;x++)
	{
		u16 *p = fb + (x+40)*240;
		for (y=0;y<200;y++)
			p[219-y] = pal[s[y*vid.rowbytes+x]];
	}
}

void I_StartupGraphics(void)
{
	if (!vid.buffer) VID_SetMode(0);
	graphics_started = keyboard_started = 1;
}

void I_ShutdownGraphics(void)
{
	if (vid.buffer) free(vid.buffer);
	vid.buffer = NULL;
	graphics_started = keyboard_started = 0;
}

void VID_StartupOpenGL(void){}

void I_SetPalette(RGBA_t *p)
{
	int i;
	for (i=0;i<256;i++)
		pal[i] = RGB8_to_565(p[i].s.red,p[i].s.green,p[i].s.blue);
}

INT32 VID_NumModes(void){ return 1; }
INT32 VID_GetModeForSize(INT32 w, INT32 h){ (void)w;(void)h;return 0; }
void VID_PrepareModeList(void){}

INT32 VID_SetMode(INT32 nope)
{
	(void)nope;

	if (!vid.buffer)
	{
		vid.buffer = malloc(320*200*NUMSCREENS);
		if (!vid.buffer) I_Error("no ram for pixels. great");
		memset(vid.buffer,0,320*200*NUMSCREENS);
	}

	vid.modenum=0;
	vid.rowbytes=320;
	vid.width=320;
	vid.height=200;
	vid.u.numpages=1;
	vid.recalc=1;
	vid.direct=NULL;
	vid.bpp=1;
	return 0;
}

boolean VID_CheckRenderer(void)
{
	rendermode = chosenrendermode = render_soft;
	return false;
}

void VID_CheckGLLoaded(rendermode_t oldrender){ (void)oldrender; }
const char *VID_GetModeName(INT32 n){ (void)n;return "320x200"; }
UINT32 I_GetRefreshRate(void){ return 60; }
void I_UpdateNoBlit(void){}

void I_FinishUpdate(void)
{
	blit();
	gfxFlushBuffers();
	gfxScreenSwapBuffers(GFX_TOP,false);
	gspWaitForVBlank();
}

void I_UpdateNoVsync(void)
{
	blit();
	gfxFlushBuffers();
	gfxScreenSwapBuffers(GFX_TOP,false);
}

void I_WaitVBL(INT32 n){ while(n-->0) gspWaitForVBlank(); }

void I_ReadScreen(UINT8 *out)
{
	if (out && screens[0]) memcpy(out,screens[0],vid.rowbytes*vid.height);
}

void I_BeginRead(void){}
void I_EndRead(void){}
