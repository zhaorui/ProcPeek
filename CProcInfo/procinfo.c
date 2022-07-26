//
//  procinfo.c
//  
//
//  Created by saast on 2022/7/21.
//

#include "procinfo.h"


static struct proc_fdinfo *Fds = (struct proc_fdinfo*)NULL;     // FD buffer
static int NbFds = 0;                    // number of bytes allocated to FDs

static int *Pids = (int *)NULL;       /* PID buffer */
static int NbPids = 0;                /* bytes allocated to Pids */

// MARK: - TCP Ports
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

// MARK: - Pids
#define PIDS_INCR    (sizeof(int) * 32)    /* PID space increment */

/*
 * proc_argv0() -- get argv[0] for process
 */

static char * proc_argv0(int pid)
{
    char            *argv0;
    char            *cp;
    static int      argmax  = 0;
    int             mib[3];
    static char     *argbuf = NULL;
    int             ret;
    size_t          syssize;

    if (argmax == 0) {
        mib[0] = CTL_KERN;
        mib[1] = KERN_ARGMAX;
        syssize = sizeof(argmax);
        ret = sysctl(mib, 2, &argmax, &syssize, NULL, 0);
        if (ret == -1) {
            fprintf(stderr, "can't get max argument size: %s\n", strerror(errno));
            exit(1);
        }
    }

    if (argbuf == NULL) {
        argbuf = malloc(argmax);
        if (!argbuf) {
            fprintf(stderr, "can't allocate space for argument buffer: %s\n", strerror(errno));
            exit(1);
        }
    }

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROCARGS2;
    mib[2] = pid;
    syssize = (size_t)argmax;
    ret = sysctl(mib, 3, argbuf, &syssize, NULL, 0);
    if (ret == -1) {
        fprintf(stderr, "PID %d: can't get argument buffer: %s\n", pid, strerror(errno));
        return NULL;
    }

    /* skip 'argc' */
    cp = argbuf + sizeof(int);

    /* The next chunk of data is the exec_path, skip it */
    for ( ; cp < &argbuf[syssize]; cp++) {
        if (*cp == '\0') {
            break;    /* end of exec_path reached */
        }
    }

    /* Skip trailing '\0' characters. */
    for ( ; cp < &argbuf[syssize]; cp++) {
        if (*cp != '\0') {
            break;    /* start of first argument reached. */
        }
    }

    /* Get the process name, removing the path if it exists */
    argv0 = cp;
    for ( ; *cp != '\0' && cp < &argbuf[syssize]; cp++) {
        if (*cp == '/') {
            argv0 = cp + 1;
        }
    }

    return argv0;
}


int listProcs(struct proc_profile* profiles[], int* count) {
    int nb, ef, np;
    struct proc_taskallinfo tai;
    char *cmd;
    int cmdlen;
    
    /*
     * Determine how many bytes are needed to contain the PIDs on the system;
     * make sure sufficient buffer space is allocated to hold them (and a few
     * extra); then read the list of PIDs.
     */
    if ((nb = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0)) <= 0) {
        fprintf(stderr, "can't get PID byte count: %s\n", strerror(errno));
        return 1;
    }
    
    if (nb > NbPids) {
        while (nb > NbPids) {
            NbPids += PIDS_INCR;
        }
        
        if (!Pids)
            Pids = (int *)malloc(NbPids);
        else
            Pids = (int *)realloc(Pids, NbPids);
        
        if (!Pids) {
            fprintf(stderr, "can't allocate space for %d PIDs\n", (int)(NbPids / sizeof(int *)));
            return 1;
        }
    }
    
    /*
     * Get the list of PIDs.
     */
    for (ef = 0; !ef;) {
        if ((nb = proc_listpids(PROC_ALL_PIDS, 0, Pids, NbPids)) <= 0) {
            fprintf(stderr, "can't get list of PIDs: %s\n", strerror(errno));
            return 1;
        }

        if ((nb + sizeof(int)) < NbPids) {

            /*
             * There is room in the buffer for at least one more PID.
             */
            np = nb / sizeof(int);
            ef = 1;
        } else {

            /*
             * The PID buffer must be enlarged.
             */
            NbPids += PIDS_INCR;
            Pids = (int *)realloc(Pids, NbPids);
            if (!Pids) {
                fprintf(stderr, "can't allocate space for %d PIDs\n", (int)(NbPids / sizeof(int *)));
                return 1;
            }
        }
    }
    
    int cnt = 0;
    struct proc_profile* results = (struct proc_profile*)malloc(sizeof(struct proc_profile) * np);
    
    /*
     * Loop through the identified processes.
     */
    for (int i = 0; i < np; i++) {
        int pid;
        if (!(pid = Pids[i]))
            continue;
        
        nb = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &tai, sizeof(tai));
        
        if (nb <= 0) {
            if ((errno == EPERM) || (errno == ESRCH))
                continue;
            fprintf(stderr, "PID %d information error: %s\n", pid, strerror(errno));
            continue;
        } else if (nb < sizeof(tai)) {
            fprintf(stderr, "PID %d: proc_pidinfo(PROC_PIDTASKALLINFO);\n", pid);
            fprintf(stderr, "      too few bytes; expected %ld, got %d\n", sizeof(tai), nb);
            return 1;
        }
        
        
        /*
         * Capture the process name
         */
        tai.pbsd.pbi_name[sizeof(tai.pbsd.pbi_name) - 1] = '\0';
        if (tai.pbsd.pbi_name[0]) {
            cmd = tai.pbsd.pbi_name;
            if (tai.pbsd.pbi_name[sizeof(tai.pbsd.pbi_name) - 2] != '\0') {
                cmdlen = sizeof(tai.pbsd.pbi_name) - 1;
            } else {
                cmdlen = 0;
            }
        } else {
            tai.pbsd.pbi_comm[sizeof(tai.pbsd.pbi_comm) - 1] = '\0';
            if (tai.pbsd.pbi_comm[0]) {
                cmd = tai.pbsd.pbi_comm;
                if (tai.pbsd.pbi_comm[sizeof(tai.pbsd.pbi_comm) - 2] != '\0') {
                    cmdlen = sizeof(tai.pbsd.pbi_comm) - 1;
                } else {
                    cmdlen = 0;
                }
            } else {
                cmd = NULL;
                cmdlen = 0;
            }
        }
        
        if ( cmd == NULL || cmdlen != 0 ) {
            
            char *argv0;
            /*
            * We need to copy the full command name from the process
            * if the data returned by PROC_PIDTASKALLINFO does not
            * include the command name, if we are printing command
            * names longer than the number of characters we already
            * have available, or if we need to match (or exclude)
            * command names and we don't have the full name.
            */
            argv0 = proc_argv0(pid);
            if (argv0 != NULL) {
                cmd = argv0;
                results[cnt].pid = pid;
                results[cnt].name = strdup(cmd);
                cnt++;
            }
        } else {
            results[cnt].pid = pid;
            results[cnt].name = strdup(cmd);
            cnt++;
        }
    }
    
    *profiles = results;
    *count = cnt;
}
