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
#ifdef AIO_LISTIO_MAX
  #define AIO_MAX_LIST AIO_LISTIO_MAX
#else
  #define AIO_MAX_LIST 16
#endif

static VALUE mAio, eAio;

VALUE rb_cCB;

typedef struct aiocb aiocb_t;

typedef struct{
	aiocb_t cb;
	VALUE io; 
} rb_aiocb_t;

static void rb_aio_error( char * msg ){
    rb_raise( eAio, msg );
}

#define GetCBStruct(obj)	(Check_Type(obj, T_DATA), (rb_aiocb_t*)DATA_PTR(obj))

static void mark_control_block(rb_aiocb_t *cb)
{
	rb_gc_mark(cb->cb.aio_buf);
	rb_gc_mark(cb->io);
}

static void free_control_block(rb_aiocb_t* cb)
{
    xfree(cb);
}

static VALUE
control_block_nbytes_set(VALUE cb, VALUE bytes)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	Check_Type(bytes, T_FIXNUM);
	cbs->cb.aio_nbytes = FIX2INT(bytes);
	if (cbs->cb.aio_buf != NULL) xfree( cbs->cb.aio_buf);
	cbs->cb.aio_buf = malloc(cbs->cb.aio_nbytes + 1);
	if (!cbs->cb.aio_buf) rb_aio_error( "not able to allocate a read buffer" );	
	return bytes;
}

static VALUE
control_block_open(VALUE cb, VALUE file)
{
#ifdef HAVE_TBR
	rb_io_t *fptr;
#else	
	OpenFile *fptr;
#endif
    rb_aiocb_t *cbs = GetCBStruct(cb);

    struct stat stats;
   
    Check_Type(file, T_STRING);	

    cbs->io = rb_file_open(RSTRING_PTR(file), "r");
    GetOpenFile(cbs->io, fptr);
    rb_io_check_readable(fptr); 	

    if ( cbs->cb.aio_fildes == 0 && cbs->cb.aio_nbytes == 0){
#ifdef HAVE_TBR
      cbs->cb.aio_fildes = fptr->fd;
#else	
      cbs->cb.aio_fildes = fileno(fptr->f);
#endif
      fstat(cbs->cb.aio_fildes, &stats);
	  control_block_nbytes_set(cb, INT2FIX(stats.st_size));
    }
	return cb;    
}

static void
control_block_reset0(rb_aiocb_t *cb)
{    
    bzero(cb, sizeof(rb_aiocb_t));
    /* cleanup with rb_io_close(cb->io) */
    cb->io = Qnil;
    cb->cb.aio_fildes = 0; 
    cb->cb.aio_buf = NULL; 
    cb->cb.aio_nbytes = 0;
    cb->cb.aio_offset = 0;
    cb->cb.aio_reqprio = 0;
    cb->cb.aio_lio_opcode = LIO_READ;
   /* Disable signals for the time being */
    cb->cb.aio_sigevent.sigev_notify = SIGEV_NONE;
    cb->cb.aio_sigevent.sigev_signo = 0;
    cb->cb.aio_sigevent.sigev_value.sival_int = 0;
}

static VALUE
control_block_reset(VALUE cb)
{
	rb_aiocb_t *cbs = GetCBStruct(cb);
	control_block_reset0(cbs);
	return cb;
}

static VALUE control_block_alloc _((VALUE));
static VALUE
control_block_alloc(VALUE klass)
{
	VALUE obj;
	rb_aiocb_t *cb;
	obj = Data_Make_Struct(klass, rb_aiocb_t, mark_control_block, free_control_block, cb);
	control_block_reset0(cb);	
    return obj;
}

static VALUE
control_block_initialize(int argc, VALUE *argv, VALUE cb)
{
	VALUE file;
	rb_scan_args(argc, argv, "01", &file);
	if (RTEST(file)) control_block_open(cb, file);
	if (rb_block_given_p()) rb_obj_instance_eval( 0, 0, cb );
	return cb;
}

static VALUE 
control_block_path(VALUE cb)
{ 
	rb_aiocb_t *cbs = GetCBStruct(cb);
#ifdef HAVE_TBR
	rb_io_t *fptr;
#else	
	OpenFile *fptr;
#endif
    if NIL_P(cbs->io){
		return rb_str_new2("");
    }else{	
	   	GetOpenFile(cbs->io, fptr);
	    rb_io_check_readable(fptr);
#ifdef HAVE_TBR
		return rb_file_s_expand_path( 1, &fptr->pathv );
#else
		VALUE path = rb_str_new2(fptr->path);
		return rb_file_s_expand_path( 1, &path );
#endif
	}
}

static VALUE
control_block_fildes_get(VALUE cb)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	return INT2FIX(cbs->cb.aio_fildes);
}

static VALUE
control_block_fildes_set(VALUE cb, VALUE fd)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	Check_Type(fd, T_FIXNUM);
	cbs->cb.aio_fildes = FIX2INT(fd);
	return fd;
}

static VALUE
control_block_buf_get(VALUE cb)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	return cbs->cb.aio_buf == NULL ? Qnil : rb_str_new(&cbs->cb.aio_buf, cbs->cb.aio_nbytes);
}

static VALUE
control_block_nbytes_get(VALUE cb)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	return INT2FIX(cbs->cb.aio_nbytes);
}

static VALUE
control_block_offset_get(VALUE cb)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	return INT2FIX(cbs->cb.aio_offset);
}

static VALUE
control_block_offset_set(VALUE cb, VALUE offset)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	Check_Type(offset, T_FIXNUM);
	cbs->cb.aio_offset = FIX2INT(offset);
	return offset;
}

static VALUE
control_block_reqprio_get(VALUE cb)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	return INT2FIX(cbs->cb.aio_reqprio);
}

static VALUE
control_block_reqprio_set(VALUE cb, VALUE reqprio)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	Check_Type(reqprio, T_FIXNUM);
	cbs->cb.aio_reqprio = FIX2INT(reqprio);
	return reqprio;
}

static VALUE
control_block_lio_opcode_get(VALUE cb)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	return INT2FIX(cbs->cb.aio_lio_opcode);
}

static VALUE
control_block_lio_opcode_set(VALUE cb, VALUE opcode)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	Check_Type(opcode, T_FIXNUM);
	cbs->cb.aio_lio_opcode = NUM2INT(opcode);
	return opcode;
}

static VALUE
control_block_validate(VALUE cb)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	if (cbs->cb.aio_fildes <= 0) rb_aio_error( "Invalid file descriptor" );    
	if (cbs->cb.aio_nbytes <= 0) rb_aio_error( "Invalid buffer length" );    
	if (cbs->cb.aio_offset < 0) rb_aio_error( "Invalid file offset" );    
	if (cbs->cb.aio_reqprio < 0) rb_aio_error( "Invalid request priority" );
	if (cbs->cb.aio_lio_opcode != LIO_READ && cbs->cb.aio_lio_opcode != LIO_WRITE) rb_aio_error("Only AIO::READ and AIO::WRITE modes supported");
	if (!cbs->cb.aio_buf) rb_aio_error( "No buffer allocated" );	
	return cb;    
}

static VALUE
control_block_open_p(VALUE cb)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	return NIL_P(cbs->io) ? Qfalse : Qtrue;
}

static VALUE
control_block_close(VALUE cb)
{
 	rb_aiocb_t *cbs = GetCBStruct(cb);
	if NIL_P(cbs->io){
		return Qfalse;
    }else{
		rb_io_close(cbs->io);
		cbs->io = Qnil; 
		return Qtrue;
	} 	
}

/*
 *  Error handling for aio_read
 */
static void 
rb_aio_read_error(){
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
static VALUE 
rb_aio_read( aiocb_t *cb ){
	int ret;
	
	TRAP_BEG;
    ret = aio_read( cb );
	TRAP_END;
	if (ret != 0) rb_aio_read_error();
	while ( aio_error( cb ) == EINPROGRESS );
	if ((ret = aio_return( cb )) > 0) {
		return rb_tainted_str_new( (char *)cb->aio_buf, cb->aio_nbytes );
	}else{
		return INT2NUM(errno);
	}	
}

/*
 *  Error handling for lio_listio
 */
static void 
rb_aio_read_multi_error(){
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
static VALUE 
rb_aio_lio_listio( VALUE *cbs ){
	int op, ret;
	aiocb_t *list[AIO_MAX_LIST];
	int reads = RARRAY_LEN(cbs);
	
    bzero( (char *)list, sizeof(list) );	
    VALUE results = rb_ary_new2( reads );
    for (op=0; op < reads; op++) {		
	    rb_aiocb_t *cb = GetCBStruct(RARRAY_PTR(cbs)[op]);
		list[op] = &cb->cb;       
    }
	TRAP_BEG;
    ret = lio_listio( LIO_WAIT, &list, reads, NULL );
	TRAP_END;
	if (ret != 0) rb_aio_read_multi_error();
    for (op=0; op < reads; op++) {
		rb_ary_push( results, rb_tainted_str_new( (char *)list[op]->aio_buf, list[op]->aio_nbytes ) );
    } 
	return results;
}

/*
 *  Helper to ensure files opened via AIO.lio_listio is closed.
 */
static void 
rb_io_closes( VALUE cbs ){
    int io;
    for (io=0; io < RARRAY_LEN(cbs); io++) {
	  control_block_close( RARRAY_PTR(cbs)[io] );
    }  
}

/*
 *  call-seq:
 *     AIO.read(cb) -> string
 *  
 *  Asynchronously reads a file.This is an initial *blocking* implementation until
 *  cross platform notification is supported.
 */
static VALUE 
rb_aio_s_read( VALUE aio, VALUE cb ){
    rb_aiocb_t *cbs = GetCBStruct(cb);
    return rb_ensure( rb_aio_read, (VALUE)&cbs->cb, control_block_close, cb );
}

/*
 *  call-seq:
 *     AIO.lio_listio(cb1, cb2, ...) -> array
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
static VALUE 
rb_aio_s_lio_listio( VALUE aio, VALUE cbs ){
	int reads = RARRAY_LEN(cbs);
	if (reads > AIO_MAX_LIST) rb_aio_error( "maximum number of AIO calls exceeded!" );
	return rb_ensure( rb_aio_lio_listio, (VALUE)cbs, rb_io_closes, cbs );
}

void Init_aio()
{	
    mAio = rb_define_module("AIO");

	rb_cCB  = rb_define_class_under( mAio, "CB", rb_cObject);
    rb_define_alloc_func(rb_cCB, control_block_alloc);
    rb_define_method(rb_cCB, "initialize", control_block_initialize, -1);
    rb_define_method(rb_cCB, "fildes", control_block_fildes_get, 0);
    rb_define_method(rb_cCB, "fildes=", control_block_fildes_set, 1);
    rb_define_method(rb_cCB, "buf", control_block_buf_get, 0);
    rb_define_method(rb_cCB, "nbytes", control_block_nbytes_get, 0);
    rb_define_method(rb_cCB, "nbytes=", control_block_nbytes_set, 1);
    rb_define_method(rb_cCB, "offset", control_block_offset_get, 0);
    rb_define_method(rb_cCB, "offset=", control_block_offset_set, 1);
    rb_define_method(rb_cCB, "reqprio", control_block_reqprio_get, 0);
    rb_define_method(rb_cCB, "reqprio=", control_block_reqprio_set, 1);
    rb_define_method(rb_cCB, "lio_opcode", control_block_lio_opcode_get, 0);
    rb_define_method(rb_cCB, "lio_opcode=", control_block_lio_opcode_set, 1);
    rb_define_method(rb_cCB, "validate!", control_block_validate, 0);
    rb_define_method(rb_cCB, "reset!", control_block_reset, 0);
    rb_define_method(rb_cCB, "open", control_block_open, 1);
    rb_define_method(rb_cCB, "open?", control_block_open_p, 0);
    rb_define_method(rb_cCB, "close!", control_block_close, 0);
    rb_define_method(rb_cCB, "path", control_block_path, 0);

    rb_define_const(mAio, "ALLDONE", INT2NUM(AIO_ALLDONE));
    rb_define_const(mAio, "CANCELED", INT2NUM(AIO_CANCELED));
    rb_define_const(mAio, "NOTCANCELED", INT2NUM(AIO_NOTCANCELED));
    rb_define_const(mAio, "WAIT", INT2NUM(LIO_WAIT));
    rb_define_const(mAio, "NOWAIT", INT2NUM(LIO_NOWAIT));
    rb_define_const(mAio, "NOP", INT2NUM(LIO_NOP));
    rb_define_const(mAio, "READ", INT2NUM(LIO_READ));
    rb_define_const(mAio, "WRITE", INT2NUM(LIO_WRITE));

    eAio = rb_define_class_under(mAio, "Error", rb_eStandardError);

    rb_define_module_function( mAio, "lio_listio", rb_aio_s_lio_listio, -2 );
    rb_define_module_function( mAio, "read", rb_aio_s_read, 1 );

}