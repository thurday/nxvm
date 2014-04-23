
#ifndef NXVM_VLOG_H
#define NXVM_VLOG_H

#include "stdio.h"
#include "../vmachine/vglobal.h"

#define VLOG_COUNT_MAX 0x1000

typedef struct {
	FILE *logfile;
	t_nubitcc line;
} t_log;

extern t_log vlog;

void vlogInit();
void vlogFinal();
void vlogExec();

#endif