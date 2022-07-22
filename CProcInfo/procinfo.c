//
//  procinfo.c
//  
//
//  Created by saast on 2022/7/21.
//

#include "procinfo.h"

static struct proc_fdinfo *Fds = (struct proc_fdinfo*)NULL;     // FD buffer
static int NbFds = 0;                    // number of bytes allocated to FDs

static inline int getLocalTcpPortFromFdInfo(struct socket_fdinfo* si) {
    int fam = si->psi.soi_family; // network family
    return (fam == AF_INET || fam == AF_INET6) && si->psi.soi_kind == SOCKINFO_TCP ?
    (int)ntohs(si->psi.soi_proto.pri_tcp.tcpsi_ini.insi_lport) : 0;
}

static int getLocalTcpPortFromFd(int pid, int32_t fd) {
    struct socket_fdinfo si;
    int nb = proc_pidfdinfo(pid, fd, PROC_PIDFDSOCKETINFO, &si, sizeof(si));
    if (nb <= 0) {
        printf("socket");
        return -1;
    } else if (nb < sizeof(si)) {
        fprintf(stderr, "PID %d, FD %d: proc_pidfdinfo(PROC_PIDFDSOCKETINFO);\n", pid, fd);
        fprintf(stderr, "       too few bytes; expected %ld, got %d\n", sizeof(si), nb);
        return -1;
    }
    return getLocalTcpPortFromFdInfo(&si);
}

static int getLocalTcpPortsFromPid(pid_t pid, uint32_t n, int* ports[], int* count) {
    int nb; // number of bytes of buffer
    int nf; // nubmer of fd
    struct proc_fdinfo *fdp;
    
    // Make sure an FD buffer has been allocated.
    if (!Fds) {
        NbFds = sizeof(struct proc_fdinfo) * n;
        Fds = (struct proc_fdinfo *)malloc(NbFds);
    } else if (NbFds < sizeof(struct proc_fdinfo) * n) {
        // More proc_fdinfo space is required.  Allocate it.
        NbFds = sizeof(struct proc_fdinfo) * n;
        Fds = (struct proc_fdinfo *)realloc(Fds, NbFds);
    }
    
    if (!Fds) {
        fprintf(stderr, "PID %d: can't allocate space for %d FDs\n", pid, n);
        goto FAILED;
    }
    
    // Get FD information for the process.
    nb = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, Fds, NbFds);
    if (nb <= 0) {
        if (errno == ESRCH) {
            // Quit if no FD information is available for the process.
            *ports = NULL;
            *count = 0;
            return 0;
        }
        
        fprintf(stderr, "FD info error: %s", strerror(errno));
        goto FAILED;
    }
    nf = (int)(nb / sizeof(struct proc_fdinfo));
    
    int* array = (int*)malloc(sizeof(int) * nf);
    int cnt = 0;
    
    // Loop through the file descriptors.
    for (int i = 0; i < nf; i++) {
        fdp = &Fds[i];
        if (fdp->proc_fdtype == PROX_FDTYPE_SOCKET) {
            int port = getLocalTcpPortFromFd(pid, fdp->proc_fd);
            if (port > 0) {
                array[cnt] = port;
                cnt++;
            }
        }
    }
    
    *ports = (int*)realloc(array, sizeof(int) * cnt);
    *count = cnt;
    return 0;

FAILED:
    *ports = NULL;
    *count = 0;
    return 1;
}

int listLocalTcpPorts(pid_t pid, int* ports[], int* count) {
    int nb;
    struct proc_taskallinfo tai;
    nb = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &tai, sizeof(tai));
    
    if (nb <= 0) {
        if ((errno != EPERM) || (errno != ESRCH)) {
            fprintf(stderr, "PID %d information error: %s\n",  pid, strerror(errno));
        }
        goto FAILED;
    } else if (nb < sizeof(tai)) {
        fprintf(stderr, "PID %d: proc_pidinfo(PROC_PIDTASKALLINFO);\n",pid);
        fprintf(stderr,"too few bytes; expected %ld, got %d\n", sizeof(tai), nb);
        goto FAILED;
    }
    
    return getLocalTcpPortsFromPid(pid, tai.pbsd.pbi_nfiles, ports, count);
    
FAILED:
    *ports = NULL;
    *count = 0;
    return 1;
}

