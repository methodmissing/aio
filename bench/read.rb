require File.dirname(__FILE__) + '/../ext/aio/aio'
require "benchmark"

CALLBACKS = (1..8).to_a.map{|f| AIO::CB.new( File.dirname(__FILE__) + "/../test/fixtures/#{f}.txt" ) }
PATHS = CALLBACKS.map{|cb| cb.path }

Benchmark.bm do |results|
  results.report("AIO.lio_listio") { AIO.lio_listio( *CALLBACKS ) }  
  results.report("IO.read") { PATHS.each{|p| IO.read(p) } }  
end

=begin
macbook-pros-computer:rb_aio_native lourens$ ruby bench/read.rb
      user     system      total        real
AIO.lio_listio  0.000000   0.000000   0.000000 (  0.000266)
IO.read  0.000000   0.000000   0.000000 (  0.000378)

|===================================================================
| Syscall overheads ... note multiple reads VS single lio_listio
|===================================================================
open_nocancel("bench/../test/fixtures/1.txt\0", 0x0, 0x1B6)		 = 3 0
fstat(0x3, 0xBFFFC3C0, 0x1B6)		 = 0 0
open_nocancel("bench/../test/fixtures/2.txt\0", 0x0, 0x1B6)		 = 4 0
fstat(0x4, 0xBFFFC3C0, 0x1B6)		 = 0 0
open_nocancel("bench/../test/fixtures/3.txt\0", 0x0, 0x1B6)		 = 5 0
fstat(0x5, 0xBFFFC3C0, 0x1B6)		 = 0 0
open_nocancel("bench/../test/fixtures/4.txt\0", 0x0, 0x1B6)		 = 6 0
fstat(0x6, 0xBFFFC3C0, 0x1B6)		 = 0 0
open_nocancel("bench/../test/fixtures/5.txt\0", 0x0, 0x1B6)		 = 7 0
fstat(0x7, 0xBFFFC3C0, 0x1B6)		 = 0 0
open_nocancel("bench/../test/fixtures/6.txt\0", 0x0, 0x1B6)		 = 8 0
fstat(0x8, 0xBFFFC3C0, 0x1B6)		 = 0 0
open_nocancel("bench/../test/fixtures/7.txt\0", 0x0, 0x1B6)		 = 9 0
fstat(0x9, 0xBFFFC3C0, 0x1B6)		 = 0 0
open_nocancel("bench/../test/fixtures/8.txt\0", 0x0, 0x1B6)		 = 10 0
fstat(0xA, 0xBFFFC3C0, 0x1B6)		 = 0 0
lio_listio(0x2, 0xBFFFC770, 0x8)		 = 0 0
close_nocancel(0x3)		 = 0 0
close_nocancel(0x4)		 = 0 0
close_nocancel(0x5)		 = 0 0
close_nocancel(0x6)		 = 0 0
close_nocancel(0x7)		 = 0 0
close_nocancel(0x8)		 = 0 0
close_nocancel(0x9)		 = 0 0
close_nocancel(0xA)		 = 0 0
getrusage(0x0, 0xBFFFC1C0, 0x8)		 = 0 0
getrusage(0xFFFFFFFF, 0xBFFFC1C0, 0x8)		 = 0 0
write(0x1, "  0.000000   0.000000   0.000000 (  0.000605)\n\0", 0x2E)		 = 46 0
getrusage(0x0, 0xBFFFC1C0, 0x7)		 = 0 0
open_nocancel("bench/../test/fixtures/1.txt\0", 0x0, 0x1B6)		 = 3 0
fstat(0x3, 0xBFFFBCF0, 0x1B6)		 = 0 0
lseek(0x3, 0x0, 0x1)		 = 0 0
fstat64(0x3, 0xBFFFBBD4, 0x1)		 = 0 0
read_nocancel(0x3, "one\300\003\0", 0x1000)		 = 3 0
read_nocancel(0x3, "one\300\003\0", 0x1000)		 = 0 0
close_nocancel(0x3)		 = 0 0
open_nocancel("bench/../test/fixtures/2.txt\0", 0x0, 0x1B6)		 = 3 0
fstat(0x3, 0xBFFFBCF0, 0x1B6)		 = 0 0
lseek(0x3, 0x0, 0x1)		 = 0 0
fstat64(0x3, 0xBFFFBBD4, 0x1)		 = 0 0
read_nocancel(0x3, "two\300\003\0", 0x1000)		 = 3 0
read_nocancel(0x3, "two\300\003\0", 0x1000)		 = 0 0
close_nocancel(0x3)		 = 0 0
open_nocancel("bench/../test/fixtures/3.txt\0", 0x0, 0x1B6)		 = 3 0
fstat(0x3, 0xBFFFBCF0, 0x1B6)		 = 0 0
lseek(0x3, 0x0, 0x1)		 = 0 0
fstat64(0x3, 0xBFFFBBD4, 0x1)		 = 0 0
read_nocancel(0x3, "three\0", 0x1000)		 = 5 0
read_nocancel(0x3, "three\0", 0x1000)		 = 0 0
close_nocancel(0x3)		 = 0 0
open_nocancel("bench/../test/fixtures/4.txt\0", 0x0, 0x1B6)		 = 3 0
fstat(0x3, 0xBFFFBCF0, 0x1B6)		 = 0 0
lseek(0x3, 0x0, 0x1)		 = 0 0
fstat64(0x3, 0xBFFFBBD4, 0x1)		 = 0 0
read_nocancel(0x3, "foure\0", 0x1000)		 = 4 0
read_nocancel(0x3, "foure\0", 0x1000)		 = 0 0
close_nocancel(0x3)		 = 0 0
open_nocancel("bench/../test/fixtures/5.txt\0", 0x0, 0x1B6)		 = 3 0
fstat(0x3, 0xBFFFBCF0, 0x1B6)		 = 0 0
lseek(0x3, 0x0, 0x1)		 = 0 0
fstat64(0x3, 0xBFFFBBD4, 0x1)		 = 0 0
read_nocancel(0x3, "fivee\0", 0x1000)		 = 4 0
read_nocancel(0x3, "fivee\0", 0x1000)		 = 0 0
close_nocancel(0x3)		 = 0 0
open_nocancel("bench/../test/fixtures/6.txt\0", 0x0, 0x1B6)		 = 3 0
fstat(0x3, 0xBFFFBCF0, 0x1B6)		 = 0 0
lseek(0x3, 0x0, 0x1)		 = 0 0
fstat64(0x3, 0xBFFFBBD4, 0x1)		 = 0 0
read_nocancel(0x3, "sixee\0", 0x1000)		 = 3 0
read_nocancel(0x3, "sixee\0", 0x1000)		 = 0 0
close_nocancel(0x3)		 = 0 0
open_nocancel("bench/../test/fixtures/7.txt\0", 0x0, 0x1B6)		 = 3 0
fstat(0x3, 0xBFFFBCF0, 0x1B6)		 = 0 0
lseek(0x3, 0x0, 0x1)		 = 0 0
fstat64(0x3, 0xBFFFBBD4, 0x1)		 = 0 0
read_nocancel(0x3, "seven\0", 0x1000)		 = 5 0
read_nocancel(0x3, "seven\0", 0x1000)		 = 0 0
close_nocancel(0x3)		 = 0 0
open_nocancel("bench/../test/fixtures/8.txt\0", 0x0, 0x1B6)		 = 3 0
fstat(0x3, 0xBFFFBCF0, 0x1B6)		 = 0 0
lseek(0x3, 0x0, 0x1)		 = 0 0
fstat64(0x3, 0xBFFFBBD4, 0x1)		 = 0 0
read_nocancel(0x3, "eight\0", 0x1000)		 = 5 0
read_nocancel(0x3, "eight\0", 0x1000)		 = 0 0
close_nocancel(0x3)		 = 0 0
=end