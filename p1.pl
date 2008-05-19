 package Embed::Persistent;

# p1.pl for Nagios

use strict ;

use Text::ParseWords qw(parse_line) ;

use constant	LEAVE_MSG		=> 1 ;
use constant	CACHE_DUMP		=> 2 ;
use constant	PLUGIN_DUMP		=> 4 ;

use constant	DEBUG_LEVEL		=> 0 ;
# use constant	DEBUG_LEVEL		=> CACHE_DUMP ;
# use constant	DEBUG_LEVEL		=> LEAVE_MSG ;
# use constant	DEBUG_LEVEL		=> LEAVE_MSG | CACHE_DUMP ;
# use constant	DEBUG_LEVEL		=> LEAVE_MSG | CACHE_DUMP | PLUGIN_DUMP ;

use constant	DEBUG_LOG_PATH		=> '/usr/local/nagios/var/' ;
# use constant	DEBUG_LOG_PATH		=> './' ;
use constant	LEAVE_MSG_STREAM	=> DEBUG_LOG_PATH . 'epn_leave-msgs.log' ;
use constant	CACHE_DUMP_STREAM	=> DEBUG_LOG_PATH . 'epn_cache-dump.log' ;
use constant	PLUGIN_DUMP_STREAM	=> DEBUG_LOG_PATH . 'epn_plugin-dump.log' ;

use constant	NUMBER_OF_PERL_PLUGINS	=> 60 ;

use constant	Cache_Dump_Interval	=> 20 ;
								# Cache will be dumped every Cache_Dump_Interval plugin compilations

(DEBUG_LEVEL & LEAVE_MSG)	&& do 	{
							open LH, '>> ' . LEAVE_MSG_STREAM
								or die "Can't open " . LEAVE_MSG_STREAM . ": $!" ;

								# Unbuffer LH since this will be written by child processes.

							select( (select(LH), $| = 1)[0] ) ;
					} ;
(DEBUG_LEVEL & CACHE_DUMP)	&& do	{
							 (open CH, '>> ' . CACHE_DUMP_STREAM
								or die "Can't open " . CACHE_DUMP_STREAM . ": $!") ;
							select( (select(CH), $| = 1)[0] ) ;
					} ;
(DEBUG_LEVEL & PLUGIN_DUMP)	&& (open PH, '>> ' . PLUGIN_DUMP_STREAM
								or die "Can't open " . PLUGIN_DUMP_STREAM . ": $!") ;

require Data::Dumper
	if DEBUG_LEVEL & CACHE_DUMP ;

my (%Cache, $Current_Run) ;
keys %Cache	= NUMBER_OF_PERL_PLUGINS ;
								# Offsets in %Cache{$filename}
use constant	MTIME			=> 0 ;
use constant	PLUGIN_ARGS		=> 1 ;
use constant	PLUGIN_ERROR		=> 2 ;
use constant	PLUGIN_HNDLR		=> 3 ;

package main;

use subs 'CORE::GLOBAL::exit';

sub CORE::GLOBAL::exit { die "ExitTrap: $_[0] (Redefine exit to trap plugin exit with eval BLOCK)" }


package OutputTrap;

								# Methods for use by tied STDOUT in embedded PERL module.
								# Simply ties STDOUT to a scalar and caches values written to it.
								# NB No more than 4KB characters per line are kept.
 
sub TIEHANDLE {
	my ($class) = @_;
	my $me = '';
	bless \$me, $class;
}

sub PRINT {
	my $self = shift;
	# $$self = substr(join('',@_), 0, 256) ;
	$$self .= substr(join('',@_), 0, 4096) ;
}

sub PRINTF {
	my $self = shift;
	my $fmt = shift;
	# $$self = substr(sprintf($fmt,@_), 0, 256) ;
	$$self .= substr(sprintf($fmt,@_), 0, 4096) ;
}

sub READLINE {
	my $self = shift;

# CHANGED 12/26/07 EG Following two statements didn't allow for multi-line output or output > 256 chars
								# Omit all lines after the first, per the nagios plugin guidelines
#        $$self = (split /\n/, $$self)[0];
								# Perl code other than plugins may print nothing; in this case return "(No output!)\n".
#	return $$self ? substr($$self, 0, 256) : "(No output!)\n" ;

	return $$self;
}

sub CLOSE {
	my $self = shift;
	undef $self ;
}

sub DESTROY {
	my $self = shift;
	undef $self;
}

package Embed::Persistent;

sub valid_package_name {
	local $_ = shift ;
	s|([^A-Za-z0-9\/])|sprintf("_%2x",unpack("C",$1))|eg;
								# second pass only for words starting with a digit
	s|/(\d)|sprintf("/_%2x",unpack("C",$1))|eg;
	
								# Dress it up as a real package name
	s|/|::|g;
	return /^::/ ? "Embed$_" : "Embed::$_" ;
 }

# Perl 5.005_03 only traps warnings for errors classed by perldiag
# as Fatal (eg 'Global symbol """"%s"""" requires explicit package name').
# Therefore treat all warnings as fatal.

sub throw_exception {
	die shift ;
}

sub eval_file {
	my ($filename, $delete, undef, $plugin_args) = @_ ;

	my $mtime 	= -M $filename ;
	my $ts 		= localtime(time())
				if DEBUG_LEVEL ;


	if (
		exists($Cache{$filename})	&&
		$Cache{$filename}[MTIME]	&&
	   	$Cache{$filename}[MTIME] <= $mtime
	   )  {
								# We have compiled this plugin before and
								# it has not changed on disk; nothing to do except
								# 1 parse the plugin arguments and cache them (to save 
								#   repeated parsing of the same args) - the same
								#   plugin could be called with different args.
								# 2 return the error from a former compilation
								#   if there was one.

			$Cache{$filename}[PLUGIN_ARGS]{$plugin_args} ||= [ parse_line('\s+', 0, $plugin_args) ]
				if $plugin_args ;

			if ( $Cache{$filename}[PLUGIN_ERROR] ) {
				print LH qq($ts eval_file: $filename failed compilation formerly and file has not changed; skipping compilation.\n)
					if DEBUG_LEVEL & LEAVE_MSG ;
				die qq(**ePN failed to compile $filename: "$Cache{$filename}[PLUGIN_ERROR]") ;
			} else {
				print LH qq($ts eval_file: $filename already successfully compiled and file has not changed; skipping compilation.\n)
					if DEBUG_LEVEL & LEAVE_MSG ;
				return $Cache{$filename}[PLUGIN_HNDLR] ;
			}
	}

	my $package	= valid_package_name($filename) ;

	$Cache{$filename}[PLUGIN_ARGS]{$plugin_args} ||= [ parse_line('\s+', 0, $plugin_args) ]
		if $plugin_args ;

	local *FH;
								# die will be trapped by caller (checking ERRSV)
	open FH, $filename
		or die qq(**ePN failed to open "$filename": "$!") ;

	my $sub ;
	sysread FH, $sub, -s FH ;
	close FH;
								# Cater for scripts that have embedded EOF symbols (__END__)
								# XXXX 
								# Doesn't make sense to me.

	# $sub		=~ s/__END__/\;}\n__END__/;
								# Wrap the code into a subroutine inside our unique package
								# (using $_ here [to save a lexical] is not a good idea since 
								# the definition of the package is visible to any other Perl
								# code that uses [non localised] $_).
	my $hndlr	= <<EOSUB ;
package $package;

sub hndlr {
	\@ARGV = \@_ ;
	local \$^W = 1 ;
 	\$ENV{NAGIOS_PLUGIN} = '$filename' ;

								# <<< START of PLUGIN (first line of plugin is line 8 in the text) >>>
$sub
								# <<< END of PLUGIN >>>
}
EOSUB

	$Cache{$filename}[MTIME]		= $mtime 
							unless $delete ;
								# Suppress warning display.
	local $SIG{__WARN__}			= \&throw_exception ;


                                                                # Following 3 lines added 10/18/07 by Larry Low to fix problem where
                                                                # modified Perl plugins didn't get recached by the epn
        no strict 'refs';
        undef %{$package.'::'};
        use strict 'refs';

								# Compile &$package::hndlr. Since non executable code is being eval'd
								# there is no need to protect lexicals in this scope.
	eval $hndlr;

								# $@ is set for any warning and error.
								# This guarantees that the plugin will not be run.
	if ($@) {
								# Report error line number wrt to original plugin text (7 lines added by eval_file).
								# Error text looks like
								# 'Use of uninitialized ..' at (eval 23) line 186, <DATA> line 218
								# The error line number is 'line 186'
		chomp($@) ;
		$@ =~ s/line (\d+)[\.,]/'line ' . ($1 - 7) . ','/e ;

		print LH qq($ts eval_file: syntax error in $filename: "$@".\n) 
			if DEBUG_LEVEL & LEAVE_MSG ;

	 	if ( DEBUG_LEVEL & PLUGIN_DUMP ) {
			my $i = 1 ;
			$_ = $hndlr ;
			s/^/sprintf('%10d  ', $i++)/meg ;
								# Will only get here once (when a faulty plugin is compiled). 
								# Therefore only _faulty_ plugins are dumped once each time the text changes.

			print PH qq($ts eval_file: transformed plugin "$filename" to ==>\n$_\n) ;
		}

		$@ = substr($@, 0, 4096)
			if length($@) > 4096 ;

		$Cache{$filename}[PLUGIN_ERROR] = $@ ;
								# If the compilation fails, leave nothing behind that may affect subsequent
								# compilations. This will be trapped by caller (checking ERRSV).
		die qq(**ePN failed to compile $filename: "$@") ;
		
	} else {
		$Cache{$filename}[PLUGIN_ERROR] = '' ;
	}

	print LH qq($ts eval_file: successfully compiled "$filename $plugin_args".\n)
		if DEBUG_LEVEL & LEAVE_MSG ;

	print CH qq($ts eval_file: after $Current_Run compilations \%Cache =>\n), Data::Dumper->Dump([\%Cache], [qw(*Cache)]), "\n"
		if ( (DEBUG_LEVEL & CACHE_DUMP) && (++$Current_Run % Cache_Dump_Interval == 0) ) ;

	no strict 'refs' ;
	return $Cache{$filename}[PLUGIN_HNDLR] = *{ $package . '::hndlr' }{CODE} ;

}

sub run_package {
	my ($filename, undef, $plugin_hndlr_cr, $plugin_args) = @_;
								# Second parm (after $filename) _may_ be used to wallop stashes.

	my $res		= 3 ;
	my $ts		= localtime(time()) 
				if DEBUG_LEVEL ;
     
	local $SIG{__WARN__}	= \&throw_exception ;
	my $stdout		= tie (*STDOUT, 'OutputTrap');
	my @plugin_args		= $plugin_args ? @{ $Cache{$filename}[PLUGIN_ARGS]{$plugin_args} } : () ;

								# If the plugin has args, they have been cached by eval_file.
								# ( cannot cache @plugin_args here because run_package() is
								#   called by child processes so cannot update %Cache.)

	eval { $plugin_hndlr_cr->(@plugin_args) } ;

	if ($@) {
								# Error => normal plugin termination (exit) || run time error.
		$_			= $@ ;
		/^ExitTrap: (-?\d+)/	? $res = $1 :
								# For normal plugin exit, $@ will  always match /^ExitTrap: (-?\d+)/
		/^ExitTrap:  /		? $res = 0  :	do {
								# Run time error/abnormal plugin termination.
	
								chomp ;
								# Report error line number wrt to original plugin text (7 lines added by eval_file).
								s/line (\d+)[\.,]/'line ' . ($1 - 7) . ','/e ;
								print STDOUT qq(**ePN $filename: "$_".\n) ;
							} ;
	
		($@, $_)		= ('', '') ;
	}
								# ! Error => Perl code is not a plugin (fell off the end; no exit)

								# !! (read any output from the tied file handle.)
	my $plugin_output	= <STDOUT> ;

	undef $stdout ;
	untie *STDOUT;

	print LH qq($ts run_package: "$filename $plugin_args" returning ($res, "$plugin_output").\n) 
		if DEBUG_LEVEL & LEAVE_MSG ;

	return ($res, $plugin_output) ;
}

1;

=head1 NAME

p1.pl - Perl program to provide Perl code persistence for the Nagios project (http://www.Nagios.Org).

This program provides an API for calling Nagios Perl plugins from Nagios when it is built with embedded Perl support. The
intent is to tradeoff memory usage (see BUGS) against repeated context switches to recompile Perl plugins.

=head1 SYNOPSIS


B<From C>

  /* 1 Initialise Perl - see perlembed - maintaining a persistent interpreter> */
  /* 2 Push the arguments (char *args[]) of call_pv() onto the Perl stack */
  /* 3 Compile the plugin - char *args[] is an array of pointers to strings required by p1.pl */

  call_pv("Embed::Persistent::eval_file", G_SCALAR | G_EVAL)

  /* 4 if (SvTrue(ERRSV)) */
    goto outahere ;

  /* 5 Pop the code reference to the Perl sub (corresp to the plugin) returned by Perl */
  /* 6 Push the arguments (char *args[]) of call_pv() onto the Perl stack */
  /* 7 Run the plugin */

  call_pv("Embed::Persistent::run_package", G_ARRAY)

  /* 8 example arguments for call_ functions */

  args = {  "check_rcp",       /* pointer to plugin file name          */
            "1",               /* 1 to recompile plugins each time     */
            "",                /* temporary file name - no longer used */
            "-H sundev -C nfs" /* pointer to plugin argument string    */
         }

B<From Perl>

  my ($plugin_file, $plugin_args) = split(/\s+/, $_, 2) ;
  my $plugin_hndlr_cr ;
  eval {

    # 'eval {}' simulates the G_EVAL flag to perl_call_argv('code', 'flags')
    # Otherwise, die in 'eval_file' will end the caller also.

    $plugin_hndlr_cr = Embed::Persistent::eval_file($plugin_file, 0, '', $plugin_args) ;

  } ;

  if ( $@) {
    print "plugin compilation failed.\n" ;
  } else {
    my ($rc, $output) = Embed::Persistent::run_package($plugin_file, 0, $plugin_hndlr_cr, $plugin_args) ;
    printf "embedded perl plugin return code and output was: %d & %s\n", $rc, $output) ;

In the p1.pl text, 'use constant' statements set the log path and the log level.

The log level flags determine the amount and type of messages logged in the log path.

The default log level results in similar behaviour to former versions of p1.pl -
log files will B<not> be opened.

If you want to enable logging

=over 4

=item 1 Choose log options (see below)

=item 2 Choose a log path

=item 3 Edit p1.pl

=back

Set the values of (the 'use constant' statements) the log path, B<DEBUG_LOG_PATH>, and set the B<DEBUG_LEVEL> constant to
one or more of the log options (B<LEAVE_MSG> and friends ) or'ed together.
The default is to log nothing and to use S<<path_to_Nagios>/var/epn_stderr.log> as the log path.

=head1 DESCRIPTION

Nagios is a program to monitor service availability by scheduling 'plugins' - discrete programs
that check a service (by for example simulating a users interaction with a web server using WWW::Mechanize)  and output a line of
text (the summary of the service state) for those responsible for the service, and exit with a coded value to relay the same information to Nagios.

Each plugin is run in a new child process forked by Nagios.

Plugins, like CGIs, can be coded in Perl. The persistence framework embeds a Perl interpreter in Nagios to

=over 4

=item * reduce the time taken for the Perl compile and execute cycle.

=item * eliminate the need for Nagios to fork a process (with popen) to run the Perl code.

=item * eliminate reloading of frequently used modules.

=back

and all the good things mentioned in the B<perlembed> man page under 'Maintaining a persistent interpreter'.

Plugin run-time and syntax errors, are returned to Nagios as the 'plugin output'. These messages
appear in the Nagios log like S<**ePN 'check_test' Global symbol "$status" requires explicit package name at (eval 54) line 15.>

Extra logging is given by setting DEBUG_LEVEL to include


B<LEAVE_MSG>

B<1> opens an extra output stream in the path given by the value of DEBUG_LOG_PATH

B<2> logs messages describing the success or otherwise of the plugin compilation and the result of the plugin run.

An example of such messages are

 Fri Apr 22 11:54:21 2005 eval_file: successfully compiled "/usr/local/nagios/libexec/check_bass ".
 Fri Apr 22 11:54:21 2005 run_package: "/usr/local/nagios/libexec/check_bass " returning ("0", "BASS Transaction completed Ok.
 ").
 Fri Apr 22 11:55:02 2005 eval_file: successfully compiled "/usr/local/nagios/libexec/check_ad -D production.prod -S".
 Fri Apr 22 11:55:02 2005 run_package: "/usr/local/nagios/libexec/check_ad -D foo.dom -S" returning ("0", "Ok. Expected 2 domain controllers [foo1 foo2] for "foo.dom.prod" domain from "1.1.2.3" DNS, found 8 [foo1 foo2 ..]
 ").
 Fri Apr 22 11:55:19 2005 eval_file: successfully compiled "/usr/local/nagios/libexec/check_ldap adonis".
 Fri Apr 22 11:55:19 2005 run_package: "/usr/local/nagios/libexec/check_ldap adonis" returning ("0", "Ok. Schema query response DN: dc=ipaustralia,dc=gov,dc=au aci: (target="ldap:///dc=ipaustralia,dc=gov,dc=au")(targetattr!="userPassword")(targetfi
 ").
 Fri Apr 22 11:55:29 2005 eval_file: successfully compiled "/usr/local/nagios/libexec/check_scheduler -H aphrodite -p 7003".
 Fri Apr 22 11:55:30 2005 eval_file: successfully compiled "/usr/local/nagios/libexec/check_pams -H aphrodite -p 7003 -R".
 Fri Apr 22 11:55:29 2005 run_package: "/usr/local/nagios/libexec/check_scheduler -H aphrodite -p 7003" returning ("0", "Ok. COMSQ last ran 31 seconds ago. System: 0.02s Number of jobs waiting 0 "Detail" system sch_V2_6 14/01/2005 12:22:53 aimali Jobs: COMSQ/PollerManager Fri Apr 22 11:55:00, adhoc pause Fri Apr 22 09:00:00, PAMS/SchedExamDocCheck Thu Apr 21 23:00:00, CFX Cl"
 ).
 Fri Apr 22 11:55:30 2005 run_package: "/usr/local/nagios/libexec/check_pams -H aphrodite -p 7003 -R" returning ("0", "OK PAMS Worst: Test Time 2.61 Failure Ratio 0 [0:5] Statii: BASE OK Oracle (direct) OK COMS Processor OK CCS Name Search (direct) OK Correspondence Manager OK PAMS Tier OK CASEWORK OK Objective (direct) OK Customer Manager OK 
 ").
 Fri Apr 22 11:55:45 2005 eval_file: successfully compiled "/usr/local/nagios/libexec/check_coms ".
 Fri Apr 22 11:55:45 2005 run_package: "/usr/local/nagios/libexec/check_coms " returning ("0", "COMS Ok. 11 successes 20 minutes ago. 55 minute deltas: (0 0 0 11 0 1 3 4 0 6) or <a href='http://tsitc/cgi-bin/coms_graph_deltas?INT=-2h'>graph</a>
 )

  .. after all the plugins are compiled, the 'successfully compiled mesages' are replaced by  'skipping compilation'

 Fri Apr 22 12:05:10 2005 eval_file: /usr/local/nagios/libexec/check_adds already successfully compiled and file has not changed; skipping compilation.
 Fri Apr 22 12:05:11 2005 eval_file: /usr/local/nagios/libexec/check_aub already successfully compiled and file has not changed; skipping compilation
 .
 Fri Apr 22 12:05:10 2005 run_package: "/usr/local/nagios/libexec/check_adds " returning ("0", "ADDS Transaction completed Ok.
 ").
 Fri Apr 22 12:05:13 2005 eval_file: /usr/local/nagios/libexec/check_eForm already successfully compiled and file has not changed; skipping compilation.
 Fri Apr 22 12:05:13 2005 run_package: "/usr/local/nagios/libexec/check_eForm " returning ("0", "eForm Transaction completed Ok.
 ").
 Fri Apr 22 12:05:15 2005 eval_file: /usr/local/nagios/libexec/check_cfx_log already successfully compiled and file has not changed; skipping compilation.
 Fri Apr 22 12:05:15 2005 run_package: "/usr/local/nagios/libexec/check_cfx_log -H faxgw1" returning ("0", "Ok. Last write of "//faxgw1/Faxloader$/cfxFaxLoaderClient.log"  0.0 minutes ago. File info (create, access, modify, write times): "Wed Mar 26 17:19:42 2003 Fri Apr 22 12:05:13 2005 Fri Apr 22 12:05:13 2005 Fri Apr 22 12:05:13 2005".
 ").
 Fri Apr 22 12:05:16 2005 eval_file: /usr/local/nagios/libexec/check_cfx_log already successfully compiled and file has not changed; skipping compilation.
 Fri Apr 22 12:05:16 2005 run_package: "/usr/local/nagios/libexec/check_cfx_log -H faxgw2" returning ("0", "Ok. Last write of "//faxgw2/Faxloader$/cfxFaxLoaderClient.log"  0.3 minutes ago. File info (create, access, modify, write times): "Wed Mar 26 17:27:24 2003 Fri Apr 22 12:04:55 2005 Fri Apr 22 12:04:55 2005 Fri Apr 22 12:04:55 2005".
 ").
 Fri Apr 22 12:05:17 2005 eval_file: /usr/local/nagios/libexec/check_apps_asearch already successfully compiled and file has not changed; skipping compilation.
 Fri Apr 22 12:05:18 2005 eval_file: /usr/local/nagios/libexec/check_aurioness already successfully compiled and file has not changed; skipping compi lation.
 Fri Apr 22 12:05:11 2005 run_package: "/usr/local/nagios/libexec/check_aub " returning ("0", "AU-B Transaction completed Ok.
 ").

If you are lucky enough to have plugins with errors in them,

 Fri Apr 22 12:16:01 2005 run_package: "//usr/local/nagios/libexec/eventhandlers/restart_coldfusion OK SOFT" returning ("3", "**ePN "//usr/local/nagios/libexec/eventhandlers/restart_coldfusion": "Can't use string ("") as a subroutine ref while "strict refs" in use at /usr/local/nagios/bin/p1.pl line 291, <DATA> line 218".


B<PLUGIN_DUMP>

B<1> opens an extra output stream in the path given by the value of DEBUG_LOG_PATH.

B<2> logs a listing of the text of any B<faulty> plugin - as transformed by the persistence framework. Note that plugins that compile
are B<never> dumped. This option is only useful for investigating WTF a plugin that runs from the CLI does not run under Nagios with embedded Perl.

 Sat Apr 23 19:25:32 2005 eval_file: transformed plugin "check_dummy_plugin" to ==>
         1  package Embed::check_5fdummy_5fplugin;
         2  
         3  sub hndlr {
         4      @ARGV = @_ ;
         5      local $^W = 1 ;
         6  
         7                                                              # <<< START of PLUGIN (first line of plugin is line 8 in the text) >>>
         8  #!/usr/bin/perl -w
         9  
        10  use strict 
        11  # use strict ;
        12  
        13  my @texts = split(/\n/, <<EOTEXTS) ;
        14  The Lord is near to the brokenhearted, and saves those who are crushed in spirit. Psalms 34:18 ,
        15  Let the words of my mouth, and the meditation of my heart, be acceptable in thy sight, O LORD, my strength, and my redeemer. Psalms 19:14,
        16  The LORD is my shepherd; I shall not want. He restoreth my soul: He leadeth me in the paths of righteousness for His name's sake. Psalms 23:1-2,
        17  But as many as received him, to them gave he power to become the sons of God, even to them that believe on his name John 1:12,
        18  And he said to them all, 'If any man will come after me, let him deny himself, and take up his cross daily, and follow me.' Luke 9:23,
        19  EOTEXTS
        20      #EOTEXTS
        21  
        22  print $texts[ int(rand($#texts) + 0.5) ], "\n" ;
        23  my $rc = int(rand(1) + 0.5) > 0 ? 2 : 0 ;
        24  exit $rc ;
        25  
        26  
        27                                                              # <<< END of PLUGIN >>>
        28  }


This listing is logged each time the plugin source file modification time stamp is changed (when the file is
compiled for the first time and then either by correcting a syntax error or touching the file).

Note that the plugin text (lines 8 to 27 inclusive) has been transformed by the persistence framework as described below.

B<CACHE_DUMP>

B<1> opens an extra output stream in the path given by the value of DEBUG_LOG_PATH.

B<2> A dump of the %Cache data structure (showing the plugin file modification time, a hash keyed by the plugin argument string of arrays of parsed
arguments (if non null), the last compilation error,  and a code ref to the Perl subroutine corresponding the plugin (this is undef if the plugin failed to compile).

 Sat Apr 23 19:24:59 2005 eval_file: after 5 compilations %Cache =>
 %Cache = (
           '/usr/local/nagios/libexec/check_adds' => [
                                                       '100.230810185185',
                                                       undef,
                                                       '',
                                                       sub { "DUMMY" }
                                                     ],
           'check_adds' => [
                             '3.96288194444444',
                             undef,
                             '',
                             sub { "DUMMY" }
                           ],
           'check_atmoss' => [
                               '3.96288194444444',
                               undef,
                               '',
                               sub { "DUMMY" }
                             ],
          '/usr/local/nagios/libexec/check_pams' => [
                                                       '1.90859953703704',
                                                       {
                                                         '-R -I -H asterix -p 7003' => [
                                                                                         '-R',
                                                                                         '-I',
                                                                                         '-H',
                                                                                         'asterix',
                                                                                         '-p',
                                                                                         '7003'
                                                                                       ]
                                                       },
						       sub { "DUMMY" }
                                                     ],
           'check_dummy_plugin' => [
                                     '3.96288194444444',
                                     undef,
                                     '',
                                     sub { "DUMMY" }
                                   ]
         );
  ..




This dump is produced periodically: each B<$Cache_Dump_Interval> plugin compilations the %Cache data structure is dumped. 

=head1 SUBROUTINES

Unless otherwise stated, all subroutines take two (4) arguments :-

=over 4

=item 1 plugin_filename - char * (called from C) or scalar (called from Perl): the path to the plugin.

=item 2 DO_CLEAN - boolean: set if plugin is not to be cached. Defaults to 0.

Setting this flag means that the plugin is compiled each time it is executed. Nagios B<never> sets this flag when the 
Nagios is compiled with the configure setting --with-perlcache.

=item 3 (SV *) code ref to the Perl subroutine corresponding to the plugin

This argument is only used by run_package(); it is returned by eval_file().

=item 4 plugin arguments - char ** (called from C) or scalar (called from Perl); the plugin options and arguments


=back 

=over 4

=item Embed::Persistent::eval_file( plugin_filename, DO_CLEAN, "", plugin_arguments )

E<10>
Returns B<either> a Perl code reference (an SV containing a hard reference to a subroutine) to the subroutine that
has been produced and compiled by eval_file, B<or> raises an exception (by calling die) and setting the value of B<ERRSV> or
B<$@> (if called from Perl).


eval_file() transforms the plugin to a subroutine in a package, by compiling the string containing the
transformed plugin, and caches the plugin file modification time (to avoid recompiling it), 
the parsed plugin arguments. and either the  error trapped when the plugin is compiled or a code reference to the 
compiled subroutine representing the plugin.

eval_file() caches these values in the cache named B<%Cache>. The plugin file name is the key to an array containing
the values illustrated above.

If the plugin file has not been modified, eval_file returns the cached plugin error B<or> the code ref to the plugin subroutine.

Otherwise, the plugin is compiled into a subroutine in a new package by

=over 4

=item 1 creating a package name from the plugin file name (C<Embed::Persistent::valid_package_name>)

=item 2 turning off subroutine redefinition warnings (C<use subs 'CORE::GLOBAL::exit'>)

=item 3 overriding CORE::GLOBAL::exit from within package main (C<sub CORE::GLOBAL::exit { die "ExitTrap: \$_[0] (yada)"; }>)

This allows the plugin to both call exit without taking down the persistence framework, and to return the exit code to the
Nagios.

=item 4 prepending the plugin text with code to let the plugin function as a subroutine.

The plugin command line arguments are expected to be in @ARGV, so @ARGV is set to the subroutine arguments (@_).

The new subroutine also sets the warning level to trap all warnings that may have been caused by 
by the transformation (the -w option to Perl in the text is no longer significant (because the shebang line is not fed to exec()).

=item 5 writing the plugin as the subroutine named B<hndlr> in the new package.

=item 6 returning either a code reference to the subroutine named hndlr in the package named in item 1, OR the 
compilation error trapped by eval 'string'. It is the callers responsibility to check either ERRSV (from C) or $@ (from Perl)
and skip run_package() if these values are true.

=back

=item Embed::Persistent::run_package( plugin_filename, DO_CLEAN, (SV *) plugin_hndlr_cr, plugin_argument_string )

E<10>
Returns (plugin_return_code, plugin_output)

run_plugin() actually runs the plugins with the arguments contained in the (space separated string) 4th argument.

=back

=head1 DIFFERENCES FROM PERSISTENT.PL

The example B<persistent.pl> takes no account of

=over 4

=item * Capturing output from the Perl program being run

This framework ties STDOUT to a scalar that stores the result of PRINT or PRINTF.

=item * Running Perl programs in child processes

This is the largest single difference between this framework and the example program persistent.pl. The example uses one
subroutine (eval_file()) to compile and run the program. This is unsuitable for a process like Nagios that
fork a new process to run a plugin. (It is unsuitable because were the child process 
to call eval_file() and then the update its copy of %Cache, other child processes would not get the updated %Cache,
and would therefore recompile the plugin).

Instead, eval_file() is split into two: eval_file() and run_package().

Eval_file is called by the Nagios parent process to compile the plugin 
and update %Cache. Child processes forked in base/checks.c have the same copy of %Cache and call run_plugin() to check the
last compilation error (from %Cache) and run the plugin if the plugin was error free.

=item * Dealing with Perl programs that call exit

This framework redefines exit() to die emitting a string containing the plugin return code (the first argument of exit).
Since the plugin is run by eval(), B<$@> contains this string from which the return code is extracted.

=item * Providing command line arguments to the Perl program

This framework sets @ARGV in the B<Hndlr> subroutine to the remaining subroutine arguments.

All of these clever ideas came from, AFAIK, Stephen Davies.

=back

=head1 BUGS

=item * MEMORY LEAK



This framework does nothing to prevent the memory leaks mentioned in B<perlembed>, relying on operator intervention.

Probably the best way of doing so is by periodically scheduling 

=over 4

=item 1 A check of the memory used by the Nagios process  (by running for example the standard Nagios plugin check_procs)

=item 2 Restarting Nagios with the (supplied with Nagios) startup script (restart command).


=back

If you do periodically restart Nagios, make sure that 

=over 4

=item 1 plugins all set the PATH environment variable if they need other system binaries (otherwise, if the
init script is excec'd by cron, the PATH will be reset and the plugins will fail - but only when reatsrted by cron).

=item 2 that the group owning the Nagios command pipe is the same as the Nagios group (otherwise, the restarted
Nagios will not be able to read from the command pipe).

=back

Nagios installations using the persistence framework B<must> monitor the memory use of the Nagios process and stop/start it when
the usage is exorbidant (eg, for a site with 400 services on 200 hosts and custom Perl plugins used for about 10% of the
service checks, the Nagios process uses ~80 MB after 20-30 days runningi with Perl 5.005 [Memory usage is
B<much> greater with recent Perls]. It is usually stopped and started at this point).

Note that a HUP signal is B<not> sufficient to deallocate the Perl memory; the Nagios process must be stopped and started. In fact, since HUP
causes Nagios to re-run the Perl interpreter initialisation code, memory use increases significantly. B<Don't use HUP>; use the 'restart' argument
of the Nagios supplied startup script.

There are all sorts of suprising gotchas about the debug logging including

=over 4

=item * No dump of good plugins.

Only plugins that fail to compile (after transformation) are dumped.

=item * Cache dump is periodic

The Cache is only dumped after the user specified number of plugin B<compilations>. If plugins are not compiled, you get
no dump of the cache.

=item * Debug level set at compile time

Nagios must be restarted to change the debug level (use the examples if you have a troublesome plugin;l you may need a debug copy 
of Nagios)

=item * Not all Cached fields visible

Always compile one more plugin to ensure that all the fields in the cache are set.


=back

=head1 SEE ALSO

=over 4

=item * perlembed (section on maintaining a persistent interpreter)

=item * examples in the examples/ directory including both C and Perl drivers that run Nagios plugins using p1.pl.

=back


=head1 AUTHOR

Originally by Stephen Davies.

Now maintained by Stanley Hopcroft <hopcrofts@cpan.org> who retains responsibility for the 'bad bits'.

=head1 COPYRIGHT

Copyright (c) 2004 Stanley Hopcroft. All rights reserved.
This program is free software; you can redistribute it and/or modify it under the same terms as Perl itself.

=cut

