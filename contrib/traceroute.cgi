#!/usr/bin/perl
#
# (c)1999 Ian Cass Knowledge Matters Ltd
#
# Adapted from code I got from somewhere. Apologies to the original
# author of the bits I stole, can't remember where I got them :)
#
# This is part of the modified check_ping plugin. This script 
# should be put in your netsaint cgi-bin directory (usually
# /usr/local/netsaint/sbin)
#
# This will perform a traceroute from your netsaint box to
# the machine that the check_ping plugin is pinging.
#

$| = 1;

# accept either traceroute/foo or traceroute?foo; default to REMOTE_ADDR
# if nothing else is specified
$addr = $ENV{PATH_INFO} || $ENV{QUERY_STRING} || $ENV{REMOTE_ADDR};

$addr =~ tr/+/ /;
$addr =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;
$addr =~ s,^addr=,,;
$addr =~ s/[^A-Za-z0-9\.-]//g;		# for security
chomp($me=`uname -n`);

print <<"EOF";
Content-Type: text/html

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<html>

<head>
  <title>Traceroute $addr</title>
</head>

<body bgcolor="#FFFFFF" text="#000000" link="#0000ee" vlink="#551a8b">

<h3>
Here is the result of a traceroute from <code>$me</code> to <code>$addr</code>:
</h3>

<pre>
EOF

open( TRACEROUTE, "/usr/sbin/traceroute $addr | " ) ||
    die "couldn't open pipe to traceroute! $!";

while (<TRACEROUTE>) {
    chomp;
    s/\&/\&amp;/g;
    s/\</\&lt;/g;
    print "$_\n";
}
close( TRACEROUTE ) || die "couldn't close pipe to traceroute! $!";

print <<"EOF";
</pre>

<br><p>End of trace</p>
<hr>
EOF

exit;

