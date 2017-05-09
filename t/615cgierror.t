#!/usr/bin/perl
# 
# Check that you get an appropriate error when the CGI config file does not exist

use warnings;
use strict;
use Test::More;
use FindBin qw($Bin);

chdir $Bin or die "Cannot chdir";

my $topdir = "$Bin/..";
my $cgi_dir = "$topdir/cgi";

opendir(DIR, $cgi_dir) or die "Cannot opendir $cgi_dir: $!";
my %cgis = map { ( $_ => 1 ) } grep /\.cgi$/, readdir DIR;
closedir DIR;

# Remove these two because the output is different
my @todos = qw(statuswml.cgi statuswrl.cgi);

TODO: {
	local $TODO = "Output is different for these CGIs";
	foreach my $cgi (@todos) {
		delete $cgis{$cgi};
#		my $output = `NAGIOS_CGI_CONFIG=etc/cgi.nonexistent REQUEST_METHOD=GET $cgi_dir/$cgi`;
#		like( $output, "/Error: Could not open CGI config file 'etc/cgi.nonexistent' for reading/", "Found error for $cgi" );
	}
}
plan tests => scalar keys %cgis;

foreach my $cgi (sort keys %cgis) {
	my $output = `NAGIOS_CGI_CONFIG=etc/cgi.nonexistent REQUEST_METHOD=GET $cgi_dir/$cgi`;
	like( $output, "/Error: Could not open CGI config file 'etc/cgi.nonexistent' for reading/", "Found error for $cgi" );
}

