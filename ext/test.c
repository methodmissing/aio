/*+++*******************************************************************
 * AIO simple test program
 ***********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <aio.h>

#define BLKSIZE 4096

#if __linux__
#define AIO_SIGNAL  SIGUSR2
#else
// #define AIO_SIGNAL  SIGEMT
#define AIO_SIGNAL  SIGUSR2
#endif

typedef struct aiocb AIOCB;

long aio_max;
long aio_listio_max;

/*^L*/
/*+
**      NAME:
**              handler_wait
**
**      SYNOPSIS:
**
**      DESCRIPTION:
**
**      RETURN VALUES:
**
**              PS_SUCCESS or error msg
**
**      FUNCTIONS USED:
**
**
-*/
int handler_wait() {
    struct timeval tv;
 
    fprintf(stdout,"Wait for signal handler (5 sec)\n");

    /* Wait up to five seconds. */
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    do {
	errno = 0;
	select(0, NULL, NULL, NULL, &tv);
    } while(errno==EINTR);	    
}

/*^L*/
/*+
**      NAME:
**              gen_file
**
**      SYNOPSIS:
**
**      DESCRIPTION:
**
**      RETURN VALUES:
**
**              PS_SUCCESS or error msg
**
**      FUNCTIONS USED:
**
**
-*/
int gen_file(int async, int n_fd, int n_aio) {
    int     nent;
    int     retval,i;
    int     filed;
    char    filename[255];
    int     fd[10];
    AIOCB   *list[255];
    AIOCB   *tmpaiocb;
    struct sigevent list_sig;

    list_sig.sigev_notify          = SIGEV_SIGNAL;
    list_sig.sigev_signo           = AIO_SIGNAL;
    list_sig.sigev_value.sival_ptr = (void *)1;

    for(i=0;i<n_fd;i++) {
	sprintf(filename,"AIO-FILE-%03i",i);
	fd[i] = open( filename, O_CREAT|O_RDWR,0600);
    }

    /* Write AIO list */
    fprintf(stdout,"Generate WRITE AIO list : \n");
    for(i=0;i<n_aio;i++) {
	tmpaiocb = (AIOCB *) malloc(sizeof(AIOCB));
	memset( tmpaiocb, 0, sizeof (AIOCB));
	tmpaiocb->aio_fildes     = fd[i%n_fd];

	tmpaiocb->aio_offset     = i*BLKSIZE;
	tmpaiocb->aio_reqprio    = 0;
	tmpaiocb->aio_buf        = (void *) malloc(BLKSIZE);
	tmpaiocb->aio_nbytes     = BLKSIZE;

	memcpy((void *)tmpaiocb->aio_buf,&i,sizeof(int));

	tmpaiocb->aio_lio_opcode = LIO_WRITE;
	tmpaiocb->aio_sigevent.sigev_notify = SIGEV_NONE;

	fprintf(stdout,"Got %p Slot %3i: fd=%i with offset=%4i size=%4i\n",
		tmpaiocb, i, tmpaiocb->aio_fildes,tmpaiocb->aio_offset,tmpaiocb->aio_nbytes);
	list[i]=tmpaiocb;
    }

    if (async) {
	fprintf(stdout,"Start async. lio_listio\n");
	retval      = lio_listio( LIO_NOWAIT, list, n_aio, &list_sig );
    } else {
	fprintf(stdout,"Start  sync. lio_listio\n");
	retval      = lio_listio( LIO_WAIT,   list, n_aio, &list_sig );
    }
    if (retval) {
	fprintf(stdout,"number=%i lio_listio ERROR (%i): %s\n", n_aio, errno, strerror(errno));
    }

    if (async) {
	handler_wait();
    }

    for(i=0;i<n_aio;i++) {
	if ((async) && ((retval=aio_suspend(list+i, 1, NULL))==-1)) {
	    fprintf(stdout,"aio_suspend stated (%i) : %s\n",
		    errno, strerror(errno));
	}
	tmpaiocb   = list[i];
	if ((retval=aio_error(tmpaiocb))!=0) {
	    if (retval == -1) {
		fprintf(stdout,"%3i. (%p) ERROR AIO : %s\n",
			i,tmpaiocb, strerror(errno));
	    } else {
		fprintf(stdout,"%3i.entry has errno (%i) : %s\n",
			i, retval, strerror(retval));
	    }
	} else {
	    fprintf(stdout,"%3i.entry o.k.\n",
		    i);
	}
    }


}

/*^L*/
/*+
**      NAME:
**              aio_signal_handler
**
**      SYNOPSIS:
**
**      DESCRIPTION:
**
**      RETURN VALUES:
**
**              PS_SUCCESS or error msg
**
**      FUNCTIONS USED:
**
**
-*/
void aio_signal_handler(int sig_nr, siginfo_t * si, void *   uc) {
    long        index;
    sigset_t   sset;

    /* AIO handler for aio_??? and lio_listio */
    index = (long) si->si_value.sival_ptr;
    fprintf(stdout,"AIO signal handler for request %i started\n", index);

}

/*^L*/
/*+
**      NAME:
**              init_signal
**
**      SYNOPSIS:
**
**      DESCRIPTION:
**
**      RETURN VALUES:
**
**              PS_SUCCESS or error msg
**
**      FUNCTIONS USED:
**
**
-*/
int init_signal() {
    stack_t          ss;
    struct sigaction new_act,old_act;

    /* Init signal handler */
    ss.ss_sp = malloc(SIGSTKSZ);
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;

    if (sigaltstack(&ss,NULL)==-1) {
	fprintf(stderr,"Error generating signal stack\n");
    }

    if (sigaction(AIO_SIGNAL, NULL, &new_act)<0) {
        fprintf(stderr,"ERROR setting signal handle\n");
        exit(1);
    }
    new_act.sa_sigaction = aio_signal_handler;
    sigemptyset(&new_act.sa_mask);
    sigaddset(&new_act.sa_mask,AIO_SIGNAL);
    new_act.sa_flags = SA_SIGINFO|SA_RESTART|SA_ONSTACK;
    if (sigaction(AIO_SIGNAL, &new_act, &old_act)<0) {
        fprintf(stderr,"ERROR setting signal handle\n");
        exit(1);
    }

}

/*^L*/
/*+
**      NAME:
**              main
**
**      SYNOPSIS:
**
**      DESCRIPTION:
**
**      RETURN VALUES:
**
**              PS_SUCCESS or error msg
**
**      FUNCTIONS USED:
**
**
-*/
int main(int argc,char **argv) {
    int              option;

    /*
     * Get kernel parameter (AIO maximal parameter)
     */
#if __linux__
    aio_max        = sysconf(_POSIX_AIO_MAX);
    aio_listio_max = sysconf(_POSIX_AIO_LISTIO_MAX); 
#else
    aio_max        = sysconf(_SC_AIO_MAX);
    aio_listio_max = sysconf(_SC_AIO_LISTIO_MAX); 
#endif

    fprintf(stdout,"Limits are AIO_MAX=%i and AIO_LISTIO_MAX=%i\n",
	    aio_max, aio_listio_max);

    init_signal();

#if 0
    /* Generate AIO file */
    if (gen_file(1,1,2)==1) {
	exit(1);
    }
#endif

    /* Generate AIO file */
    if (gen_file(1,1,2)==1) {
	exit(1);
    }

	if (gen_file(1,1,3)==1) {
	exit(1);
    }
    return 0;

}