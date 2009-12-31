#!/usr/bin/perl
# 
# Checks for status.cgi, with large number of comments and downtimes

use warnings;
use strict;
use Test::More;

use FindBin qw($Bin);

chdir $Bin or die "Cannot chdir";

my $topdir = "$Bin/..";
my $cgi_dir = "$topdir/cgi";
my $status_cgi = "$cgi_dir/status.cgi";
my $status_dat = "$Bin/var/status.dat";
my $generated = "$Bin/var/status-generated.dat";
my $generator = "$Bin/bin/generate_downtimes";

my $output;
my $expected;


my $iteration = 1;
my $iterations_max = 5;
my $copies;

plan tests => 3 * $iterations_max;

while($iteration <= $iterations_max) {
	$copies = (10)**($iteration);
	is( system("cat $status_dat > $generated"), 0, "New status.dat file" );
	is( system("$generator $copies >> $generated"), 0, "Generated $copies downtimes");
	my $num_comments = $copies + 1;

	my $start = time;
	$output = `NAGIOS_CGI_CONFIG=etc/cgi-with-generated-status.cfg REQUEST_METHOD=GET REMOTE_USER=nagiosadmin QUERY_STRING="host=host1" $status_cgi`;
	my $duration = time-$start;
	like( $output, "/This service has $num_comments comments associated with it/", "Found $num_comments comments in HTML output from status.dat. Took $duration seconds" );
	$iteration++;
}

