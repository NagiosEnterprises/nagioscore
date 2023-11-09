#!/usr/bin/perl
# 
# Local Checks for extinfo.cgi

use warnings;
use strict;
use Test::More;

defined($ARGV[0]) or die "Usage: $0 <top build dir>";

my $top_builddir = shift @ARGV;
my $cgi_dir = "$top_builddir/cgi";
my $extinfo_cgi = "$cgi_dir/extinfo.cgi";

my $output;
my $remote_user = "REMOTE_USER=nagiosadmin";

plan tests => 2;

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REQUEST_METHOD=GET $remote_user $extinfo_cgi`;
like( $output, "/Process Information/", "extinfo.cgi without params show the process information" );

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg REQUEST_METHOD=GET $remote_user QUERY_STRING='&type=1&host=host1' $extinfo_cgi`;
like( $output, "/Schedule downtime for all services on this host/", "extinfo.cgi allows us to set downtime for a host and all of his services" );

