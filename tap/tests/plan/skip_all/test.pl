#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $rc = 0;

$rc = plan skip_all => "No good reason";
diag("Returned: " . sprintf("%d", $rc));
