require 'test/unit'
require 'aio'

def fixtures( *files )
  files.map{|f| File.dirname(__FILE__) + "/fixtures/#{f}" }
end