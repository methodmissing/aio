require File.dirname(__FILE__) + '/helper'

class TestControlBlock < Test::Unit::TestCase
  
  def setup
    @control_block = AIO::ControlBlock.new
  end
  
  def test_file_descriptor
    assert_equal nil, @control_block.aio_fildes
  end
  
end