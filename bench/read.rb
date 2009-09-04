$:.unshift "."
require File.dirname(__FILE__) + '/../ext/aio/aio'
require "benchmark"

CALLBACKS = (1..8).to_a.map{|f| AIO::CB.new( File.dirname(__FILE__) + "/../test/fixtures/#{f}.txt" ) }
NON_BLOCKING = [AIO::NOWAIT].concat( CALLBACKS )
PATHS = CALLBACKS.map{|cb| cb.path }

begin
  puts "* Bench reads ..."
  Benchmark.bmbm do |results|
    results.report("AIO.lio_listio(AIO::WAIT)") { AIO.lio_listio( *CALLBACKS ) }  
    results.report("IO.read") { PATHS.each{|p| IO.read(p) } }  
  end

  Benchmark.bmbm do |results|
    results.report("AIO.lio_listio(AIO::NOWAIT)") { AIO.lio_listio( *NON_BLOCKING ) }  
    results.report("IO.read") { PATHS.each{|p| IO.read(p) } }  
  end
ensure
  CALLBACKS.each{|cb| cb.close }  
end  