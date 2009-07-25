require File.dirname(__FILE__) + '/helper'

class TestControlBlock < Test::Unit::TestCase
  
  def setup
    @cb = AIO::CB.new
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
  end  

  def test_lio_opcode
    assert_equal AIO::READ, @cb.lio_opcode
    assert_equal AIO::WRITE, @cb.lio_opcode = AIO::WRITE
    assert_raises AIO::Error do
      @cb.lio_opcode = 12
    end
  end  
  
end