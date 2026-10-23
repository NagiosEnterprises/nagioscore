#!/usr/bin/perl
#
# Verify servicegroup contacts/contact_groups parse, cache, and inherit
# onto contact-less services (matching hostgroup contact inheritance).
# Also covers additive (+) inheritance of contacts/contact_groups.

use warnings;
use strict;
use Test::More;

defined($ARGV[0]) or die "Usage: $0 <top build dir>";

my $top_builddir = shift @ARGV;
my $nagios = "$top_builddir/base/nagios";
my $etc = "etc";
my $precache = "var/objects.precache.servicegroup-contacts";

plan tests => 8;

my $output = `$nagios -v "$etc/nagios-servicegroup-contacts.cfg"`;
if ($? == 0) {
	pass("Nagios validated servicegroup contacts configuration");
} else {
	fail("Nagios validation failed:\n$output");
}

system("$nagios -vp '$etc/nagios-servicegroup-contacts.cfg' > /dev/null") == 0
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
	my ($sg) = $cache =~ /(define servicegroup \{\s*servicegroup_name\s+web-services.*?\n\t\})/s;
	ok(defined($sg)
	   && $sg =~ /contacts\s+sg-oncall,sg-direct\s*\n/
	   && $sg =~ /contact_groups\s+sg-oncall-admins,sg-admins\s*\n/,
		"Servicegroup merges template and additive contacts without '+'");
}

{
	my ($svc) = $cache =~ /(define service \{\s*host_name\s+sg-host\s*service_description\s+inherits-contacts.*?\n\t\})/s;
	ok(defined($svc)
	   && $svc =~ /contacts\s+[^\n]*sg-direct/
	   && $svc =~ /contacts\s+[^\n]*sg-oncall/
	   && $svc =~ /contacts\s+[^\n]*host-local/
	   && $svc =~ /contact_groups\s+[^\n]*sg-admins/
	   && $svc =~ /contact_groups\s+[^\n]*sg-oncall-admins/,
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
	   && $svc !~ /sg-oncall/
	   && $svc !~ /sg-admins/,
		"Servicegroup contacts are not flattened onto services that define their own contacts");
}
