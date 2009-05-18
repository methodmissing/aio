require 'mkmf'

dir_config('aio')

have_func('aio_read', 'aio.h')

$CFLAGS << ' -lrt'

create_makefile('aio')