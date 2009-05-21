#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $rc = 0;

$rc = plan tests => 4;
diag("Returned: " . sprintf("%d", $rc));


$rc = ok(1, 'Test with no hash');
diag("Returned: $rc");

$rc = ok(1, 'Test with one # hash');
diag("Returned: $rc");

$rc = ok(1, 'Test with # two # hashes');
diag("Returned: $rc");

$rc = ok(1, 'Test with ## back to back hashes');
diag("Returned: $rc");
