#!/usr/bin/env rake
require 'rake/testtask'
require 'rake/clean'
$:.unshift(File.expand_path('lib'))
AIO_ROOT = 'ext/aio'

desc 'Default: test'
task :default => :test

desc 'Run AIO tests.'
Rake::TestTask.new(:test) do |t|
  t.libs = [AIO_ROOT]
  t.pattern = 'test/test_*.rb'
  t.verbose = true
end
task :test => :build

namespace :build do
  file "#{AIO_ROOT}/aio.c"
  file "#{AIO_ROOT}/extconf.rb"
  file "#{AIO_ROOT}/Makefile" => %W(#{AIO_ROOT}/aio.c #{AIO_ROOT}/extconf.rb) do
    Dir.chdir(AIO_ROOT) do
      ruby 'extconf.rb'
    end
  end

  desc "generate makefile"
  task :makefile => %W(#{AIO_ROOT}/Makefile #{AIO_ROOT}/aio.c)

  dlext = Config::CONFIG['DLEXT']
  file "#{AIO_ROOT}/aio.#{dlext}" => %W(#{AIO_ROOT}/Makefile #{AIO_ROOT}/aio.c) do
    Dir.chdir(AIO_ROOT) do
      sh 'make' # TODO - is there a config for which make somewhere?
    end
  end

  desc "compile aio extension"
  task :compile => "#{AIO_ROOT}/aio.#{dlext}"

  task :clean do
    Dir.chdir(AIO_ROOT) do
      sh 'make clean'
    end if File.exists?("#{AIO_ROOT}/Makefile")
  end

  CLEAN.include("#{AIO_ROOT}/Makefile")
  CLEAN.include("#{AIO_ROOT}/aio.#{dlext}")
end

task :clean => %w(build:clean)

desc "compile"
task :build => %w(build:compile)

task :install do |t|
  Dir.chdir(AIO_ROOT) do
    sh 'sudo make install'
  end
end

desc "clean build install"
task :setup => %w(clean build install)

desc "run benchmarks"
task :bench do |t|
  ruby "bench/read.rb"  
end  
task :bench => :build