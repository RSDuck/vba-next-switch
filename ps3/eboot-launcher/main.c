#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <stddef.h>


#include <sys/return_code.h>
#include <sys/process.h>
#include <sys/memory.h>
#include <sys/timer.h>
#include <sys/paths.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cell/sysmodule.h>
#include <cell/cell_fs.h>
#include <sysutil/sysutil_sysparam.h>
#include <sysutil/sysutil_common.h>

SYS_PROCESS_PARAM(1001, 0x100000)

int main(int argc, char **argv) 
{
	(void) argc;
	char tn[128];
	char filename[128];

	sprintf(tn, "%s", argv[0]);
	char *pch=tn;
	char *pathpos=strrchr(pch,'/');
	pathpos[0]=0;

	sprintf(filename, "%s/RELOAD.SELF", tn);
	sys_game_process_exitspawn2(filename, NULL, NULL, NULL, 0, 1001, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
}


