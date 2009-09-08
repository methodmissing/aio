$:.unshift "."
require File.dirname(__FILE__) + '/helper'

class TestControlBlock < Test::Unit::TestCase
  
  def setup
    @cb = AIO::CB.new
  end

  def test_open
    @cb.open( fixture( '1.txt' ) ) 
    assert (2..10).include?( @cb.fildes )
    assert_equal 3, @cb.nbytes
    @cb.validate
  end  
  
  def test_open_access_mode
    assert_raises ArgumentError do
      @cb.open( fixture( '1.txt' ), "ww" )
    end
  end

  def test_path
    assert_equal '', @cb.path
    @cb.open( fixture( '1.txt' ) )
    assert_match /test\/fixtures\/1\.txt/, @cb.path    
  end

  def test_open?
    assert !@cb.open?
    @cb.open( fixture( '2.txt' ) )
    assert @cb.open?    
  end
  
  def test_close
    assert_equal false, @cb.close
    @cb.open( fixture( '2.txt' ) )
    assert @cb.open?    
    assert_equal true, @cb.close    
    assert !@cb.open?
  end

  def test_closed?
    @cb.open( fixture( '2.txt' ) )
    assert @cb.open?    
    @cb.close    
    assert @cb.closed?    
  end

  def test_init_with_file
    @cb = AIO::CB.new fixture( '2.txt' )
    assert @cb.open?
  end
  
  def test_init_with_block
    @cb = AIO::CB.new do 
      self.fildes = 12
    end  
    assert_equal 12, @cb.fildes
  end
  
  def test_file_descriptor
    assert_equal 0, @cb.fildes
    assert_equal 12, @cb.fildes = 12
    assert_raises TypeError do
      @cb.fildes = '12'
    end
  end

  def test_buffer_equals
    assert_equal '', @cb.buf
    assert_equal 0, @cb.nbytes
    @cb.buf = 'buffer'
    assert_equal 'buffer', @cb.to_s
    assert_equal 6, @cb.nbytes
  end

  def test_buffer
    assert_equal '', @cb.buf
    assert_equal '', @cb.to_str
    assert_equal '', @cb.to_s    
  end  

  def test_nbytes
    assert_equal 0, @cb.nbytes
    assert_equal 4096, @cb.nbytes = 4096
    assert_raises TypeError do
      @cb.nbytes = '4096'
    end
  end  

  def test_offset
    assert_equal 0, @cb.offset
    assert_equal 4096, @cb.offset = 4096
    assert_raises TypeError do
      @cb.offset = '4096'
    end
  end  
  
  def test_reqprio
    assert_equal 0, @cb.reqprio
    assert_equal 10, @cb.reqprio = 10
    assert_raises TypeError do
      @cb.reqprio = '10'
    end
  end  

  def test_lio_opcode
    assert_equal AIO::READ, @cb.lio_opcode
    assert_equal AIO::WRITE, @cb.lio_opcode = AIO::WRITE
    assert_aio_error do
      @cb.lio_opcode = 12
    end 
  end
  
  def test_reset
    @cb.offset = 4096
    @cb.lio_opcode = AIO::WRITE
    @cb.reset
    assert_equal 0, @cb.offset
    assert_equal AIO::READ, @cb.lio_opcode    
  end
    
  def test_validate
    assert_invalid{ @cb.fildes = 0 }
    assert_invalid{ @cb.offset = -1 }
    assert_invalid{ @cb.nbytes = 0 }
    assert_invalid{ @cb.nbytes = -1 }
    assert_invalid{ @cb.reqprio = -1 }
    assert_invalid{ @cb.lio_opcode = 12 }
    @cb = AIO::CB.new do 
      self.fildes = 1
      self.offset = 0
      self.nbytes = 4096
    end
    assert_equal @cb, @cb.validate      
  end
  def test_callback_equals
    assert_aio_error do
      @cb.callback = :invalid
    end
    assert_instance_of Proc, @cb.callback = Proc.new{|e| p e }
  end

  def teardown
    @cb.reset
  end

  private
  def assert_invalid( &block )
    assert_aio_error do
      block.call
      @cb.validate
      @cb.reset     
    end  
  end  
end