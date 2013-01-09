#!/usr/bin/perl
# 
# Checks for status.cgi

use warnings;
use strict;
use Test::More;
use FindBin qw($Bin);

chdir $Bin or die "Cannot chdir";

my $topdir = "$Bin/..";
my $cgi_dir = "$topdir/cgi";
my $cgi = "$cgi_dir/status.cgi";

my $output;

plan tests => 9;

my $numhosts;

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REMOTE_USER=nagiosadmin REQUEST_METHOD=GET QUERY_STRING=host=all $cgi`;
like( $output, '/status.cgi\?host=all&sorttype=1&sortoption=1/', "Host value should be set to all if host=all passed in" );

# Bit of a hacky way to count number of hosts
$numhosts = grep /title=/, split("\n", $output);

ok( $numhosts > 1, "Got $numhosts hosts, which is more than 1");

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REMOTE_USER=nagiosadmin REQUEST_METHOD=GET QUERY_STRING=host=host1 $cgi`;
like( $output, '/status.cgi\?host=host1&sorttype=1&sortoption=1/', "Host value should be set to specific host if passed in" );
like( $output, '/1 Matching Services/', "Found the one host" );

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REMOTE_USER=nagiosadmin REQUEST_METHOD=GET QUERY_STRING=host= $cgi`;
like( $output, '/status.cgi\?host=&sorttype=1&sortoption=1/', "Host value kept as blank if set to blank" );
like( $output, '/0 Matching Services/', "Got no hosts because looking for a blank name" );

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REMOTE_USER=nagiosadmin REQUEST_METHOD=GET $cgi`;
like( $output, '/status.cgi\?host=all&sorttype=1&sortoption=1/', "Host value should be set to all if nothing set initially" );

$_ = grep /title=/, split("\n", $output);
is( $_, $numhosts, "Same number of hosts" );

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REMOTE_USER=second REQUEST_METHOD=GET QUERY_STRING=host=all $cgi`;
like( $output, '/1 Matching Services/', "Got 1 service, as permission only allows one host");

