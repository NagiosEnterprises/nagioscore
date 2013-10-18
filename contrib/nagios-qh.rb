#!/usr/bin/env ruby

# Simple interface to interact with the Query Handler
# Daniel Wittenberg <dwittenberg2008@gmail.com>

require "socket" 
require 'optparse'

$DEBUG="false"
$DEBUGLOG=""

def debug(msg)
  if($DEBUG == "true")
    if($DEBUGLOG != "") 
      File.open("#{$DEBUGLOG}",'a') {|f| f.puts("DEBUG: #{msg}")}
    else
      puts("DEBUG: #{msg}")
    end
  end 
end

def send_cmd(cmd, mysock) 
   mysock="/var/nagios/rw/nagios.qh" if mysock.nil? 
   cmd=cmd.chomp
   debug("Starting send_cmd with command: #{cmd}")
  
   if(cmd == "quit" or cmd == "exit") 
      exit(0)
   end

   # If we forget to start with a command type assume #
   if(! cmd.start_with?('#') and ! cmd.start_with?('@'))
      cmd = "##{cmd}" 
   end

   # Cleanup the space if they forget you can't have one
   cmd.gsub!(/^#\s+/,"#")
   cmd.gsub!(/^@\s+/,"@")

   # If no socket we might as well bail 
   if(! File.socket?(mysock)) 
      puts "Not a socket (#{mysock}) sucker!"
      exit(1)
   end 

   debug("Writing command to socket: #{cmd}")
   # open socket and lets get to work!
   UNIXSocket.open("#{mysock}") do |socket|
      cmd=cmd + ''
      socket.send("#{cmd}\000", 0)
      socket.flush
      while line = socket.gets   # Read lines from the socket
         puts line.chop      # And print with platform line terminator
      end
      if(! cmd.start_with?('#'))
         socket.close
         puts ""
      end
   end
end

def print_usage()
  puts "
Simple command-line interface to Nagios Query Handler

   -h, --help                     Show this help message.
  
   -d, --debug                    Turn on debug mode
 
   -s, --socket                   Specify the query handler socket 
                                  (default: /var/nagios/rw/nagios.qh)
 
   -c, --cmd <cmd>        Run a specific command and exit
  "
  exit
end

#######################################
#    __  __          _____ _   _ 
#   |  \/  |   /\   |_   _| \ | |
#   | \  / |  /  \    | | |  \| |
#   | |\/| | / /\ \   | | | . ` |
#   | |  | |/ ____ \ _| |_| |\  |
#   |_|  |_/_/    \_\_____|_| \_|
#
#######################################

options = {}
optparse = OptionParser.new do |opts|
  opts.banner = "Usage: $0 [options]"
  
  opts.on('-d','--debug','Debugging mode on') do
      options[:debug] = true
      $DEBUG="true"
  end
  opts.on('-h','--help','Help') do
      print_usage()
  end
  opts.on('-s','--socket [socket]','Query Handler Socket') do |socket|
      options[:socket] = socket
  end
  opts.on('-c','--command command','Command') do|command|
     options[:cmd] = command
  end
end 

optparse.parse!

# If debug mode let's print our options so we confirm what our input was
options.each do |o,v|
  debug("#{o}=#{v}")
end

if(options[:cmd])
     send_cmd("##{options[:cmd]}", options[:socket])
     exit
  puts "Command not specified"
  print_usage()
end 

puts "Welcome to Nagios Query handler, here's a list of handlers available:"
puts ""

# We'll give the users a nudge to know what to type next
send_cmd("#help list", options[:socket])

puts "use @<handler> <cmd> for running queries such as '@nerd subscribe servicechecks'"
puts ""
puts "quit or exit will quit interactive mode"
  
puts ""

STDOUT.sync = true
print("qh> ")
while input = STDIN.gets
   send_cmd(input, options[:socket])
   STDOUT.sync = true
   print("qh> ")
end
