#include "ruby.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#ifdef _POSIX_ASYNCHRONOUS_IO
#include    <aio.h>
#else
....
#endif

static VALUE mAio, eAio;

struct rb_aiocb {
    struct aiocb a;
    off_t curr_offset;
    int wait;
    char *buffer;
};

#define BUFSIZE 4096

static void rb_aio_signal_completion_handler( int signo, siginfo_t *info, void *context )
{
    if (signo == SIGIO) { 
      printf( "done\n" );
    }

  return;
}

static int rb_aio_schedule( int argc, VALUE* argv, struct rb_aiocb * callback, int operation ) 
{
	VALUE fname, wait, offset, bytes;
    struct stat stats;
    rb_scan_args(argc, argv, "04", &fname, &wait, &offset, &bytes);
    Check_Type(fname, T_STRING);  
    FILE * file = fopen( RSTRING_PTR(fname), "r+" );

    if (offset == Qnil)
      offset = 0;
    if (bytes == Qnil)
      fstat(fileno(file), &stats);
      bytes = stats.st_size;
    if (wait == Qnil)
      wait = rb_const_get(mAio, rb_intern("WAIT") );

    char buffer[bytes];

    memset( callback, 0, sizeof(struct rb_aiocb));

    (*callback).wait = NUM2INT(wait);
    (*callback).a.aio_fildes = fileno(file);
    (*callback).a.aio_buf = (*callback).buffer = buffer;
    (*callback).a.aio_nbytes = FIX2INT(bytes);
    (*callback).a.aio_offset = FIX2INT(offset);
    (*callback).a.aio_reqprio = 0;
    (*callback).a.aio_lio_opcode = operation;
    (*callback).a.aio_sigevent.sigev_notify = SIGEV_NONE;

  return fileno(file);
}

static void rb_aio_error( char * msg ){
    rb_raise( eAio, msg );
}

static void rb_aio_schedule_read_error(){
	switch(errno){
	  case EOVERFLOW:
	    rb_aio_error( "The file is a regular file, aiocbp->aio_nbytes is greater than 0 and the starting offset in aiocbp->aio_offset is before the end-of-file and is at or beyond the offset maximum in the open file descriptor associated with aiocbp->aio_fildes." );
		break;
	  case EAGAIN:
	   	rb_aio_error( "At least one request could not be queued either because of a resource shortage or because the per-process or system-wide limit on asynchronous I/O operations or asynchronous threads would have been exceeded." );
		break;
	  case EINVAL:
	    rb_aio_error( "The sigevent specified by sig is not valid." );
		break;
	  case EINTR:
	    rb_aio_error( "The mode argument was LIO_WAIT and a signal was delivered while waiting for the requested operations to complete. This signal may result from completion of one or more of the requested operations and other requests may still be pending or completed." );
	  case EBADF:
	rb_aio_error( "The aiocbp->aio_fildes was not a valid file descriptor open for reading or writing as appropriate to the aio_lio_opcode." );	
	  case EIO:
	    rb_aio_error( "One or more of the enqueued operations did not complete successfully." );
		break;
	} 
}

static void rb_aio_install_signal_handler(){
    stack_t          ss;
    struct sigaction new_act,old_act;

    /* Init signal handler */
    ss.ss_sp = malloc(SIGSTKSZ);
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;

    if (sigaltstack(&ss,NULL)==-1) {
	    rb_aio_error( "Error generating signal stack." );
    }

    if (sigaction( SIGIO, NULL, &new_act) < 0) {
        rb_aio_error( "Not able to install the aio signal handler." );
    }
    new_act.sa_sigaction = rb_aio_signal_completion_handler;
    sigemptyset( &new_act.sa_mask );
    sigaddset( &new_act.sa_mask, SIGIO);
    new_act.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
    if (sigaction( SIGIO, &new_act, &old_act ) < 0) {
        rb_aio_error( "Not able to install the aio signal handler." );
    }	
}

static VALUE rb_aio_schedule_read( int argc, VALUE* argv, VALUE aio ){
    struct rb_aiocb callback; 
    struct aiocb *list[1];
    int fd = rb_aio_schedule( argc, argv, &callback, LIO_READ );
    struct sigevent list_sig;  

    rb_aio_install_signal_handler();

    list[0] = &callback.a;

    list_sig.sigev_notify = SIGEV_SIGNAL;
    list_sig.sigev_signo = SIGIO;

    if ( lio_listio( callback.wait, list, 1, &list_sig ) ) {  
  	  rb_aio_schedule_read_error();
    }	
    close(fd);

  return Qtrue;
}

static VALUE rb_aio_limits( int argc, VALUE* argv, VALUE aio ){
}

void Init_aio()
{
    mAio = rb_define_module("AIO");

    rb_define_const(mAio, "WAIT", INT2NUM(LIO_WAIT));
    rb_define_const(mAio, "NOWAIT", INT2NUM(LIO_NOWAIT));

    eAio = rb_define_class_under(mAio, "Error", rb_eStandardError);

    rb_define_module_function( mAio, "read", rb_aio_schedule_read, -1 );

}