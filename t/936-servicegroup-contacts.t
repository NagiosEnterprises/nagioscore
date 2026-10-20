#!/usr/bin/perl
#
# Verify servicegroup contacts/contact_groups parse, cache, and inherit
# onto contact-less services (matching hostgroup contact inheritance).

use warnings;
use strict;
use Test::More;

defined($ARGV[0]) or die "Usage: $0 <top build dir>";

my $top_builddir = shift @ARGV;
my $nagios = "$top_builddir/base/nagios";
my $etc = "etc";
my $precache = "var/objects.precache.servicegroup-contacts";

plan tests => 6;

my $output = `$nagios -v "$etc/nagios-servicegroup-contacts.cfg" 2>&1`;
if ($? == 0) {
	pass("Nagios validated servicegroup contacts configuration");
} else {
	fail("Nagios validation failed:\n$output");
}

$output = `$nagios -vp '$etc/nagios-servicegroup-contacts.cfg' 2>&1`;
if ($? != 0) {
	die "Cannot create precached objects file:\n$output";
}

open(my $fh, '<', $precache) or die "Cannot read $precache: $!";
local $/;
my $cache = <$fh>;
close($fh);

like($cache, qr/define servicegroup \{\s*servicegroup_name\s+web-services.*?contacts\s+sg-direct.*?contact_groups\s+sg-admins/s,
	"Servicegroup contacts and contact_groups appear in object cache");

{
	my ($svc) = $cache =~ /(define service \{\s*host_name\s+sg-host\s*service_description\s+inherits-contacts.*?\n\t\})/s;
	ok(defined($svc)
	   && $svc =~ /contacts\s+[^\n]*sg-direct/
	   && $svc =~ /contacts\s+[^\n]*host-local/
	   && $svc =~ /contact_groups\s+sg-admins/,
		"Contact-less service inherits host and servicegroup contacts");
}

unlike($cache, qr/define service \{\s*host_name\s+sg-host\s*service_description\s+own-contacts.*?sg-direct/s,
	"Service with its own contacts does not inherit servicegroup contacts");

like($cache, qr/define service \{\s*host_name\s+sg-host\s*service_description\s+own-contacts.*?contacts\s+service-only/s,
	"Service with its own contacts keeps only those contacts");

{
	my ($svc) = $cache =~ /(define service \{\s*host_name\s+sg-host\s*service_description\s+own-contacts.*?\n\t\})/s;
	ok(defined($svc)
	   && $svc =~ /contacts\s+service-only\n/
	   && $svc !~ /sg-direct/
	   && $svc !~ /sg-admins/,
		"Servicegroup contacts are not flattened onto services that define their own contacts");
}
