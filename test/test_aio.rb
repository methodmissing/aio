require File.dirname(__FILE__) + '/helper'

class TestAio < Test::Unit::TestCase

  def test_read_multi
    cbs = fixtures( *%w(1.txt 2.txt 3.txt 4.txt) ).map{|f| AIO::CB.new(f) }
    cbs.each{|cb| assert cb.open? }
    assert_equal %w(one two three four), AIO.lio_listio( *cbs )
    cbs.each{|cb| assert !cb.open? }
  end

  def test_read_multi_limits
    assert_raises AIO::Error do
      AIO.lio_listio( *fixtures( *([AIO::CB.new] * 17)) )
    end  
  end 

  def test_read
    cb = AIO::CB.new(fixtures( '1.txt' ).first)
    assert cb.open?
    assert_equal 'one', AIO.read( cb )
    assert !cb.open?
  end
  
end