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

#ifdef RUBY19
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
    int bufsize;
    int err;
    VALUE io; 
    VALUE rcb;
} rb_aiocb_t;

static struct sigevent rb_aio_sig_event;

static ID s_to_str, s_to_s, s_buf;

static VALUE c_aio_sync, c_aio_queue, c_aio_inprogress, c_aio_alldone;
static VALUE c_aio_canceled, c_aio_notcanceled, c_aio_wait, c_aio_nowait;
static VALUE c_aio_nop, c_aio_read, c_aio_write;

static void
inspect_aiocb(const char* func,aiocb_t *cb)
{    
    printf("\n%s:\naio_fildes = %d\naio_buf = %s (%d)\naio_nbytes = %d\naio_offset = %d\naio_reqprio = %d\naio_lio_opcode = %d\n",
           func, cb->aio_fildes, cb->aio_buf, sizeof(cb->aio_buf), cb->aio_nbytes, cb->aio_offset, cb->aio_reqprio, cb->aio_lio_opcode);
}

void 
rb_aio_completion_handler(int signo, siginfo_t *info, void *context) 
{
}

void
setup_rb_aio_signals() {
    struct sigaction sig_act;
    memset(&sig_act, 0, sizeof(sig_act));
    sigemptyset(&sig_act.sa_mask);
    sig_act.sa_sigaction = rb_aio_completion_handler;
    sig_act.sa_flags = SA_SIGINFO;
    sigaction(SIGIO, &sig_act, NULL);

    rb_aio_sig_event.sigev_notify = SIGEV_SIGNAL;
    rb_aio_sig_event.sigev_signo = SIGIO;
}

static void 
rb_aio_error(const char * msg)
{
    rb_raise( eAio, msg );
}

static void
setup_aio_buffer(rb_aiocb_t *cbs)
{
    if (!cbs->cb.aio_buf){ 
      cbs->cb.aio_buf = malloc(cbs->bufsize);
    }else{ 
      if (cbs->bufsize != cbs->cb.aio_nbytes){
        cbs->bufsize = cbs->cb.aio_nbytes + 1;
        cbs->cb.aio_buf = realloc((void*)cbs->cb.aio_buf,cbs->bufsize);
      }
    }
    if (!cbs->cb.aio_buf) rb_aio_error("Not able to allocate / resize the AIO buffer");
}

#define GetCBStruct(obj)	(Check_Type(obj, T_DATA), (rb_aiocb_t*)DATA_PTR(obj))

static void 
mark_control_block(rb_aiocb_t *cb)
{
    rb_gc_mark(cb->io);
    rb_gc_mark(cb->rcb);
}

static void 
free_control_block(rb_aiocb_t* cb)
{
    xfree(cb);
}

static VALUE
control_block_nbytes_set(VALUE cb, VALUE bytes)
{
    rb_aiocb_t *cbs = GetCBStruct(cb);
    Check_Type(bytes, T_FIXNUM);
    cbs->cb.aio_nbytes = FIX2INT(bytes);
    setup_aio_buffer(cbs);
    return bytes;
}

static VALUE
control_block_open(int argc, VALUE *argv, VALUE cb)
{
    const char *fmode;
#ifdef RUBY19
    rb_io_t *fptr;
#else	
    OpenFile *fptr;
#endif
    VALUE file, mode;
    rb_aiocb_t *cbs = GetCBStruct(cb);
    rb_scan_args(argc, argv, "02", &file, &mode);
    fmode = NIL_P(mode) ? "r" : RSTRING_PTR(mode);
    struct stat stats;
   
    Check_Type(file, T_STRING);

    cbs->io = rb_file_open(RSTRING_PTR(file), fmode);
    GetOpenFile(cbs->io, fptr);
    rb_io_check_readable(fptr);

    if ( cbs->cb.aio_fildes == 0 && cbs->cb.aio_nbytes == 0){
#ifdef RUBY19
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
control_block_reset0(rb_aiocb_t *cbs)
{    
    bzero((char *)cbs, sizeof(rb_aiocb_t));
    bzero((char *)&cbs->cb, sizeof(aiocb_t));
    /* cleanup with rb_io_close(cb->io) */
    cbs->io = Qnil;
    cbs->rcb = Qnil;
    cbs->bufsize = 1;
    cbs->err = 0;
    cbs->cb.aio_fildes = 0; 
    cbs->cb.aio_buf = NULL; 
    cbs->cb.aio_nbytes = 0;
    cbs->cb.aio_offset = 0;
    cbs->cb.aio_reqprio = 0;
    cbs->cb.aio_lio_opcode = LIO_READ;
    cbs->cb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    cbs->cb.aio_sigevent.sigev_signo = SIGIO;
    cbs->cb.aio_sigevent.sigev_value.sival_int = 12;
    setup_aio_buffer(cbs);
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
    rb_aiocb_t *cbs;
    obj = Data_Make_Struct(klass, rb_aiocb_t, mark_control_block, free_control_block, cbs);
    control_block_reset0(cbs);
    return obj;
}

static VALUE
control_block_initialize(int argc, VALUE *argv, VALUE cb)
{
    VALUE file, mode;
    VALUE args[2];
    rb_scan_args(argc, argv, "02", &file, &mode);
    if (RTEST(file)){ 
      args[0] = file;
      args[1] = mode;
      control_block_open(1, (VALUE *)args, cb);
    }
    if (rb_block_given_p()) rb_obj_instance_eval( 0, 0, cb );
    return cb;
}

static VALUE 
control_block_path(VALUE cb)
{ 
    rb_aiocb_t *cbs = GetCBStruct(cb);
#ifdef RUBY19
    rb_io_t *fptr;
#else	
    OpenFile *fptr;
#endif
    if NIL_P(cbs->io) return rb_tainted_str_new2("");
    GetOpenFile(cbs->io, fptr);
    rb_io_check_readable(fptr);
#ifdef RUBY19
    return rb_file_s_expand_path( 1, &fptr->pathv );
#else
    VALUE path = rb_tainted_str_new2(fptr->path);
    return rb_file_s_expand_path( 1, &path );
#endif
}

static VALUE
control_block_callback_get(VALUE cb)
{
    rb_aiocb_t *cbs = GetCBStruct(cb);
    return cbs->rcb;
}

static VALUE
control_block_callback_set(VALUE cb, VALUE rcb)
{
    rb_aiocb_t *cbs = GetCBStruct(cb);
    if (RBASIC(rcb)->klass != rb_cProc) rb_aio_error("Block required for callback!");
    cbs->rcb = rcb;
    return cbs->rcb;
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
    return cbs->cb.aio_buf == NULL ? rb_tainted_str_new2("") : rb_tainted_str_new((char *)cbs->cb.aio_buf, cbs->cb.aio_nbytes);
}

static VALUE
control_block_buf_set(VALUE cb, VALUE buf)
{
    rb_aiocb_t *cbs = GetCBStruct(cb);
    Check_Type(buf, T_STRING);
    cbs->cb.aio_nbytes = RSTRING_LEN(buf);
    setup_aio_buffer(cbs);
    cbs->cb.aio_buf = RSTRING_PTR(buf);
    return buf;
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
    if (opcode != c_aio_read && opcode != c_aio_write) rb_aio_error("Only AIO::READ and AIO::WRITE modes supported");
    cbs->cb.aio_lio_opcode = NUM2INT(opcode);
    return opcode;
}

static VALUE
control_block_validate(VALUE cb)
{
    rb_aiocb_t *cbs = GetCBStruct(cb);
    if (cbs->cb.aio_fildes <= 0) rb_aio_error("Invalid file descriptor");    
    if (cbs->cb.aio_nbytes <= 0) rb_aio_error("Invalid buffer length");    
    if (cbs->cb.aio_offset < 0) rb_aio_error("Invalid file offset");    
    if (cbs->cb.aio_reqprio < 0) rb_aio_error("Invalid request priority");
    if (cbs->cb.aio_lio_opcode != LIO_READ && cbs->cb.aio_lio_opcode != LIO_WRITE) rb_aio_error("Only AIO::READ and AIO::WRITE modes supported");
    if (!cbs->cb.aio_buf) rb_aio_error("No AIO buffer allocated");	
    return cb;    
}

static VALUE
control_block_open_p(VALUE cb)
{
    rb_aiocb_t *cbs = GetCBStruct(cb);
    return NIL_P(cbs->io) ? Qfalse : Qtrue;
}

static VALUE
control_block_closed_p(VALUE cb)
{
    rb_aiocb_t *cbs = GetCBStruct(cb);
    return NIL_P(cbs->io) ? Qtrue : Qfalse;
}

static VALUE
control_block_close(VALUE cb)
{
    rb_aiocb_t *cbs = GetCBStruct(cb);
    if NIL_P(cbs->io) return Qfalse;
    aio_return(&cbs->cb);
    rb_io_close(cbs->io);
    cbs->io = Qnil; 
    cbs->rcb = Qnil;
    return Qtrue;
}

/*
 *  Error handling for aio_write
 */
static void 
rb_aio_write_error()
{
    switch(errno){
       case EAGAIN: 
            rb_aio_error("[EAGAIN] The request cannot be queued due to exceeding resource (queue) limitations.");
       case EBADF: 
            rb_aio_error("[EBADF] File descriptor is not valid for writing.");
       case ENOSYS: 
            rb_aio_error("[ENOSYS] aio_read not supported by this implementation.");
       case EINVAL: 
            rb_aio_error("[EINVAL] Read offset is invalid");
       case EOVERFLOW: 
            rb_aio_error("[EOVERFLOW] Control block offset exceeded.");
       case ECANCELED:
            rb_aio_error("[ECANCELED] The requested I/O was canceled by an explicit aio_cancel() request.");
       case EFBIG:
            rb_aio_error("[EFBIG] Wrong offset.");
    }
}

/*
 *  Error handling for aio_read
 */
static void 
rb_aio_read_error()
{
    switch(errno){
       case EAGAIN: 
            rb_aio_error("[EAGAIN] The request cannot be queued due to exceeding resource (queue) limitations.");
       case EBADF: 
            rb_aio_error("[EBADF] File descriptor is not valid for reading.");
       case ENOSYS: 
            rb_aio_error("[ENOSYS] aio_read not supported by this implementation.");
       case EINVAL: 
            rb_aio_error("[EINVAL] Read offset is invalid");
       case EOVERFLOW: 
            rb_aio_error("[EOVERFLOW] Control block offset exceeded.");
    }
}

/*
 *  Initiates a *blocking* write
 */
static VALUE 
rb_aio_write(aiocb_t *cb)
{
    int ret;
    
    TRAP_BEG;
    ret = aio_write(cb);
    TRAP_END;
    if (ret != 0) rb_aio_write_error();
    while ( aio_error(cb) == EINPROGRESS );
    if ((ret = aio_return(cb)) > 0) {
     return INT2NUM(cb->aio_nbytes);
    }else{
      return INT2NUM(errno);
    }
}

/*
 *  Initiates a *blocking* read
 */
static VALUE 
rb_aio_read(aiocb_t *cb)
{
    int ret;    
    TRAP_BEG;
    ret = aio_read(cb);
    TRAP_END;
    if (ret != 0) rb_aio_read_error();
    while ( aio_error(cb) == EINPROGRESS );
    if ((ret = aio_return(cb)) > 0) {
      return rb_tainted_str_new( (char *)cb->aio_buf, cb->aio_nbytes );
    }else{
      return INT2NUM(errno);
    }
}

/*
 *  Error handling for lio_listio
 */
static void 
rb_aio_listio_error()
{
    switch(errno){
       case EAGAIN: 
            rb_aio_error("[EAGAIN] Resources necessary to queue all the requests are not available at the moment.");
       case EIO: 
            rb_aio_error("[EIO] One or more requests failed");
       case ENOSYS: 
            rb_aio_error("[ENOSYS] lio_listio not supported by this implementation.");
       case EINVAL: 
            rb_aio_error("[EINVAL] Maximum number of allowed simultaneous requests exceeded.");
       case EINTR: 
            rb_aio_error("[EINTR] A signal was delivered while waiting for all I/O requests to complete during a LIO_WAIT operation.");
    }
}

static void
rb_aio_lio_listio0(int mode, VALUE *cbs, aiocb_t **list, int ops)
{
    int op;
    bzero((char *)list, sizeof(list));
    for (op=0; op < ops; op++) {
        rb_aiocb_t *cb = GetCBStruct(RARRAY_PTR(cbs)[op]);
        setup_aio_buffer(cb);
        if (rb_block_given_p()){
          cb->rcb = rb_block_proc();
        } 
      list[op] = &cb->cb;
    }
}

/*
 *  Initiates lio_listio
 */
static int 
rb_aio_lio_listio(int mode, VALUE *cbs, aiocb_t **list)
{
    int ret;
    int ops = RARRAY_LEN(cbs);
    rb_aio_lio_listio0(mode, cbs, list, ops);

    TRAP_BEG;
    /* &rb_aio_sig_event */
    ret = lio_listio(mode, list, ops, NULL);
    TRAP_END;
    /* Darwin triggers an error to clean it's work q */
    if (ret != 0 && mode != LIO_NOP) rb_aio_listio_error();
    return ops; 
}

/*
 *  Blocking lio_listio
 */
static VALUE
rb_aio_lio_listio_blocking(VALUE *cbs)
{
    aiocb_t *list[AIO_MAX_LIST];
    int op;
    int ops = rb_aio_lio_listio( LIO_WAIT, cbs, list );
    VALUE results = rb_ary_new2( ops );
    for (op=0; op < ops; op++) {
      if (list[op]->aio_lio_opcode == LIO_READ){ 
         rb_ary_push( results, rb_tainted_str_new( (char *)list[op]->aio_buf, list[op]->aio_nbytes ) );
      }else{
         rb_ary_push( results, INT2FIX(list[op]->aio_nbytes) );
      }
    } 
    return results;
}

/*
 *  No-op lio_listio
 */
static VALUE
rb_aio_lio_listio_noop(VALUE *cbs)
{
    aiocb_t *list[AIO_MAX_LIST];
    rb_aio_lio_listio(LIO_NOP, cbs, list);
    return Qnil;
}

/*
 *  Non-blocking lio_listio
 */
static VALUE
rb_aio_lio_listio_non_blocking(VALUE *cbs)
{
    aiocb_t *list[AIO_MAX_LIST];
    rb_aio_lio_listio(LIO_NOWAIT, cbs, list);
    return Qnil;
}

/*
 *  Helper to ensure files opened via AIO.lio_listio is closed.
 */
static void 
rb_io_closes(VALUE cbs){
    int io;
    for (io=0; io < RARRAY_LEN(cbs); io++) {
      control_block_close(RARRAY_PTR(cbs)[io]);
    }  
}

/*
 *  call-seq:
 *     AIO.write(cb) -> fixnum
 *  
 *  Asynchronously writes to a file.This is an initial *blocking* implementation until
 *  cross platform notification is supported.
 */
static VALUE 
rb_aio_s_write(VALUE aio, VALUE cb)
{
    rb_aiocb_t *cbs = GetCBStruct(cb);
#ifdef RUBY19
    rb_io_t *fptr;
#else	
    OpenFile *fptr;
#endif
    GetOpenFile(cbs->io, fptr);
    rb_io_check_writable(fptr);    
    if (rb_block_given_p()){
      cbs->rcb = rb_block_proc();
    }
    
    return rb_ensure(rb_aio_write, (VALUE)&cbs->cb, control_block_close, cb);
}

/*
 *  call-seq:
 *     AIO.read(cb) -> string
 *  
 *  Asynchronously reads a file.This is an initial *blocking* implementation until
 *  cross platform notification is supported.
 */
static VALUE 
rb_aio_s_read(VALUE aio, VALUE cb)
{
    rb_aiocb_t *cbs = GetCBStruct(cb);
    if (rb_block_given_p()){
      cbs->rcb = rb_block_proc();
    }
    return rb_ensure(rb_aio_read, (VALUE)&cbs->cb, control_block_close, cb);
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
rb_aio_s_lio_listio(VALUE aio, VALUE cbs)
{
    VALUE mode_arg, mode;
    int ops;
    ops = RARRAY_LEN(cbs);
    mode_arg = RARRAY_PTR(cbs)[0];
    mode = (mode_arg == c_aio_wait || mode_arg == c_aio_nowait || mode_arg == c_aio_nop) ? rb_ary_shift(cbs) : c_aio_wait;
    if (ops > AIO_MAX_LIST) return c_aio_queue;
    switch(NUM2INT(mode)){
        case LIO_WAIT:
             return rb_ensure(rb_aio_lio_listio_blocking, (VALUE)cbs, rb_io_closes, (VALUE)cbs);   
        case LIO_NOWAIT:
             return rb_aio_lio_listio_non_blocking((VALUE)cbs);
        case LIO_NOP:
             return rb_ensure(rb_aio_lio_listio_noop, (VALUE)cbs, rb_io_closes, (VALUE)cbs);
    }
    rb_aio_error("Only modes AIO::WAIT, AIO::NOWAIT and AIO::NOP supported");
}

/*
 *  Error handling for aio_cancel
 */
static void 
rb_aio_cancel_error()
{
    switch(errno){
       case EBADF: 
            rb_aio_error("[EBADF] Invalid file descriptor.");
       case ENOSYS: 
            rb_aio_error("[ENOSYS] aio_cancel is not supported by this implementation.");
    }
}

static VALUE 
rb_aio_cancel(int fd, void *cb)
{   
    int ret;
    TRAP_BEG;
    ret = aio_cancel( fd, cb );
    TRAP_END;
    if (ret != 0) rb_aio_cancel_error();
    switch(ret){
       case AIO_CANCELED: 
            return c_aio_canceled;
       case AIO_NOTCANCELED: 
            return c_aio_notcanceled;
       case AIO_ALLDONE: 
            return c_aio_alldone;
   }
   return Qnil;
}

static VALUE 
rb_aio_s_cancel(int argc, VALUE *argv, VALUE aio)
{
    VALUE fd, cb;
    rb_scan_args(argc, argv, "02", &fd, &cb);
    if (NIL_P(fd) || !FIXNUM_P(fd)) rb_aio_error("No file descriptor given");
    if (NIL_P(cb)) return rb_aio_cancel( NUM2INT(fd), NULL );
    rb_aiocb_t *cbs = GetCBStruct(cb);
    if (rb_block_given_p()){
      cbs->rcb = rb_block_proc();
    }	
    return rb_aio_cancel( NUM2INT(fd), &cbs->cb );	
}

/*
 *  Error handling for aio_return
 */
static void 
rb_aio_return_error()
{
    switch(errno){
       case ENOMEM: 
            rb_aio_error("[ENOMEM] There were no Kernel control blocks available to service this request.");
       case EINVAL: 
            rb_aio_error("[EINVAL] The control block does not refer to an asynchronous operation whose return status has not yet been retrieved.");
       case ENOSYS: 
            rb_aio_error("[ENOSYS] aio_return not supported by this implementation.");
    }
}

static VALUE 
rb_aio_return(aiocb_t *cb)
{ 
    int ret;
    TRAP_BEG;
    ret = aio_return( cb );
    TRAP_END;
    if (ret != 0) rb_aio_return_error();
    return INT2FIX(ret);
}

static VALUE 
rb_aio_s_return(VALUE aio, VALUE cb)
{
    rb_aiocb_t *cbs = GetCBStruct(cb);
    if (rb_block_given_p()){
      cbs->rcb = rb_block_proc();
    }
    return rb_aio_return( &cbs->cb );	
}

/*
 *  Error handling for aio_error
 */
static void 
rb_aio_err_error()
{
    switch(errno){
       case EINVAL: 
            rb_aio_error("[EINVAL] The control block does not refer to an asynchronous operation whose return status has not yet been retrieved.");
       case ENOSYS: 
            rb_aio_error("[ENOSYS] aio_error not supported by this implementation.");
    }
}

static VALUE 
rb_aio_err(aiocb_t *cb)
{ 
    int ret;
    TRAP_BEG;
    ret = aio_error(cb);
    TRAP_END;
    if (ret != 0) rb_aio_err_error();
    return INT2FIX(ret);
}

static VALUE 
rb_aio_s_error(VALUE aio, VALUE cb)
{
    rb_aiocb_t *cbs = GetCBStruct(cb);
    if (rb_block_given_p()){
      cbs->rcb = rb_block_proc();
    }
    return rb_aio_err( &cbs->cb );
}

/*
 *  Error handling for aio_error
 */
static void 
rb_aio_sync_error()
{
    switch(errno){
       case EINVAL: 
            rb_aio_error("[EINVAL] A value of op other than AIO::DSYNC or AIO::SYNC was specified.");
       case EAGAIN: 
            rb_aio_error("[EAGAIN] The requested asynchronous operation was not queued due to temporary resource limitations.");
       case ENOSYS: 
            rb_aio_error("[ENOSYS] aio_error not supported by this implementation.");
       case EBADF: 
            rb_aio_error("[EBADF] Invalid file descriptor.");
    }
}

static VALUE 
rb_aio_sync(int op, aiocb_t *cb)
{ 
    int ret;
    TRAP_BEG;
    ret = aio_fsync( op, cb );
    TRAP_END;
    if (ret != 0) rb_aio_sync_error();
    return INT2FIX(ret);
}

static VALUE 
rb_aio_s_sync(VALUE aio, VALUE op, VALUE cb)
{
    rb_aiocb_t *cbs = GetCBStruct(cb);
    if (rb_block_given_p()){
      cbs->rcb = rb_block_proc();
    }
    Check_Type( op, T_FIXNUM );
    /* XXX handle AIO::DSYNC gracefully as well */
    if (op != c_aio_sync) rb_aio_error("Operation AIO::SYNC expected");
    return rb_aio_sync( NUM2INT(op), &cbs->cb );	
}

void Init_aio()
{   
    s_buf = rb_intern("buf");
    s_to_str = rb_intern("to_str");
    s_to_s = rb_intern("to_s");
   
    mAio = rb_define_module("AIO");

    rb_cCB  = rb_define_class_under( mAio, "CB", rb_cObject);
    rb_define_alloc_func(rb_cCB, control_block_alloc);
    rb_define_method(rb_cCB, "initialize", control_block_initialize, -1);
    rb_define_method(rb_cCB, "fildes", control_block_fildes_get, 0);
    rb_define_method(rb_cCB, "fildes=", control_block_fildes_set, 1);
    rb_define_method(rb_cCB, "buf", control_block_buf_get, 0);
    rb_define_method(rb_cCB, "buf=", control_block_buf_set, 1);
    rb_define_method(rb_cCB, "nbytes", control_block_nbytes_get, 0);
    rb_define_method(rb_cCB, "nbytes=", control_block_nbytes_set, 1);
    rb_define_method(rb_cCB, "offset", control_block_offset_get, 0);
    rb_define_method(rb_cCB, "offset=", control_block_offset_set, 1);
    rb_define_method(rb_cCB, "reqprio", control_block_reqprio_get, 0);
    rb_define_method(rb_cCB, "reqprio=", control_block_reqprio_set, 1);
    rb_define_method(rb_cCB, "lio_opcode", control_block_lio_opcode_get, 0);
    rb_define_method(rb_cCB, "lio_opcode=", control_block_lio_opcode_set, 1);
    rb_define_method(rb_cCB, "callback", control_block_callback_get, 0);
    rb_define_method(rb_cCB, "callback=", control_block_callback_set, 1);
    rb_define_method(rb_cCB, "validate", control_block_validate, 0);
    rb_define_method(rb_cCB, "reset", control_block_reset, 0);
    rb_define_method(rb_cCB, "open", control_block_open, -1);
    rb_define_method(rb_cCB, "open?", control_block_open_p, 0);
    rb_define_method(rb_cCB, "close", control_block_close, 0);
    rb_define_method(rb_cCB, "closed?", control_block_closed_p, 0);
    rb_define_method(rb_cCB, "path", control_block_path, 0);
 
    rb_alias( rb_cCB, s_to_str, s_buf );
    rb_alias( rb_cCB, s_to_s, s_buf );

    rb_define_const(mAio, "SYNC", INT2NUM(O_SYNC));
    /*
    XXX O_DSYNC not supported by Darwin
    rb_define_const(mAio, "DSYNC", INT2NUM(O_DSYNC));*/
    rb_define_const(mAio, "QUEUE", INT2NUM(100));
    rb_define_const(mAio, "INPROGRESS", INT2NUM(EINPROGRESS));
    rb_define_const(mAio, "ALLDONE", INT2NUM(AIO_ALLDONE));
    rb_define_const(mAio, "CANCELED", INT2NUM(AIO_CANCELED));
    rb_define_const(mAio, "NOTCANCELED", INT2NUM(AIO_NOTCANCELED));
    rb_define_const(mAio, "WAIT", INT2NUM(LIO_WAIT));
    rb_define_const(mAio, "NOWAIT", INT2NUM(LIO_NOWAIT));
    rb_define_const(mAio, "NOP", INT2NUM(LIO_NOP));
    rb_define_const(mAio, "READ", INT2NUM(LIO_READ));
    rb_define_const(mAio, "WRITE", INT2NUM(LIO_WRITE));

    c_aio_sync = INT2NUM(O_SYNC);
    c_aio_queue = INT2NUM(100);
    c_aio_inprogress = INT2NUM(EINPROGRESS);
    c_aio_alldone = INT2NUM(AIO_ALLDONE);
    c_aio_canceled = INT2NUM(AIO_CANCELED);
    c_aio_notcanceled = INT2NUM(AIO_NOTCANCELED);
    c_aio_wait = INT2NUM(LIO_WAIT);
    c_aio_nowait = INT2NUM(LIO_NOWAIT);
    c_aio_nop = INT2NUM(LIO_NOP);
    c_aio_read = INT2NUM(LIO_READ);
    c_aio_write = INT2NUM(LIO_WRITE);

    eAio = rb_define_class_under(mAio, "Error", rb_eStandardError);

    rb_define_module_function( mAio, "lio_listio", rb_aio_s_lio_listio, -2 );
    rb_define_module_function( mAio, "read", rb_aio_s_read, 1 );
    rb_define_module_function( mAio, "write", rb_aio_s_write, 1 );
    rb_define_module_function( mAio, "cancel", rb_aio_s_cancel, -1 );
    rb_define_module_function( mAio, "return", rb_aio_s_return, 1 );
    rb_define_module_function( mAio, "error", rb_aio_s_error, 1 );
    rb_define_module_function( mAio, "sync", rb_aio_s_sync, 2 );

    setup_rb_aio_signals();
}