#include <3ds.h>
#include <unistd.h>

u32 __stacksize__ = 128 * 1024;

#include "../doomdef.h"
#include "../d_main.h"
#include "../i_system.h"
#include "../m_argv.h"

int main(int argc, char **argv)
{
	static char *a[] = {
		"srb2-o3ds",
		"-warp", "GFZ1",
		"-noaudio",
		"-width", "320",
		"-height", "200",
		"-home", "sdmc:/3ds",
		NULL
	};

	(void)argc; (void)argv;
	myargc = 10;
	myargv = a;

	I_StartupSystem();
	I_SetTextInputMode(false);

	if (chdir("sdmc:/3ds/srb2"))
		I_Error("cant cd to sdmc:/3ds/srb2");

	printf("trying gfz1 lol\n");
	D_SRB2Main();

	D_SRB2Loop();
	return 0;
}
