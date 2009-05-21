#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $rc = 0;

plan tests => 2;

$rc = diag("A diagnostic message");
diag("Returned: $rc");

ok(1, 'test 1') or diag "ok() failed, and shouldn't";
ok(0, 'test 2') or diag "ok() passed, and shouldn't";
