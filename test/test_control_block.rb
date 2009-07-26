require File.dirname(__FILE__) + '/helper'

class TestControlBlock < Test::Unit::TestCase
  
  def setup
    @cb = AIO::CB.new
  end

  def test_open
    @cb.open( fixtures( '1.txt' ).first ) 
    assert (2..5).include?( @cb.fildes )
    assert_equal 3, @cb.nbytes
    assert_nothing_raised do
      @cb.validate!
    end
  end  

  def test_path
    assert_equal '', @cb.path
    @cb.open( fixtures( '1.txt' ).first )
    assert_match /test\/fixtures\/1\.txt/, @cb.path    
  end

  def test_open?
    assert !@cb.open?
    @cb.open( fixtures( '2.txt' ).first )
    assert @cb.open?    
  end
  
  def test_close!
    assert_equal false, @cb.close!
    @cb.open( fixtures( '2.txt' ).first )
    assert @cb.open?    
    assert_equal true, @cb.close!    
    assert !@cb.open?
  end

  def test_init_with_file
    cb = AIO::CB.new fixtures( '2.txt' ).first
    assert cb.open?
  end
  
  def test_init_with_block
    cb = AIO::CB.new do 
      self.fildes = 12
    end  
    assert_equal 12, cb.fildes
  end
  
  def test_file_descriptor
    assert_equal 0, @cb.fildes
    assert_equal 12, @cb.fildes = 12
    assert_raises TypeError do
      @cb.fildes = '12'
    end
  end

  def test_buffer
    assert_equal nil, @cb.buf
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
  end
  
  def test_reset
    @cb.offset = 4096
    @cb.lio_opcode = AIO::WRITE
    @cb.reset!
    assert_equal 0, @cb.offset
    assert_equal AIO::READ, @cb.lio_opcode    
  end
    
  def test_validate!
    assert_invalid{ @cb.fildes = 0 }
    assert_invalid{ @cb.offset = -1 }
    assert_invalid{ @cb.nbytes = 0 }
    assert_invalid{ @cb.nbytes = -1 }
    assert_invalid{ @cb.reqprio = -1 }
    assert_invalid{ @cb.lio_opcode = 12 }
    cb = AIO::CB.new do 
      self.fildes = 1
      self.offset = 0
      self.nbytes = 4096
    end
    assert_equal cb, cb.validate!      
  end

  private
  
    def assert_invalid( &block )
      assert_raises AIO::Error do
        block.call
        @cb.validate!
        @cb.reset!        
      end  
    end
    
end