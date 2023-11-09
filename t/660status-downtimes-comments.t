#!/usr/bin/perl
# 
# Checks for status.cgi, with large number of comments and downtimes

use warnings;
use strict;
use Test::More;

defined($ARGV[0]) or die "Usage: $0 <top build dir>";

my $top_builddir = shift @ARGV;
my $cgi_dir = "$top_builddir/cgi";
my $status_cgi = "$cgi_dir/status.cgi";
my $extinfo_cgi = "$cgi_dir/extinfo.cgi";
my $status_dat = "var/status.dat";
my $generated = "var/status-generated.dat";
my $generator = "bin/generate_downtimes";

my $output;
my $expected;


my $iteration = 1;
my $iterations_max = shift @ARGV || 5;
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

	# This test is invalid - the comments displayed are in the order they are read
	# As the test status.dat generator is in a random order, the output will also be in the same
	# random order
	# Check that the comments ids are sorted
	#$output = `NAGIOS_CGI_CONFIG=etc/cgi-with-generated-status.cfg REQUEST_METHOD=GET REMOTE_USER=nagiosadmin QUERY_STRING="type=2&host=host1&service=Dummy+service" $extinfo_cgi`;
	#check_decrementing_comment_ids();

	$iteration++;
}

sub check_decrementing_comment_ids {
	my $last_id;
	my @ids = $output =~ m/<a href='cmd.cgi\?cmd_typ=4&com_id=(\d+)'>/g;
	while ($_ = shift @ids) {
		unless (defined $last_id && $last_id > $_) {
			fail("Comment ids out of order");
			return;
		}
		$last_id = $_;
	}
	ok( "All ids in order" );
}

