#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $rc = 0;

$rc = plan tests => 1;
diag("Returned: " . sprintf("%d", $rc));

$rc = ok(1);
diag("Returned: $rc");

$rc = ok(1);
diag("Returned: $rc");

$rc = ok(1);
diag("Returned: $rc");
