#!/usr/bin/perl
# 
# Checks for notifications.cgi

use warnings;
use strict;
use Test::More;
use FindBin qw($Bin);

chdir $Bin or die "Cannot chdir";

my $topdir = "$Bin/..";
my $cgi_dir = "$topdir/cgi";
my $notifications_cgi = "$cgi_dir/notifications.cgi";

my $output;

plan tests => 3;

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REQUEST_METHOD=GET $notifications_cgi`;
like( $output, "/<input type='hidden' name='host' value='all'>/", "Host value set to all if nothing set" );

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REQUEST_METHOD=GET QUERY_STRING=host=all $notifications_cgi`;
like( $output, "/<input type='hidden' name='host' value='all'>/", "Host value set to all if host=all set" );

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REQUEST_METHOD=GET QUERY_STRING=host=hostA $notifications_cgi`;
like( $output, "/<input type='hidden' name='host' value='hostA'>/", "Host value set to host in query string" );

