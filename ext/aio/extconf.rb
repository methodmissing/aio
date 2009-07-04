require 'mkmf'

def add_define(name)
  $defs.push("-D#{name}")
end

dir_config('aio')

have_func('aio_read', 'aio.h')

if have_func('rb_thread_blocking_region') and have_macro('RUBY_UBF_IO', 'ruby.h')
  $CFLAGS += " -DHAVE_TBR "
  $CPPFLAGS << " -DHAVE_TBR "
end

add_define 'RUBY18' if have_var('rb_trap_immediate', ['ruby.h', 'rubysig.h'])

$CFLAGS << ' -lrt -laio'

create_makefile('aio')