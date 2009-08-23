require File.dirname(__FILE__) + '/helper'

class TestAio < Test::Unit::TestCase

  def test_listio_read
    cbs = fixtures( *%w(1.txt 2.txt 3.txt 4.txt) ).map{|f| CB(f) }
    cbs.each{|cb| assert cb.open? }
    assert_equal %w(one two three four), AIO.lio_listio( *cbs )
    cbs.each{|cb| assert !cb.open? }
  end

  def test_tainted_results
    cb = CB(fixture( '1.txt' ))
    assert AIO.read( cb ).tainted?    
  end

  def test_listio_read_blocking
    cbs = fixtures( *%w(1.txt 2.txt 3.txt 4.txt) ).map{|f| CB(f) }
    assert_equal %w(one two three four), AIO.lio_listio( *([AIO::WAIT].concat(cbs)) )        
  end

  def test_listio_read_non_blocking
    cbs = fixtures( *%w(1.txt 2.txt 3.txt 4.txt) ).map{|f| CB(f) }
    assert_equal nil, AIO.lio_listio( *([AIO::NOWAIT].concat(cbs)) )     
    sleep(1)
    assert_equal %w(one two three four), cbs.map{|cb| cb.buf }   
  end

  def test_listio_read_noop
    cbs = fixtures( *%w(1.txt 2.txt 3.txt 4.txt) ).map{|f| CB(f) }
    assert_equal nil, AIO.lio_listio( *([AIO::NOP].concat(cbs)) )     
    sleep(1)
    # TODO where's the ETS char coming from ?
    assert_equal ["\x03", "", "", ""], cbs.map{|cb| cb.buf }   
  end

  def test_listio_exceed_limits
    assert_equal AIO::QUEUE,  AIO.lio_listio( *fixtures( *([CB()] * 17)) )  
  end 

  def test_read
    cb = CB(fixture( '1.txt' ))
    assert cb.open?
    assert_equal 'one', AIO.read( cb )
    assert !cb.open?
  end

  def test_cancel_with_cb
    cb = CB(fixture( '1.txt' ))
    assert cb.open?    
    AIO.read( cb )
    assert_equal AIO::ALLDONE, AIO.cancel( cb.fildes, cb )
  end

  def test_cancel_without_cb
    cb = CB(fixture( '1.txt' ))
    assert_equal AIO::ALLDONE, AIO.cancel( cb.fildes )
  end
  
  def test_cancel_invalid_fd
    assert_aio_error do
      AIO.cancel( 'fd' )
    end
  end
  
  def test_return
    cb = CB(fixture( '1.txt' ))
    AIO.read( cb )
    assert_aio_error do
      AIO.return(cb)
    end  
  end
  
  def test_error
    cb = CB(fixture( '1.txt' ))
    AIO.read( cb )
    assert_aio_error do
      AIO.error(cb)
    end
  end
  
  def test_sync 
    cb = CB(fixture( '1.txt' ))
    AIO.read( cb )
    assert_equal nil, AIO.sync( AIO::SYNC, cb)    
  end  
end