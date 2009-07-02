require 'rake'
require 'rake/testtask'

desc 'Default: test'
task :default => :test

desc 'Run AIO tests.'
Rake::TestTask.new(:test) do |t|
  t.libs = [] #reference the installed gem instead
  t.pattern = 'test/test_*.rb'
  t.verbose = true
end

task :clean do |t|
  sh 'make clean'
end

task :build do |t|
  sh 'make'
end

task :install do |t|
  sh 'sudo make install'
end

task :setup do |t|
  Dir.chdir("ext/aio") do
    %w(clean build install).each do |task|
      Rake::Task[task].invoke    
    end
  end    
end    