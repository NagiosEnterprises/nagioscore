 package Embed::Persistent;
#
# Hacked version of the sample code from the perlembedded doco.
#
# Only major changes are to separate the compiling and cacheing from 
# the execution so that the cache can be kept in "non-volatile" parent
# process while the execution is done from "volatile" child processes
# and that STDOUT is redirected to a file by means of a tied filehandle
# so that it can be returned to NetSaint in the same way as for
# commands executed via the normal popen method.
#

 use strict;
 use vars '%Cache';
 use Symbol qw(delete_package);


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

 package Embed::Persistent;

 sub valid_package_name {
     my($string) = @_;
     $string =~ s/([^A-Za-z0-9\/])/sprintf("_%2x",unpack("C",$1))/eg;
     # second pass only for words starting with a digit
     $string =~ s|/(\d)|sprintf("/_%2x",unpack("C",$1))|eg;

     # Dress it up as a real package name
     $string =~ s|/|::|g;
     return "Embed::" . $string;
 }

 sub eval_file {
     my $filename = shift;
     my $delete = shift;
     my $pn = substr($filename, rindex($filename,"/")+1);
     my $package = valid_package_name($pn);
     my $mtime = -M $filename;
     if(defined $Cache{$package}{mtime}
        &&
        $Cache{$package}{mtime} <= $mtime)
     {
        # we have compiled this subroutine already,
        # it has not been updated on disk, nothing left to do
        #print STDERR "already compiled $package->hndlr\n";
     }
     else {
        local *FH;
        open FH, $filename or die "open '$filename' $!";
        local($/) = undef;
        my $sub = <FH>;
        close FH;
	# cater for routines that expect to get args without prgname
	# and for those using @ARGV
	$sub = "shift(\@_);\n\@ARGV=\@_;\n" . $sub;

	# cater for scripts that have embedded EOF symbols (__END__)
	$sub =~ s/__END__/\;}\n__END__/;
  
        #wrap the code into a subroutine inside our unique package
        my $eval = qq{
		package main;
		use subs 'CORE::GLOBAL::exit';
		sub CORE::GLOBAL::exit { die "ExitTrap: \$_[0] ($package)"; }
                package $package; sub hndlr { $sub; }
                };
        {
            # hide our variables within this block
            my($filename,$mtime,$package,$sub);
            eval $eval;
        }
	if ($@){
		print STDERR $@."\n";
		die;
	}

        #cache it unless we're cleaning out each time
        $Cache{$package}{mtime} = $mtime unless $delete;

     }
 }

 sub run_package {
     my $filename = shift;
     my $delete = shift;
     my $tmpfname = shift;
     my $ar = shift;
     my $pn = substr($filename, rindex($filename,"/")+1);
     my $package = valid_package_name($pn);
     my $res = 0;

     tie (*STDOUT, 'OutputTrap', $tmpfname);

     my @a = split(/ /,$ar);
     
     eval {$res = $package->hndlr(@a);};

     if ($@){
		if ($@ =~ /^ExitTrap:  /) {
			$res = 0;
		} else {
              # get return code (which may be negative)
			if ($@ =~ /^ExitTrap: (-?\d+)/) {
				$res = $1;
			} else {
				$res = 2;
				print STDERR "<".$@.">\n";
			}
		}
     }
     untie *STDOUT;
     return $res;
 }

 1;
