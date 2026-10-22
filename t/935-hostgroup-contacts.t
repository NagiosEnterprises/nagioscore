#!/usr/bin/perl
#
# Verify hostgroup contacts/contact_groups parse, cache, and inherit
# onto contact-less services (matching host contact inheritance).
# Also covers additive (+) inheritance of contacts/contact_groups.

use warnings;
use strict;
use Test::More;

defined($ARGV[0]) or die "Usage: $0 <top build dir>";

my $top_builddir = shift @ARGV;
my $nagios = "$top_builddir/base/nagios";
my $etc = "etc";
my $precache = "var/objects.precache.hostgroup-contacts";

plan tests => 8;

my $output = `$nagios -v "$etc/nagios-hostgroup-contacts.cfg"`;
if ($? == 0) {
	pass("Nagios validated hostgroup contacts configuration");
} else {
	fail("Nagios validation failed:\n$output");
}

system("$nagios -vp '$etc/nagios-hostgroup-contacts.cfg' > /dev/null") == 0
	or die "Cannot create precached objects file";

open(my $fh, '<', $precache) or die "Cannot read $precache: $!";
local $/;
my $cache = <$fh>;
close($fh);

unlike($cache, qr/contacts\s+[^\n]*\+/,
	"No leading '+' remains in any contacts directive");
unlike($cache, qr/contact_groups\s+[^\n]*\+/,
	"No leading '+' remains in any contact_groups directive");

{
	my ($hg) = $cache =~ /(define hostgroup \{\s*hostgroup_name\s+linux-servers.*?\n\t\})/s;
	ok(defined($hg)
	   && $hg =~ /contacts\s+hg-oncall,hg-direct\s*\n/
	   && $hg =~ /contact_groups\s+hg-oncall-admins,hg-admins\s*\n/,
		"Hostgroup merges template and additive contacts without '+'");
}

{
	my ($svc) = $cache =~ /(define service \{\s*host_name\s+hg-host\s*service_description\s+inherits-contacts.*?\n\t\})/s;
	ok(defined($svc)
	   && $svc =~ /contacts\s+[^\n]*hg-direct/
	   && $svc =~ /contacts\s+[^\n]*hg-oncall/
	   && $svc =~ /contacts\s+[^\n]*host-local/
	   && $svc =~ /contact_groups\s+[^\n]*hg-admins/
	   && $svc =~ /contact_groups\s+[^\n]*hg-oncall-admins/,
		"Contact-less service inherits host and hostgroup contacts");
}

unlike($cache, qr/define service \{\s*host_name\s+hg-host\s*service_description\s+own-contacts.*?hg-direct/s,
	"Service with its own contacts does not inherit hostgroup contacts");

like($cache, qr/define service \{\s*host_name\s+hg-host\s*service_description\s+own-contacts.*?contacts\s+service-only/s,
	"Service with its own contacts keeps only those contacts");

{
	my ($host) = $cache =~ /(define host \{\s*host_name\s+hg-host.*?\n\t\})/s;
	ok(defined($host)
	   && $host =~ /contacts\s+host-local\n/
	   && $host !~ /hg-direct/
	   && $host !~ /hg-oncall/
	   && $host !~ /hg-admins/,
		"Hostgroup contacts are not flattened onto the host object");
}
