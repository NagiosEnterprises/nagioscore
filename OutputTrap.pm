package OutputTrap;
#
# Methods for use by tied STDOUT in embedded PERL module.
#
# Simply redirects STDOUT to a temporary file associated with the
# current child/grandchild process.
#
 
use strict;
# Perl before 5.6 does not seem to have warnings.pm ???
#use warnings;
use IO::File;

sub TIEHANDLE {
	my ($class, $fn) = @_;
	my $handle = new IO::File "> $fn" or die "Cannot open embedded work filei $!\n";
	bless { FH => $handle, Value => 0}, $class;
}

sub PRINT {
	my $self = shift;
	my $handle = $self -> {FH};
	print $handle join("",@_);
}

sub PRINTF {
	my $self = shift;
	my $fmt = shift;
	my $handle = $self -> {FH};
	printf $handle ($fmt,@_);
}

sub CLOSE {
	my $self = shift;
	my $handle = $self -> {FH};
	close $handle;
}
1;
__END__
