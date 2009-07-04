#include "ruby.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#ifdef _POSIX_ASYNCHRONOUS_IO
#include <aio.h>
#endif

#ifndef RSTRING_PTR
#define RSTRING_PTR(obj) RSTRING(obj)->ptr
#endif
 
#ifndef RSTRING_LEN
#define RSTRING_LEN(obj) RSTRING(obj)->len
#endif

#ifndef RARRAY_PTR
#define RARRAY_PTR(obj) RARRAY(obj)->ptr
#endif
 
#ifndef RARRAY_LEN
#define RARRAY_LEN(obj) RARRAY(obj)->len
#endif

#ifdef HAVE_TBR
  #include "ruby/io.h" 
  #define TRAP_BEG
  #define TRAP_END
#else
  #include "rubysig.h"
  #include "rubyio.h"
#endif

/* Max I/O operations across all supported platforms */
#define AIO_MAX_LIST 16

static VALUE mAio, eAio;

typedef struct aiocb aiocb_t;

/* Allows for passing n-1 arguments to the first rb_ensure function call */
struct aio_read_multi_args {
	aiocb_t **list;
	int reads; 
};

static void rb_aio_error( char * msg ){
    rb_raise( eAio, msg );
}

/*
 *  Error handling for aio_read
 */
static void rb_aio_read_error(){
    switch(errno){
       case EAGAIN: 
	        rb_aio_error( "The request cannot be queued due to exceeding resource (queue) limitations." );
		  break;
       case EBADF: 
	        rb_aio_error( "File descriptor is not valid for reading." );
			break;
       case EINVAL: 
            rb_aio_error( "Read offset is invalid" );
			break;
	}
}

/*
 *  Initiates a *blocking* aio_read
 */
static VALUE rb_aio_read( aiocb_t *cb ){
	int ret;
	
	TRAP_BEG;
    ret = aio_read( cb );
	TRAP_END;
	if (ret != 0) rb_aio_read_error();
	while ( aio_error( &cb ) == EINPROGRESS );
	if ((ret = aio_return( cb )) > 0) {
		return rb_tainted_str_new( (*cb).aio_buf, (*cb).aio_nbytes );
	}else{
		return INT2NUM(errno);
	}	
}

/*
 *  Error handling for lio_listio
 */
static void rb_aio_read_multi_error(){
    switch(errno){
       case EAGAIN: 
	        rb_aio_error( "Resources necessary to queue all the requests are not available at the moment." );
		  break;
       case EIO: 
	        rb_aio_error( "One or more requests failed" );
			break;
       case EINVAL: 
            rb_aio_error( "Maximum number of allowed simultaneous requests exceeded." );
			break;
	}
}

/*
 *  Initiates lio_listio
 */
static VALUE rb_aio_read_multi( struct aio_read_multi_args *args ){
	int op, ret;
    VALUE results = rb_ary_new2( args->reads );
	
	TRAP_BEG;
    ret = lio_listio( LIO_WAIT, (*args).list, args->reads, NULL );
	TRAP_END;
	if (ret != 0) rb_aio_read_multi_error();
    for (op=0; op < args->reads; op++) {
		rb_ary_push( results, rb_tainted_str_new( (*args->list)[op].aio_buf, (*args->list)[op].aio_nbytes ) );
    }
	return results;
}

/*
 *  Helper to ensure files opened via AIO.read_multi is closed.
 */
static void rb_io_closes( VALUE ios ){
    int io;

    for (io=0; io < RARRAY_LEN(ios); io++) {
  	  rb_io_close( RARRAY_PTR(ios)[io] );
    } 
}

/*
 *  Setup a AIO control block for the given file descriptor and number of bytes to read.
 *  Defaults to reading the entire file size into the buffer.Granular offsets would be 
 *  supported in a future version.
 */
static void setup_aio_cb( aiocb_t *cb, int *fd, int *length ){
    bzero(cb, sizeof(aiocb_t));

	(*cb).aio_buf = malloc(*length + 1);
	if (!(*cb).aio_buf) rb_aio_error( "not able to allocate a read buffer" );

	(*cb).aio_fildes = *fd;
	(*cb).aio_nbytes = *length;
	(*cb).aio_offset = 0;
	(*cb).aio_sigevent.sigev_notify = SIGEV_NONE;
	(*cb).aio_sigevent.sigev_signo = 0;
	(*cb).aio_sigevent.sigev_value.sival_int = 0;
	(*cb).aio_lio_opcode = LIO_READ;
}

/*
 *  call-seq:
 *     AIO.read('file1') -> string
 *  
 *  Asynchronously reads a file.This is an initial *blocking* implementation until
 *  cross platform notification is supported.
 */
static VALUE rb_aio_s_read( VALUE aio, VALUE file ){
#ifdef HAVE_TBR
	rb_io_t *fptr;
#else	
	OpenFile *fptr;
#endif
	int fd, length;
    VALUE io;

    struct stat stats;
	aiocb_t cb;
   
    Check_Type(file, T_STRING);	
	
    io = rb_file_open(RSTRING_PTR(file), "r");
    GetOpenFile(io, fptr);
    rb_io_check_readable(fptr); 	

#ifdef HAVE_TBR
  	  fd = fptr->fd;
#else	
  	  fd = fileno(fptr->f);
#endif
      fstat(fd, &stats);
      length = stats.st_size;
	  setup_aio_cb( &cb, &fd, &length );

    return rb_ensure( rb_aio_read, &cb, rb_io_close, io );
}

/*
 *  call-seq:
 *     AIO.read_multi('file1','file2', ...) -> array
 *  
 *  Schedules a batch of read requests for execution by the kernel in order
 *  to reduce system calls.Blocks until all the requests complete and returns
 *  an array equal in length to the given files, with the read buffers as string
 *  elements.The number of operations is currently limited to 16 due to cross 
 *  platform limitations. 
 *  
 *  open_nocancel("first.txt\0", 0x0, 0x1B6)	 = 3 0
 *  fstat(0x3, 0xBFFFEE04, 0x1B6)	 = 0 0
 *  open_nocancel("second.txt\0", 0x0, 0x1B6)	 = 4 0
 *  fstat(0x4, 0xBFFFEE04, 0x1B6)	 = 0 0
 *  open_nocancel("third.txt\0", 0x0, 0x1B6)	 = 5 0
 *  fstat(0x5, 0xBFFFEE04, 0x1B6)	 = 0 0
 *  fstat64(0x1, 0xBFFFE234, 0x1B6)	 = 0 0
 *  ioctl(0x1, 0x4004667A, 0xBFFFE29C)	 = 0 0
 *  lio_listio(0x2, 0xBFFFEE64, 0x3)	 = 0 0
 *  close_nocancel(0x4)	 = 0 0
 *  close_nocancel(0x3)	 = 0 0
 */
static VALUE rb_aio_s_read_multi( VALUE aio, VALUE files ){
#ifdef HAVE_TBR
	rb_io_t *fptrs[AIO_MAX_LIST];
#else	
	OpenFile *fptrs[AIO_MAX_LIST];
#endif
    struct aio_read_multi_args args;
	int fd, length, op;
    struct stat stats;
	aiocb_t cb[AIO_MAX_LIST];
	aiocb_t *list[AIO_MAX_LIST]; 
	
	int reads = RARRAY_LEN(files);
	if (reads > AIO_MAX_LIST) rb_aio_error( "maximum number of I/O calls exceeded!" );
	VALUE ios = rb_ary_new2( reads );
	VALUE io;    

    bzero( (char *)list, sizeof(list) );
    for (op=0; op < reads; op++) {
      Check_Type(RARRAY_PTR(files)[op], T_STRING);
      io = rb_file_open(RSTRING_PTR(RARRAY_PTR(files)[op]), "r");
	  rb_ary_push( ios, io );
	  GetOpenFile(io, fptrs[op]);
	  rb_io_check_readable(fptrs[op]); 	

#ifdef HAVE_TBR
  	  fd = fptrs[op]->fd;
#else	
  	  fd = fileno(fptrs[op]->f);
#endif
      fstat(fd, &stats);
      length = stats.st_size;
      setup_aio_cb( &cb[op], &fd, &length );
      list[op] = &cb[op];
    }
	args.list = list;
	args.reads = reads;
    return rb_ensure( rb_aio_read_multi, &args, rb_io_closes, ios );
}

void Init_aio()
{	
    mAio = rb_define_module("AIO");

    rb_define_const(mAio, "WAIT", INT2NUM(LIO_WAIT));
    rb_define_const(mAio, "NOWAIT", INT2NUM(LIO_NOWAIT));

    eAio = rb_define_class_under(mAio, "Error", rb_eStandardError);

    rb_define_module_function( mAio, "read_multi", rb_aio_s_read_multi, -2 );
    rb_define_module_function( mAio, "read", rb_aio_s_read, 1 );

}