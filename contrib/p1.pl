 package Embed::Persistent;

# $Id$

# $Log$
# Revision 1.5  2005-01-18 13:52:15+11  anwsmh
# 1 Set log level back to RETICENT and log file name to a placeholder for dist.
#
# Revision 1.4  2005-01-18 13:26:12+11  anwsmh
# 1 Major changes for Perl >= 5.6
#   1.1 tieing STDERR to ErrorTrap caused a SEGV in libperl because of
#       misuse of the open statement.
#   1.2 STDERR is now aliased to the handle associated with a log file
#       opened if logging is enabled.
#


# p1.pl for Nagios 2.0

# Only major changes are that STDOUT is redirected to a scalar
# by means of a tied filehandle so that it can be returned to Nagios
# without the need for a syscall to read()
#

use strict ;
use vars '%Cache' ;
use Text::ParseWords qw(parse_line) ;

use constant RETICENT	=> 1 ;
use constant GARRULOUS	=> 0 ;
use constant DEBUG		=> 0 ;

use constant EPN_STDERR_LOG => '/Path/to/embedded/Perl/Nagios/Logfile' ;

use constant TEXT_RETICENT	=> <<'RETICENT' ;

package OutputTrap;

#
# Methods for use by tied STDOUT in embedded PERL module.
#
# Simply ties STDOUT to a scalar and emulates serial semantics.
#
 
sub TIEHANDLE {
	my ($class) = @_;
        my $me ;
	bless \$me, $class;
}

sub PRINT {
	my $self = shift;
	$$self .= join("",@_);
}

sub PRINTF {
	my $self = shift;
	my $fmt = shift;
	$$self .= sprintf($fmt,@_);
}

sub READLINE {
	my $self = shift;
	# Perl code other than plugins may print nothing; in this case return "(No output!)\n".
	return(defined $$self ? $$self : "(No output!)\n");
}

sub CLOSE {
	my $self = shift;
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

# Perl 5.005_03 only traps warnings for errors classed by perldiag
# as Fatal (eg 'Global symbol """"%s"""" requires explicit package name').
# Therefore treat all warnings as fatal.

sub throw_exception {
	my $warn = shift ;
	return if $warn =~ /^Subroutine CORE::GLOBAL::exit redefined/ ;
	die $warn ;
}

sub eval_file {
	my $filename = shift;
	my $delete = shift;

	my $pn = substr($filename, rindex($filename,"/")+1);
	my $package = valid_package_name($pn);
	my $mtime = -M $filename ;
	if ( defined $Cache{$package}{mtime} &&
	     $Cache{$package}{mtime} <= $mtime) {
		# we have compiled this subroutine already,
		# it has not been updated on disk, nothing left to do
	}
	else {
		local *FH;
		# FIXME - error handling
		open FH, $filename or die "'$filename' $!";
		local($/) = undef;
		my $sub = <FH>;
		close FH;
		# cater for routines that expect to get args without progname
		# and for those using @ARGV
		$sub = qq(\nshift(\@_);\n\@ARGV=\@_;\nlocal \$^W=1;\n$sub) ;

		# cater for scripts that have embedded EOF symbols (__END__)
		$sub =~ s/__END__/\;}\n__END__/;

		# wrap the code into a subroutine inside our unique package
		my $eval = qq{
			package main;
			use subs 'CORE::GLOBAL::exit';
			sub CORE::GLOBAL::exit { die "ExitTrap: \$_[0] ($package)"; }
			package $package; sub hndlr { $sub }
			};
		$Cache{$package}{plugin_error} = 0 ;
		# suppress warning display.
		local $SIG{__WARN__} = \&throw_exception ;
		{
			# hide our variables within this block
			my ($filename, $mtime, $package, $sub);
			eval $eval;
		}
		# $@ is set for any warning and error. This guarantees that the plugin will not be run.
		if ($@) {
			# Log eval'd text of plugin.
			# Correct the line number of the error by removing the lines added (the subroutine prologue) by Embed::eval_file.
			my $i = 1 ;
			$eval =~ s/^/sprintf('%10d  ', $i++)/meg ;
			$Cache{$package}{plugin_error} = $@ ;
        		$Cache{$package}{mtime} = $mtime unless $delete;
			# If the compilation fails, leave nothing behind that may affect subsequent compilations.
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
	my $res = 3;

	tie (*STDOUT, 'OutputTrap');

	my @a = &parse_line('\s+', 0, $ar) ;

	if ( $Cache{$package}{plugin_error} ) {
		untie *STDOUT;
		# return unknown
		return (3, '**ePN' . " '$pn' " . $Cache{$package}{plugin_error} . "\n") ;
	}
     
	local $SIG{__WARN__} = \&throw_exception ;
	eval { $package->hndlr(@a); };

	if ($@) {
		if ($@ =~ /^ExitTrap:  /) {
			# For normal plugin exit the  ExitTrap string is set by the 
			# redefined CORE::GLOBAL::exit sub calling die to return a string =~ /^ExitTrap: -?\d+ $package/
			# However, there is only _one_ exit sub so the last plugin to be compiled sets _its_
			# package name.
			$res = 0;
		} else {
              		# get return code (which may be negative)
			if ($@ =~ /^ExitTrap: (-?\d+)/) {
				$res = $1;
			} else {
				# run time error/abnormal plugin termination; exit was not called in plugin
				# return unknown
				$res = 3;
				
				chomp $@ ;
				# correct line number reported by eval for the prologue added by eval_file
				$@ =~ s/(\d+)\.$/($1 - 8)/e ;
				print STDOUT '**ePN', " '$pn' ",  $@, "\n" ;
				# Don't run it again until the plugin is recompiled (clearing $Cache{$package}{plugin_error})
				# Note that the plugin should be handle any run time errors (such as timeouts)
				# that may occur in service checking.

				# FIXME - doesn't work under both 5.005 and 5.8.0. The cached value of plugin error is reset somehow.
				# $Cache{$package}{plugin_error} = $@ ;
			}
		}
	}
	# !!
	my $plugin_output = <STDOUT> ;
	untie *STDOUT;
	return ($res, $plugin_output) ;
}

1;

RETICENT

use constant TEXT_GARRULOUS	=> <<'GARRULOUS' ;

BEGIN {
	open STDERRLOG, '>> ' . EPN_STDERR_LOG 
		or die "Can't open '" . EPN_STDERR_LOG . " for append: $!" ;
}

package OutputTrap;

#
# Methods for use by tied STDOUT in embedded PERL module.
#
# Simply ties STDOUT to a scalar and emulates serial semantics.
#
 
sub TIEHANDLE {
	my ($class) = @_;
        my $me ;
	bless \$me, $class;
}

sub PRINT {
	my $self = shift;
	$$self .= join("",@_);
}

sub PRINTF {
	my $self = shift;
	my $fmt = shift;
	$$self .= sprintf($fmt,@_);
}

sub READLINE {
	my $self = shift;
	# Perl code other than plugins may print nothing; in this case return "(No output!)\n".
	return(defined $$self ? $$self : "(No output!)\n");
}

sub CLOSE {
	my $self = shift;
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

# Perl 5.005_03 only traps warnings for errors classed by perldiag
# as Fatal (eg 'Global symbol """"%s"""" requires explicit package name').
# Therefore treat all warnings as fatal.

sub throw_exception {
	my $warn = shift ;
	return if $warn =~ /^Subroutine CORE::GLOBAL::exit redefined/ ;
	die $warn ;
}

sub eval_file {
	my $filename = shift;
	my $delete = shift;

	*STDERR = *STDERRLOG ;
	
	my $pn = substr($filename, rindex($filename,"/")+1);
	my $package = valid_package_name($pn);
	my $mtime = -M $filename ;
	if ( defined $Cache{$package}{mtime} &&
	     $Cache{$package}{mtime} <= $mtime) {
		# we have compiled this subroutine already,
		# it has not been updated on disk, nothing left to do
	}
	else {
		local *FH;
		# FIXME - error handling
		open FH, $filename or die "'$filename' $!";
		local($/) = undef;
		my $sub = <FH>;
		close FH;
		# cater for routines that expect to get args without progname
		# and for those using @ARGV
		$sub = qq(\nshift(\@_);\n\@ARGV=\@_;\nlocal \$^W=1;\n$sub) ;

		# cater for scripts that have embedded EOF symbols (__END__)
		$sub =~ s/__END__/\;}\n__END__/;

		# wrap the code into a subroutine inside our unique package
		my $eval = qq{
			package main;
			use subs 'CORE::GLOBAL::exit';
			sub CORE::GLOBAL::exit { die "ExitTrap: \$_[0] ($package)"; }
			package $package; sub hndlr { $sub }
			};
		$Cache{$package}{plugin_error} = 0 ;
		# suppress warning display.
		local $SIG{__WARN__} = \&throw_exception ;
		{
			# hide our variables within this block
			my ($filename, $mtime, $package, $sub);
			eval $eval;
		}
		# $@ is set for any warning and error. This guarantees that the plugin will not be run.
		if ($@) {
			# Log eval'd text of plugin.
			# Correct the line number of the error by removing the lines added (the subroutine prologue) by Embed::eval_file.
			my $i = 1 ;
			$eval =~ s/^/sprintf('%10d  ', $i++)/meg ;
			print STDERR '[', time(), ']', qq( **ePN '$pn' error '$@' in text "\n$eval"\n) ;
			$Cache{$package}{plugin_error} = $@ ;
        		$Cache{$package}{mtime} = $mtime unless $delete;
			# If the compilation fails, leave nothing behind that may affect subsequent compilations.
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
	my $res = 3;

	tie (*STDOUT, 'OutputTrap');

	my @a = &parse_line('\s+', 0, $ar) ;

	if ( $Cache{$package}{plugin_error} ) {
		untie *STDOUT;
		# return unknown
		return (3, '**ePN' . " '$pn' " . $Cache{$package}{plugin_error} . "\n") ;
	}
     
	local $SIG{__WARN__} = \&throw_exception ;
	eval { $package->hndlr(@a); };

	if ($@) {
		if ($@ =~ /^ExitTrap:  /) {
			# For normal plugin exit the  ExitTrap string is set by the 
			# redefined CORE::GLOBAL::exit sub calling die to return a string =~ /^ExitTrap: -?\d+ $package/
			# However, there is only _one_ exit sub so the last plugin to be compiled sets _its_
			# package name.
			$res = 0;
		} else {
              		# get return code (which may be negative)
			if ($@ =~ /^ExitTrap: (-?\d+)/) {
				$res = $1;
			} else {
				# run time error/abnormal plugin termination; exit was not called in plugin
				# return unknown
				$res = 3;
				
				chomp $@ ;
				# correct line number reported by eval for the prologue added by eval_file
				$@ =~ s/(\d+)\.$/($1 - 8)/e ;
				print STDOUT '**ePN', " '$pn' ",  $@, "\n" ;
				# Don't run it again until the plugin is recompiled (clearing $Cache{$package}{plugin_error})
				# Note that the plugin should be handle any run time errors (such as timeouts)
				# that may occur in service checking.

				# FIXME - doesn't work under both 5.005 and 5.8.0. The cached value of plugin error is reset somehow.
				# $Cache{$package}{plugin_error} = $@ ;
			}
		}
	}
	# !!
	my $plugin_output = <STDOUT> ;
	untie *STDOUT;
	return ($res, $plugin_output) ;
}

1;

GARRULOUS

use constant TEXT_DEBUG		=> <<'DEBUG' ;

BEGIN {
	open STDERRLOG, '>> ' . EPN_STDERR_LOG
		or die "Can't open '" . EPN_STDERR_LOG . " for append: $!" ;
}

package OutputTrap;

#
# Methods for use by tied STDOUT in embedded PERL module.
#
# Simply ties STDOUT to a scalar and emulates serial semantics.
#
 
sub TIEHANDLE {
	my ($class) = @_;
        my $me ;
	bless \$me, $class;
}

sub PRINT {
	my $self = shift;
	$$self .= join("",@_);
}

sub PRINTF {
	my $self = shift;
	my $fmt = shift;
	$$self .= sprintf($fmt,@_);
}

sub READLINE {
	my $self = shift;
	# Perl code other than plugins may print nothing; in this case return "(No output!)\n".
	return(defined $$self ? $$self : "(No output!)\n");
}

sub CLOSE {
	my $self = shift;
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

# Perl 5.005_03 only traps warnings for errors classed by perldiag
# as Fatal (eg 'Global symbol """"%s"""" requires explicit package name').
# Therefore treat all warnings as fatal.

sub throw_exception {
	my $warn = shift ;
	return if $warn =~ /^Subroutine CORE::GLOBAL::exit redefined/ ;
	print STDERR " ... throw_exception: calling die $warn\n" ;
	die $warn ;
}

sub eval_file {
	my $filename = shift;
	my $delete = shift;

	*STDERR = *STDERRLOG ;
	
	my $pn = substr($filename, rindex($filename,"/")+1);
	my $package = valid_package_name($pn);
	my $mtime = -M $filename ;
	if ( defined $Cache{$package}{mtime} &&
	     $Cache{$package}{mtime} <= $mtime) {
		# we have compiled this subroutine already,
		# it has not been updated on disk, nothing left to do
		print STDERR "(I) \$mtime: $mtime, \$Cache{$package}{mtime}: '$Cache{$package}{mtime}' - already compiled $package->hndlr.\n";
	}
	else {
		print STDERR "(I) \$mtime: $mtime, \$Cache{$package}{mtime}: '$Cache{$package}{mtime}' - Compiling or recompiling \$filename: $filename.\n" ;
		local *FH;
		# FIXME - error handling
		open FH, $filename or die "'$filename' $!";
		local($/) = undef;
		my $sub = <FH>;
		close FH;
		# cater for routines that expect to get args without progname
		# and for those using @ARGV
		$sub = qq(\nshift(\@_);\n\@ARGV=\@_;\nlocal \$^W=1;\n$sub) ;

		# cater for scripts that have embedded EOF symbols (__END__)
		$sub =~ s/__END__/\;}\n__END__/;

		# wrap the code into a subroutine inside our unique package
		my $eval = qq{
			package main;
			use subs 'CORE::GLOBAL::exit';
			sub CORE::GLOBAL::exit { die "ExitTrap: \$_[0] ($package)"; }
			package $package; sub hndlr { $sub }
			};
		$Cache{$package}{plugin_error} = 0 ;
		# suppress warning display.
		local $SIG{__WARN__} = \&throw_exception ;
		{
			# hide our variables within this block
			my ($filename, $mtime, $package, $sub);
			eval $eval;
		}
		# $@ is set for any warning and error. This guarantees that the plugin will not be run.
		if ($@) {
			# Log eval'd text of plugin.
			# Correct the line number of the error by removing the lines added (the subroutine prologue) by Embed::eval_file.
			# $@ =~ s/line (\d+)\.\n/'line ' . ($1 - 8) . ".\n"/ge ;
			my $i = 1 ;
			$eval =~ s/^/sprintf('%10d  ', $i++)/meg ;
			print STDERR '[', time(), ']', qq( **ePN '$pn' error '$@' in text "\n$eval"\n) ;
			$Cache{$package}{plugin_error} = $@ ;
        		$Cache{$package}{mtime} = $mtime unless $delete;
			# If the compilation fails, leave nothing behind that may affect subsequent compilations.
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
	my $res = 3;

	use Data::Dumper ;
	print STDERR Data::Dumper->Dump([\%Cache], [qw(%Cache)]) ;

	tie (*STDOUT, 'OutputTrap');

	my @a = &parse_line('\s+', 0, $ar) ;

	if ( $Cache{$package}{plugin_error} ) {
		untie *STDOUT;
		# return unknown
		print STDERR " ... eval failed in eval_file, run_package returning (3, **ePN '$pn' '$Cache{$package}{plugin_error}'\\n)\n" ;
		return (3, '**ePN' . " '$pn' " . $Cache{$package}{plugin_error} . "\n") ;
	}
     
	local $SIG{__WARN__} = \&throw_exception ;
	eval { $package->hndlr(@a); };

	if ($@) {
		if ($@ =~ /^ExitTrap:  /) {
			# For normal plugin exit the  ExitTrap string is set by the 
			# redefined CORE::GLOBAL::exit sub calling die to return a string =~ /^ExitTrap: -?\d+ $package/
			# However, there is only _one_ exit sub so the last plugin to be compiled sets _its_
			# package name.
			$res = 0;
		} else {
              		# get return code (which may be negative)
			if ($@ =~ /^ExitTrap: (-?\d+)/) {
				$res = $1;
			} else {
				# run time error/abnormal plugin termination; exit was not called in plugin
				# return unknown
				$res = 3;
				
				chomp $@ ;
				# correct line number reported by eval for the prologue added by eval_file
				$@ =~ s/(\d+)\.$/($1 - 8)/e ;
				print STDOUT '**ePN', " '$pn' ",  $@, "\n" ;
				# Don't run it again until the plugin is recompiled (clearing $Cache{$package}{plugin_error})
				# Note that the plugin should be handle any run time errors (such as timeouts)
				# that may occur in service checking.

				# FIXME - doesn't work under both 5.005 and 5.8.0. The cached value of plugin error is reset somehow.
				# $Cache{$package}{plugin_error} = $@ ;
			}
		}
	}
	# !!
	my $plugin_output = <STDOUT> ;
	untie *STDOUT;
	print STDERR " ... run_package returning ('$res', '$plugin_output')\n" ;
	return ($res, $plugin_output) ;
}

1;

DEBUG

SWITCH: {
	eval TEXT_RETICENT,	last SWITCH if RETICENT ;
	eval TEXT_GARRULOUS,	last SWITCH if GARRULOUS ;
	eval TEXT_DEBUG,	last SWITCH if DEBUG ;
	die "Please set one of (use constant statements) RETICENT, GARRULOUS or DEBUG in code.\n" ;
} 

1 ;

=head1 NAME

p1.pl - Perl program to provide Perl code persistence for the Nagios project (http://www.Nagios.Org).

This program attempts to provide a mod_perl like facility for Nagios.

=head1 SYNOPSIS

Edit the text to set the values of (the 'use constant' statements) the log path, B<EPN_STDERR_LOG>, and any one
(only) of the boolean log level flags B<GARRULOUS>, B<DEBUG>, and B<RETICENT>. The default is to set RETICENT, and
to use S<<path_to_Nagios>/var/epn_stderr.log> as the log path.

The log level flags determine the amount and type of messages logged in the log path.

The RETICENT log level results in similar behaviour to former versions of p1.pl.
In particular, the log file EPN_STDERR_LOG will B<not> be opened.

=head1 DESCRIPTION

Nagios is a program to monitor service availability; it does this by scheduling 'plugins' - discrete programs
that check a service and output a line of text describing the service state intended for
those responsible for the service and exit with a coded value to relay the same information to Nagios.

Plugins, like CGIs, can be coded in Perl. The persistence framework embeds a Perl interpreter in Nagios to

=over 4

=item * reduce the time taken for the Perl compile and execute cycle.

=item * eliminate the need for the shell to fork and exec Perl.

=item * eliminate the need for Nagios to fork a process (with popen) to run the Perl code.

=item * eliminate reloading of frequently used modules.

=back

and all the good things mentioned in the B<perlembed> man page under 'Maintaining a persistent interpreter'.

Plugin syntax errors (possibly introduced when the plugin is transformed by the persistence framework) and run-time
errors are logged depending on the log level flags.

Regardless of the log level, plugin run-time and syntax errors, are returned to Nagios as the 'plugin output'. These messages
appear in the Nagios log like S<**ePN 'check_test' Global symbol "$status" requires explicit package name at (eval 54) line 15.>

=over 4

=item * GARRULOUS

=over 8

=item 1 opens an extra output stream in the path given by the value of EPN_STDERR_LOG.

=item 2 logs a listing of plugin errors, followed by a dump of the plugin source - as transformed by
the persistence framework - each time the plugin source file modification time stamp is changed (either
by correcting the syntax error or touching the file).

=back

An example of such a listing is

 [1074760243] **ePN 'check_test' error 'Global symbol "$status" requires explicit package name at (eval 54) line 15.  ' in text "
 1  
 2                      package main;
 3                      use subs 'CORE::GLOBAL::exit';
 4                      sub CORE::GLOBAL::exit { die "ExitTrap: $_[0] (Embed::check_5ftest)"; }
 5                      package Embed::check_5ftest; sub hndlr { 
 6  shift(@_);
 7  @ARGV=@_;
 8  local $^W=1;
 9  #!/usr/bin/perl -w
 10  
 11  # This plugin always does what it is told.
 12  
 13  use strict ;
 14  
 15     $status = shift @ARGV ;
 16  # my $status = shift @ARGV ;
 17     $status ||= 'fail' ;
 18  
 19  my $invert = 1 ;
 20  
 21  $status = lc($status) ;
 22  
 23  $status = ( ( $invert and $status eq 'ok'   ) ? 'fail' :
 24          ( $invert and $status eq 'fail' ) ? 'ok'   :
 25                                               $status ) ;
 26  
 27  $status eq 'ok' and do {
 28      print "Ok. Got a \"$status\". Expecting \"OK\".\n" ;
 29      exit 0 ;
 30  } ;
 31  
 32  $status eq 'fail' and do {
 33      print "Failed. Got a \"$status\". Expecting \"FAIL\".\n" ;
 34      exit 2 ;
 35  } ;
 36  
 37  # print "Mmmm. Looks like I got an "$status\" when expecting an \"ok\" or \"fail\".\n" ;
 38  print "Mmmm. Looks like I got an \"$status\" when expecting an \"ok\" or \"fail\".\n" ;
 39  exit 3 ;
 40   }
 41                      "

Note that the plugin text (lines 9 to 39 inclusive) has been transformed by the persistence frame work to
E<10>1 override the exit sub in package main

E<10>2 create a new package from the plugin file name, here Embed::check_5ftest

E<10>3 wrap the plugin in a subroutine named hndlr within the new package

E<10>4 handle the argument list the same way as for a method call, restoring @ARGV from the remaining
arguments

E<10>5 Setting the warning level to trap all warnings (note that the -w option to Perl is no
longer significant because the shebang line is not fed to exec()).

=item * DEBUG

This setting is intended only for hacking this code. In addition to the garrulous messages

=over 8

=item 1 enter, leave messages

=item 2 dump of %Cache data structure

=back

=item * RETICENT

This is the default verbosity level and recommended for production use. One line only of compile or run
time errors is reported in the Nagios log. There are no other output streams.

=back

=head1 SUBROUTINES

Unless otherwise stated, all subroutines take two (2) arguments :-

=over 4

=item 1 plugin_filename - string (called from C) or scalar(called from Perl): the path to the plugin.

=item 2 DO_CLEAN - boolean: set if plugin is not to be cached. Defaults to 0.

Setting this flag means that the plugin is compiled each time it is executed. Nagios B<never> sets this flag when the 
Nagios is compiled with the configure setting --with-perlcache.

=back 

=over 4

=item Embed::Persistent::eval_file( plugin_filename, DO_CLEAN )

E<10>
Returns nothing.


eval_file() transforms the plugin to a subroutine in a package, compiles the string containing the
transformed plugin, caches the plugin file modification time (to avoid recompiling it), and caches
any errors returned by the compilation.

If the plugin has been modified or has not been compiled before,  the plugin is transformed to a subroutine in a new package by 

creating a package name from the plugin file name (C<Embed::Persistent::valid_package_name>)

turning off subroutine redefinition warnings (C<use subs 'CORE::GLOBAL::exit'>)

overriding CORE::GLOBAL::exit from within package main (C<sub CORE::GLOBAL::exit { die "ExitTrap: \$_[0] ($package)"; }>)
This allows the plugin to call exit without taking down the persistence framework.

prepending the plugin text with code to let the plugin function as a subroutine. This code sets up
the plugin argument vector (@ARGV) from the subroutine arguments and turns on warnings.
(C<$sub = qq(\nshift(\@_);\n\@ARGV=\@_;\nlocal \$^W=1;\n$sub)>).

writing the plugin as the subroutine named 'hndlr' in the new package (C<package $package; sub hndlr { $sub }>)


The plugin compilation, performed by C<eval $eval>, caches C<$@> if the transformed plugin has syntax errors.


=item Embed::Persistent::run_package( plugin_filename, DO_CLEAN, '', plugin_argument_string )

E<10>
Returns (plugin_return_code, plugin_output)

run_package()

=back

=head1 BUGS

This framework does nothing to prevent the memory leaks mentioned in B<perlembed>, relying on operator intervention.

Probably the best way of doing so is by periodically scheduling 

=over 4

=item 1 A check of the memory used by the Nagios process  (by running for example the standard Nagios plugin check_vsz)

=item 2 Restarting Nagios with the (supplied with Nagios) startup script (restart command).


=back

If you do periodocally restart Nagios, make sure that 

=over 4

=item 1 plugins all set the PATH environment variable if they need other system binaries (otherwise, if the
init script is excec'd by cron, the PATH will be reset and the plugins will fail - but only when reatsrted by cron).

=item 2 that the group owning the Nagios command pipe is the same as the Nagios group (otherwise, the restarted
Nagios will not be able to read from the command pipe).

=back

Nagios installations using the persistence framework must monitor the memory use of the Nagios process and stop/start it when
the usage is exorbidant (eg, for a site with 400 services on 200 hosts and custom Perl plugins used for about 10% of the
service checks, the Nagios process uses ~80 MB after 20-30 days runningi with Perl 5.005 [Memory usage is
B<much> greater with recent Perls]. It is usually stopped and started at this point).

Note that a HUP signal is B<not> sufficient to deallocate the Perl memory; the Nagios process must be stopped and started.


=head1 AUTHOR

Originally by Stephen Davies.

Now maintained by Stanley Hopcroft <hopcrofts@cpan.org> who retains responsibility for the 'bad bits'.

=head1 COPYRIGHT

Copyright (c) 2004 Stanley Hopcroft. All rights reserved.
This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

