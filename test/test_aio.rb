$:.unshift "."
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

  def test_listio_write_blocking
    cbs = scratch_space( *%w(1.txt 2.txt 3.txt 4.txt) ).map{|f| WCB(f) }
    assert_equal [0, 0, 0, 0], AIO.lio_listio( *([AIO::WAIT].concat(cbs)) )        
  end

  def test_listio_write_non_blocking
    cbs = scratch_space( *%w(1.txt 2.txt 3.txt 4.txt) ).map{|f| WCB(f) }
    assert_equal nil, AIO.lio_listio( *([AIO::NOWAIT].concat(cbs)) )     
    sleep(1)
    assert_equal [0, 0, 0, 0], cbs.map{|cb| cb.buf }   
  end

  def test_listio_write_noop
    cbs = scratch_space( *%w(1.txt 2.txt 3.txt 4.txt) ).map{|f| WCB(f) }
    assert_equal nil, AIO.lio_listio( *([AIO::NOP].concat(cbs)) )     
    sleep(1)
    # TODO where's the ETS char coming from ?
    assert_equal [0,0,0,0], cbs.map{|cb| cb.buf }   
  end

  def test_listio_exceed_limits
    assert_equal AIO::QUEUE,  AIO.lio_listio( *fixtures( *([CB()] * 17)) )  
  end 

  def test_read
    cb = CB('1.txt')
    assert cb.open?
    assert_equal 'one', AIO.read( cb )
    assert !cb.open?
  end

  def test_cancel_with_cb
    cb = CB('1.txt' )
    assert cb.open?    
    AIO.read( cb )
    assert_equal AIO::ALLDONE, AIO.cancel( cb.fildes, cb )
  end

  def test_cancel_without_cb
    cb = CB('1.txt')
    assert_equal AIO::ALLDONE, AIO.cancel( cb.fildes )
  end
  
  def test_cancel_invalid_fd
    assert_aio_error do
      AIO.cancel( 'fd' )
    end
  end
  
  def test_return
    cb = CB('1.txt')
    AIO.read( cb )
    assert_aio_error do
      AIO.return(cb)
    end  
  end
  
  def test_error
    cb = CB('1.txt')
    AIO.read( cb )
    assert_aio_error do
      AIO.error(cb)
    end
  end
  
  def test_sync 
    cb = CB('1.txt')
    AIO.read( cb )
    assert_equal nil, AIO.sync( AIO::SYNC, cb)    
  end  
  
  def test_write_not_writable
    cb = CB('1.txt')
    assert cb.open?
    assert_raises IOError do
      assert_equal nil, AIO.write( cb )
    end
  end  
 
  def test_write
    cb = WCB('1.txt','w+')
    assert cb.open?
    cb.buf = 'buffer'
    assert_equal 6, AIO.write(cb)
    assert_equal 'buffer', IO.read(scratch('1.txt'))
  end
  
  def teardown
    FileUtils.rm Dir.glob("#{SCRATCH_SPACE}/*.txt")
  end
  
end