$:.unshift "."
require File.dirname(__FILE__) + '/../ext/aio/aio'
require "benchmark"
require "fileutils"

PAYLOAD = 'a' * 4096
CALLBACKS = (1..8).to_a.map{|f| c = AIO::CB.new; c.open(File.dirname(__FILE__) + "/../test/scratch/#{f}.aio.txt", 'w+' ); c.buf = PAYLOAD; c.lio_opcode = AIO::WRITE; c }
NON_BLOCKING = [AIO::NOWAIT].concat( CALLBACKS )
IOS = CALLBACKS.map{|cb| File.new(cb.path.gsub(/\.aio\.txt/,'.io.txt'), 'w+') }

aio_results, io_results = [], []

begin
  puts "* Bench writes ..."  
  Benchmark.bmbm do |results|
    results.report("AIO.lio_listio(AIO::WAIT)") { aio_results << AIO.lio_listio( *CALLBACKS ) }  
    results.report("IO.write") { io_results << IOS.map{|io| io.write(PAYLOAD) } }  
  end

  Benchmark.bmbm do |results|
    results.report("AIO.lio_listio(AIO::NOWAIT)") { aio_results << AIO.lio_listio( *NON_BLOCKING ) }  
    results.report("IO.write") { io_results << IOS.map{|io| io.write(PAYLOAD) } }  
  end
ensure
  CALLBACKS.each{|cb| cb.close }
  IOS.each{|io| io.close }
  FileUtils.rm Dir.glob(File.dirname(__FILE__) + "/../test/scratch/*.txt")
  puts "* AIO results ..." 
  aio_results.each{|r| p r }
  puts "* IO results ..."
  io_results.each{|r| p r }  
end
