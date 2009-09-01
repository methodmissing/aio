Gem::Specification.new do |s|
  s.name     = "rb_aio"
  s.version  = "0.1.0"
  s.date     = "2009-09-01"
  s.summary  = "POSIX Realtime IO extension for Ruby"
  s.email    = "lourens@methodmissing.com"
  s.homepage = "http://github.com/methodmissing/rb_aio"
  s.description = "POSIX Realtime IO extension for Ruby MRI (1.8.{6,7} and 1.9.2)"
  s.has_rdoc = true
  s.authors  = ["Lourens Naudé (methodmissing)","James Tucker (raggi)","Aman Gupta (tmm1)"]
  s.platform = Gem::Platform::RUBY
  s.files    = %w[
    README
    Rakefile
    bench/read.rb
    ext/aio/extconf.rb
    ext/aio/aio.c
    aio.gemspec
  ] + Dir.glob('test/*')
  s.rdoc_options = ["--main", "README"]
  s.extra_rdoc_files = ["README"]
  s.extensions << "ext/aio/extconf.rb"
end