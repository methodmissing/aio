require File.dirname(__FILE__) + '/helper'

class TestAio < Test::Unit::TestCase
  
  def test_read_multi
    assert_equal %w(one two three four), AIO.read_multi( *fixtures( *%w(one.txt two.txt three.txt four.txt) ) )
  end
  
  def test_read
    assert_equal 'one', AIO.read( fixtures( 'one.txt' ).first )
  end
  
end