#include "ruby.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#ifdef _POSIX_ASYNCHRONOUS_IO
#include    <aio.h>
#else
....
#endif

static VALUE mAio, eAio, cAioCallback;

struct rb_aiocb {
    struct aiocb a;
    off_t curr_offset;
    int notification;
    char *buffer;
};

#define BUFSIZE 4096

static void
rb_free_aio_callback( struct rb_aiocb* cb )
{
	xfree(cb);	
}

void inspect_cb( struct rb_aiocb * cb, char * context ){

  printf( "=========== %s\n", context );
  printf( "callback.buffer = %p\n", (*cb).buffer );
  printf( "callback.curr_offset = %d\n", (*cb).curr_offset );
  printf( "callback.notification = %d\n", (*cb).notification );
  printf( "callback.a.aio_fildes = %d\n", (*cb).a.aio_fildes );
  printf( "callback.a.aio_buf = %p\n", (*cb).a.aio_buf );
  printf( "callback.a.aio_nbytes = %d\n", (*cb).a.aio_nbytes );
  printf( "callback.a.aio_reqprio = %d\n", (*cb).a.aio_reqprio);
  printf( "callback.a.aio_offset = %d\n", (*cb).a.aio_offset );
  printf( "callback.a.aio_lio_opcode = %d\n", (*cb).a.aio_lio_opcode );
  printf( "callback.a.aio_sigevent.sigev_notify = %d\n", (*cb).a.aio_sigevent.sigev_notify );
  printf( "callback.a.aio_sigevent.sigev_signo = %d\n", (*cb).a.aio_sigevent.sigev_signo );
  printf( "callback.a.aio_sigevent.sigev_notify_function = %p\n", (*cb).a.aio_sigevent.sigev_notify_function);
  printf( "callback.a.aio_sigevent.sigev_notify_attributes = %p\n", (*cb).a.aio_sigevent.sigev_notify_attributes );  
  printf( "callback.a.aio_sigevent.sigev_value.sival_int = %d\n", (*cb).a.aio_sigevent.sigev_value.sival_int );  
  printf( "callback.a.aio_sigevent.sigev_value.sival_ptr = %p\n", (*cb).a.aio_sigevent.sigev_value.sival_ptr );
}  

static void rb_aio_signal_completion_handler( int signo, siginfo_t *info, void *context )
{
  struct rb_aiocb *callback;
  ssize_t bytes_read;
  int ret;

  if (signo == SIGUSR2) { 
    callback = (struct rb_aiocb *)(info->si_value.sival_ptr);
    printf("aio_signal_completion_handler %d %d %d %d %d %d %p %d %p\n", info->si_signo, info->si_errno, info->si_code, info->si_pid, info->si_uid, info->si_status, info->si_addr, info->si_value.sival_int, info->si_value.sival_ptr );
    /*inspect_cb( &callback, "aio_signal_completion_handler" );*/
	ret = aio_error( &callback->a );
		printf("%d\n", ret);
/*
    if ( ( ret = aio_error( &callback->a ) ) != EINPROGRESS )
      rb_raise( eAio, "read failure" );*/
  }

  return;
}

static void rb_aio_schedule_with_signal_callback( struct rb_aiocb * callback ){
  printf("aio_schedule_with_signal_callback");
  struct sigaction sig_act;

  sig_act.sa_sigaction = rb_aio_signal_completion_handler;
  sig_act.sa_flags = SIGUSR2;
  sigemptyset( &sig_act.sa_mask );
  sigaction( SIGUSR2, &sig_act, 0 );

  inspect_cb( callback, "before aio_schedule_with_signal_callback" );

  (*callback).a.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
  (*callback).a.aio_sigevent.sigev_signo = SIGUSR2;

  (*callback).a.aio_sigevent.sigev_value.sival_ptr = callback;
  (*callback).a.aio_sigevent.sigev_notify_attributes = callback;
  (*callback).a.aio_sigevent.sigev_value.sival_int = SIGUSR2;

  inspect_cb( callback, "after aio_schedule_with_signal_callback" );
}

static void rb_aio_schedule_without_callback( struct rb_aiocb * callback ){
  printf("aio_schedule_without_callback");
}

void rb_aio_schedule( int argc, VALUE* argv, struct rb_aiocb * callback ) 
{
  VALUE fname, strategy, offset, bytes;
  int ret;
  char buffer[BUFSIZE];
  rb_scan_args(argc, argv, "04", &fname, &strategy, &offset, &bytes);
  if (offset == Qnil)
    offset = 0;
  if (bytes == Qnil)
    bytes = BUFSIZE;
  if (strategy == Qnil)
    strategy = rb_const_get(mAio, rb_intern("NOTIFY_NONE") );
  Check_Type(fname, T_STRING);  
  
  FILE * file = fopen( RSTRING_PTR(fname), "r+" );

  memset( callback, 0, sizeof(struct rb_aiocb));

  (*callback).notification = NUM2INT(strategy);
  (*callback).a.aio_fildes = fileno(file);
  (*callback).a.aio_buf = (*callback).buffer = buffer;
  (*callback).a.aio_nbytes = FIX2INT(bytes);
  (*callback).a.aio_offset = FIX2INT(offset);
  (*callback).a.aio_reqprio = 0;

  switch(NUM2INT(strategy)){
    case SIGEV_NONE:
      rb_aio_schedule_without_callback( callback );
      break;
    case SIGEV_SIGNAL:
      rb_aio_schedule_with_signal_callback( callback ); 
      break;
  }
  inspect_cb( callback, "aio_schedule" );  
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
	   	rb_aio_error( "The requested asynchronous I/O operation was not queued because of system resource limitations." );
		break;
	  case EFAULT:
	    rb_aio_error( "aiocbp or the aio_buf member points outside the allocated address space." );
		break;
	  case ECANCELED:
	    rb_aio_error( "The requested I/O was canceled before the I/O completed because of aio_cancel." );
		break;
	  case EINVAL:
	    rb_aio_error( "On SVR4.2, the request does not have the AIO_RAW flag bit set in aio_flags." );
		break;
	  case ENOMEM:
	    rb_aio_error( "There were no internal kernal aio control blocks available to service the request (number of kernel aio control blocks is tunable via the NUMAIO kernel parameter" );
		break;
	} 
}

static VALUE rb_aio_schedule_read( int argc, VALUE* argv, VALUE aio ){
  VALUE obj;
  int ret;
  struct rb_aiocb callback; 
  rb_aio_schedule( argc, argv, &callback );
  /*inspect_cb( &callback, "before aio_schedule_read" );*/
  if ( ret = aio_read( &callback.a ) == -1 ){
	  /*inspect_cb( &callback, "after aio_schedule_read" ); */ 
	rb_aio_schedule_read_error();
  }	
  return Qtrue; 
  /*return Data_Make_Struct( cAioCallback, struct rb_aiocb, 0, rb_free_aio_callback, callback );
  rb_obj_call_init(obj,0,0);
  return obj; */
}

static VALUE rb_aio_supported_p( int argc, VALUE* argv, VALUE aio ){
    
}

void Init_aio()
{
  mAio = rb_define_module("AIO");

  rb_define_const(mAio, "NOTIFY_NONE", INT2NUM(SIGEV_NONE));
  rb_define_const(mAio, "NOTIFY_THREAD", INT2NUM(SIGEV_THREAD));
  rb_define_const(mAio, "NOTIFY_SIGNAL", INT2NUM(SIGEV_SIGNAL));

  eAio = rb_define_class_under(mAio, "Error", rb_eStandardError);
  cAioCallback = rb_define_class_under( mAio, "Callback", rb_cObject);

  rb_define_module_function( mAio, "read", rb_aio_schedule_read, -1 );

}