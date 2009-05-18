#!/usr/sbin/dtrace -ZFs 
#pragma D option quiet
#pragma D option dynvarsize=64m
#pragma D option bufsize=128m
#pragma D option switchrate=5
self int depth;

dtrace:::BEGIN{
  self->depth = 0;
}

pid$target::rb_aio_schedule_read_error:entry{
  printf("%3d %-16d %*s %-22s ->\n", cpu, timestamp / 1000, self->depth * 1, "", probefunc );
  self->depth++;
}

pid$target::rb_aio_schedule_read_error:return{
   self->depth--;
   printf("%3d %-16d %*s %-22s <-\n", cpu, timestamp / 1000, self->depth * 1, "", probefunc );	
}

dtrace:::END{
  exit(0);
}