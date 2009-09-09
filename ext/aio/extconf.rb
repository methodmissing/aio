require 'mkmf'

def add_define(name)
  $defs.push("-D#{name}")
end

dir_config('aio')

if RUBY_PLATFORM =~ /linux/i
  raise 'cannot find AIO' unless have_library('rt', 'aio_read', 'aio.h')
end
add_define 'RUBY19' if have_func('rb_thread_blocking_region') and have_macro('RUBY_UBF_IO', 'ruby.h')
add_define 'RUBY18' if have_var('rb_trap_immediate', ['ruby.h', 'rubysig.h'])

$defs.push("-pedantic")

create_makefile('aio')