#!/usr/bin/perl
# 
# Check that you CGI security errors are fixed

use warnings;
use strict;
use Test::More;
use FindBin qw($Bin);

chdir $Bin or die "Cannot chdir";

my $topdir = "$Bin/..";
my $cgi_dir = "$topdir/cgi";

plan 'no_plan';

my $output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REQUEST_METHOD=GET QUERY_STRING="layer=' style=xss:expression(alert('XSS')) '" $cgi_dir/statusmap.cgi`;
unlike( $output, qr/' style=xss:expression\(alert\('XSS'\)\) '/, "XSS injection not passed straight through" );

# Is this correct? Nothing weird happens anyway, so let's assume so
like( $output, qr/&#39 style&#61xss:expression&#40alert&#40&#39XSS&#39&#41&#41 &#39/, "Expected escaping of quotes" ) || diag $output;


$output = `REMOTE_USER=nagiosadmin NAGIOS_CGI_CONFIG=etc/cgi.cfg REQUEST_METHOD=GET QUERY_STRING="type=command&expand=<body onload=alert(666)>" $cgi_dir/config.cgi`;
unlike( $output, qr/<body onload=alert\(666\)>/, "XSS injection not passed through" ) || diag ($output);
