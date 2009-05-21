#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $rc = 0;

$rc = plan tests => 5;
diag("Returned: " . sprintf("%d", $rc));

my $side_effect = 0;		# Check whether TODO has side effects

$rc = ok(1 == 1, '1 equals 1');	# Test ok() passes when it should
diag("Returned: $rc");

# Start TODO tests
TODO: {
	local $TODO = 'For testing purposes';

	$side_effect++;

	# This test should fail
	$rc = ok($side_effect == 0, 'side_effect checked out');
	diag("Returned: $rc");

	# This test should unexpectedly succeed
	$rc = ok($side_effect == 1, 'side_effect checked out');
	diag("Returned: $rc");
}

TODO: {
	local $TODO = 'Testing printf() expansion in todo_start()';

	$rc = ok(0, 'dummy test');
	diag("Returned: $rc");
}

$rc = ok($side_effect == 1, "side_effect is $side_effect");
diag("Returned: $rc");
