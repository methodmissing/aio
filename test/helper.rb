require 'test/unit'
require 'aio'

FIXTURES = File.dirname(__FILE__) + "/fixtures"
SCRATCH = File.dirname(__FILE__) + "/scratch"

def fixtures(*files)
  files.map{|f| File.join(FIXTURES,f) }
end

def fixture(file)
  file =~ /\// ? file : fixtures(*file).first
end

def scratch_space(*files)
  files.map{|f| File.join(SCRATCH,f) }
end

def scratch(file)
  file =~ /\// ? file : scratch_space(*file).first
end

def assert_aio_error(&block)
  assert_raises AIO::Error do
    block.call
  end  
end  

module Kernel
  private
  def CB(file = nil, mode = nil)
    aio_cb(file, mode, :fixture) 
  end
  
  def WCB(file = nil, mode = nil)
    aio_cb(file, mode, :scratch) 
  end  
  
  def aio_cb(file, mode, meth)
    cb = AIO::CB.new
    if file
      cb.open(__send__(meth,file),mode)
      cb
    else
      cb
    end
  end
end