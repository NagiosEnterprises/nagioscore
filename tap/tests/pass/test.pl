#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $rc = 0;

$rc = plan tests => 2;
diag("Returned: " . sprintf('%d', $rc));

$rc = pass('test to pass');
diag("Returned: $rc");

$rc = pass('test to pass with extra string');
diag("Returned: $rc");
