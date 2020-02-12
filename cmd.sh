#!/bin/sh

# Write a command to the Nagios command file to cause
# it to disable active service checks.  This can be
# referred to as 'standby' mode in a redundant monitoring
# environment.

# Notes:
# 1) This script is not intended to be used as an
#    event handler by itself.  Instead, it is used by other
#    event handler scripts (like the redundancy examples).
# 2) In order for Nagios to process any commands that
#    are written to the command file, you must enable
#    the check_external_commands option in the main
#    configuration file.

printfcmd="/usr/bin/printf"

CommandFile="/usr/local/nagios/var/rw/nagios.cmd"

# get the current date/time in seconds since UNIX epoch
datetime=`date +%s`
datetime2=`date --date="+60 seconds" +%s`
# pipe the command to the command file
echo $datetime
echo $datetime2
`$printfcmd "[%i] ADD_HOST_COMMENT;localhost;1;0;%i;Nagios Admin;asdf a   s fa s  dfa   s dfs d; a;;fsd;;af;asdfl\n" $datetime $datetime2 >> $CommandFile` && echo $?
