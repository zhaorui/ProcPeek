//
//  procinfo.h
//  
//
//  Created by saast on 2022/7/21.
//

#ifndef procinfo_h
#define procinfo_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libproc.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <assert.h>

struct proc_profile {
    int pid;
    char* name; // malloc
};

int listLocalTcpPorts(pid_t pid, int* ports[], int* count);
int listProcs(struct proc_profile* profiles[], int* count);

#endif /* procinfo_h */
