#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $rc = 0;

$rc = plan tests => 5;
diag("Returned: " . sprintf("%d", $rc));


$rc = ok(1 == 1, '1 equals 1');	# Test ok() passes when it should
diag("Returned: $rc");

$rc = ok(1 == 1, '1 equals 1'); # Used for %d testing in test.c
diag("Returned: $rc");

$rc = ok(1 == 1, '1 == 1');	# Test ok1() passes when it should
diag("Returned: $rc");

$rc = ok(1 == 2, '1 equals 2');	# Test ok() fails when it should
diag("Returned: $rc");

$rc = ok(1 == 2, '1 == 2');	# Test ok1() fails when it should
diag("Returned: $rc");
