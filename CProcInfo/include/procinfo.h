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

int listLocalTcpPorts(pid_t pid, int* ports[], int* count);


#endif /* procinfo_h */
