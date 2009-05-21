#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $rc = 0;

$rc = plan tests => 2;
diag("Returned: " . sprintf('%d', $rc));

$rc = fail('test to fail');
diag("Returned: $rc");

$rc = fail('test to fail with extra string');
diag("Returned: $rc");
