#!/usr/bin/perl

use warnings;
use strict;

my $rc = 0;

use Test::More;

$rc = plan qw(no_plan);
diag("Returned: " . sprintf("%d", $rc));

$rc = ok(1);
diag("Returned: $rc");
