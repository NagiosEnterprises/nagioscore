#!/usr/bin/perl
use Test::More;
$_ = shift @ARGV or die "Must specify a file";
if (! -e "$_") {
	plan skip_all => "$_ not compiled - please install tap library to test";
}
system "./$_";
