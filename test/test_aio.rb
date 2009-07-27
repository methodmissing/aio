require File.dirname(__FILE__) + '/helper'

class TestAio < Test::Unit::TestCase

  def test_read_multi
    cbs = fixtures( *%w(1.txt 2.txt 3.txt 4.txt) ).map{|f| AIO::CB.new(f) }
    cbs.each{|cb| assert cb.open? }
    assert_equal %w(one two three four), AIO.lio_listio( *cbs )
    cbs.each{|cb| assert !cb.open? }
  end

  def test_read_multi_blocking
    cbs = fixtures( *%w(1.txt 2.txt 3.txt 4.txt) ).map{|f| AIO::CB.new(f) }
    assert_equal %w(one two three four), AIO.lio_listio( *([AIO::WAIT].concat(cbs)) )        
  end

  def test_read_multi_non_blocking
    cbs = fixtures( *%w(1.txt 2.txt 3.txt 4.txt) ).map{|f| AIO::CB.new(f) }
    assert_equal nil, AIO.lio_listio( *([AIO::NOWAIT].concat(cbs)) )     
    sleep(1)
    assert_equal %w(one two three four), cbs.map{|cb| cb.buf }   
  end

  def test_read_multi_limits
    assert_aio_error do
      AIO.lio_listio( *fixtures( *([AIO::CB.new] * 17)) )
    end  
  end 

  def test_read
    cb = AIO::CB.new(fixtures( '1.txt' ).first)
    assert cb.open?
    assert_equal 'one', AIO.read( cb )
    assert !cb.open?
  end

  def test_cancel_with_cb
    cb = AIO::CB.new(fixtures( '1.txt' ).first)
    assert cb.open?    
    AIO.read( cb )
    assert_equal AIO::ALLDONE, AIO.cancel( cb.fildes, cb )
  end

  def test_cancel_without_cb
    cb = AIO::CB.new(fixtures( '1.txt' ).first)
    assert_equal AIO::ALLDONE, AIO.cancel( cb.fildes )
  end
  
  def test_cancel_invalid_fd
    assert_aio_error do
      AIO.cancel( 'fd' )
    end
  end
  
  def test_return
    cb = AIO::CB.new(fixtures( '1.txt' ).first)
    AIO.read( cb )
    assert_aio_error do
      AIO.return(cb)
    end  
  end
  
  def test_error
    cb = AIO::CB.new(fixtures( '1.txt' ).first)
    AIO.read( cb )
    assert_aio_error do
      AIO.error(cb)
    end
  end
  
end