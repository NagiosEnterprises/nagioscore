#!/usr/bin/perl
# 
# Checks for statuswml.cgi

use warnings;
use strict;
use Test::More;

# Useful for diagnostics, but not part of a core perl install
#use Test::LongString;

use FindBin qw($Bin);

chdir $Bin or die "Cannot chdir";

my $topdir = "$Bin/..";
my $cgi_dir = "$topdir/cgi";
my $statuswml = "$cgi_dir/statuswml.cgi";

my $output;
my $expected;

plan tests => 5;

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REQUEST_METHOD=GET QUERY_STRING="ping=127.0.0.1%3Becho+this+should+not+get+here" $statuswml`;
unlike( $output, "/this should not get here/", "Check that security error does not exist" );
like( $output, qr%<p>Invalid host name/ip</p>% );

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REQUEST_METHOD=GET QUERY_STRING="traceroute=127.0.0.1%3Becho+this+should+not+get+here" $statuswml`;
unlike( $output, "/this should not get here/", "Check that security error does not exist" );
like( $output, qr%<p>Invalid host name/ip</p>% );

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REQUEST_METHOD=GET QUERY_STRING="ping=127.0.0.1" $statuswml`;
like( $output, qr%<b>Results For Ping Of 127.0.0.1:</b><br/>%, "Works correctly for valid address for ping" );

# Don't run this test below, because it actually invokes traceroute
#$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REQUEST_METHOD=GET QUERY_STRING="traceroute=127.0.0.1" $statuswml`;
#like( $output, qr%<b>Results For Traceroute To 127.0.0.1:</b><br/>%, "... and traceroute" );

