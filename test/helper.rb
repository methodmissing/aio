require 'test/unit'
require 'aio'

def fixtures( *files )
  files.map{|f| File.dirname(__FILE__) + "/fixtures/#{f}" }
end

def fixture(file)
  fixtures(*file).first
end

def assert_aio_error(&block)
  assert_raises AIO::Error do
    block.call
  end  
end  

module Kernel
  private
  def CB(file = nil)
    file ? AIO::CB.new(file) : AIO::CB.new
  end
end