#!/bin/ruby
# Copyright 2004-present Facebook. All Rights Reserved.

require 'yaml'

if ARGV.length != 2 or !File.file? ARGV[0] or !File.file? ARGV[1]
  STDERR.puts "Usage: checkmodel.rb <model> <objectfile>"
  exit 7
end

modelname = ARGV[0]
rawmodel = File.read(modelname)
data = YAML.load(rawmodel)

filedesc = %x(file #{ARGV[1]}).chomp
if filedesc.include? "80386"
  toolchain = "x86-4.9"
elsif filedesc.include? "x86-64"
  toolchain = "x86_64-4.9"
elsif filedesc.include? "arm64"
  toolchain = "aarch64-linux-android-4.9"
elsif filedesc.include? "ARM"
  toolchain = "arm-linux-androideabi-4.9"
else
  STDERR.puts "Didn't understand file type or invalid architecture (#{filedesc})"
  exit 2
end

ndkversion = %x(ls -1 #{ENV["ANDROID_NDK_REPOSITORY"]}).lines[-1].chomp
if ndkversion.empty?
  STDERR.puts "Unable to find NDK directory. Is $ANDROID_NDK_REPOSITORY set?"
  exit 3
end

uname = %x(uname -s).chomp
if uname.include? "Darwin"
  arch = "darwin-x86_64"
elsif uname.include? "Linux"
  arch = "linux-x86_64"
else
  STDERR.puts "Only Linux and Darwin hosts supported at the moment"
  exit 4
end

symbols = %x(#{ENV["ANDROID_NDK_REPOSITORY"]}/#{ndkversion}/toolchains/#{toolchain}/prebuilt/#{arch}/bin/i686-linux-android-objdump -Tt #{ARGV[1]}).lines.map(&:chomp)
if $?.exitstatus != 0
  STDERR.puts "objdump was unable to parse #{ARGV[1]}"
  STDERR.puts symbols
  exit 5
end

failure = false
atleastonemangledname = false

(data['namespaces'] or []).each do |namespace|
  (namespace['classes'] or []).each do |classSymbols|
    (classSymbols['symbols'] or []).each do |symbol|
      (symbol['mangledNames'] or []).each do |mangledName|
        atleastonemangledname = true
        foundSymbols = symbols.select do |elem|
          elem.include? mangledName and elem =~ /\s#{mangledName}$/ and not elem.include? "*UND*"
        end
        if foundSymbols.empty?
          STDERR.puts "failed to find mangledName #{mangledName} for symbol #{symbol['symbolName']}"
          failure = true
        end
      end
    end
  end
end

if failure
  exit 1
end

if !atleastonemangledname
  STDERR.puts "no mangledNames found in #{ARGV[0]}"
  exit 6
end
