#!/usr/bin/perl -w

# Script to test downtime in Nagios with and without restarts

use English;
use Getopt::Long;
use Algorithm::Permute;

my $testPaused = 0;

my @SLEEPTICKS = ( "-", "\\", "|", "/", "-", "\\", "|", "/");
my $USESLEEPTICKS = 0;

my $maxStatusLevel = 3;

sub statusString {
	my $level = shift;
	my $message = shift;

	return if( $level > $maxStatusLevel);
	return sprintf( "    %s %s", humanTime( time), $message);
}

# Read the contents of a .dat file such as status.dat or retention.dat
sub readDatFile {
	my $file = shift;

	# Read the entire file into a variable
	my $oldrs = $RS;
	$RS = undef;
	open( FILE, "$file") || die "Unable to open $file for reading";
	my $contents = <FILE>;
	close( FILE) || die "Unable to close $file";
	$RS = $oldrs;

	# Strip comment and blank lines
	$contents =~ s/#.*\n//g;
	$contents =~ s/^\s*\n//g;
	$contents =~ s/\n\s*\n/\n/g;

	# Break into sections
	my @sections = split( /}\n/, $contents);

	my %contents;

	# Parse each section
	for( my $x = 0; $x < @sections; $x++) {
		my @lines = split( /\n/, $sections[ $x]);

		# Get the section type
		chomp $lines[ 0];
		$lines[ 0] =~ s/^\s*//g;
		$lines[ 0] =~ s/\s+{\s*$//g;

		# Create a hash of the remaining lines
		my $href = {};
		for( my $y = 1; $y < @lines; $y++) {
			next if( $lines[ $y] =~ /^\s*$/);
			chomp( $lines[ $y]);
			$lines[ $y] =~ s/^\s*//;
			$lines[ $y] =~ s/\s*$//;
			die "Unknown format for line: " . $lines[ $y] unless( $lines[ $y] =~ /^([^=]+)=(.*)$/);
			my( $key, $value) = ( $1, $2);
			$href->{ $key} = $value;
		}

		# Enter stuff into the contents hash
		if( $lines[ 0] =~ /^(info|program|programstatus)$/) {
			$contents{ $lines[ 0]} = $href;
		}
		elsif( $lines[ 0] =~ /^(contact|contactstatus|host|hoststatus|service|servicecomment|servicedowntime|servicestatus)$/) {
			$contents{ $lines[ 0]} = [] unless( exists( $contents{ $lines[ 0]}));
			push( @{ $contents{ $lines[ 0]}}, $href);
		}
		else {
			die "Unanticipated section encountered: " . $lines[ 0];
		}
	}

	return \%contents;
}

# Read the contents of the objects.cache file
sub readObjectCache {
	my $file = shift;

	# Read the entire file into a variable
	my $oldrs = $RS;
	$RS = undef;
	open( FILE, "$file") || die "Unable to open $file for reading";
	my $contents = <FILE>;
	close( FILE) || die "Unable to close $file";
	$RS = $oldrs;

	# Strip comment and blank lines
	$contents =~ s/#.*\n//g;
	$contents =~ s/^\s*\n//g;
	$contents =~ s/\n\s*\n/\n/g;

	# Break into sections
	my @sections = split( /}\n/, $contents);

	my %contents;

	# Parse each definition
	for( my $x = 0; $x < @sections; $x++) {
		my @lines = split( /\n/, $sections[ $x]);

		# Get the definition type
		chomp $lines[ 0];
		my $definitionType = undef;
		if( $lines[ 0] =~ /define\s+(\S+)\s*{/) {
			$definitionType = $1;
		}
		die "Unknown definition line: " . $lines[ 0] if( !defined( $definitionType));

		# Create a hash of the remaining lines
		my $href = {};
		for( my $y = 1; $y < @lines; $y++) {
			chomp( $lines[ $y]);
			next if( $lines[ $y] =~ /^\s*$/);
			die "Unknown format for line: " . $lines[ $y] unless( $lines[ $y] =~ /^\s*(\S+)\s+(.*)\s*$/);
			my( $key, $value) = ( $1, $2);
			$href->{ $key} = $value;
		}

		# Enter stuff into the contents hash
		$contents{ $definitionType} = [] unless( exists( $contents{ $definitionType}));
		push( @{ $contents{ $definitionType}}, $href);
	}

	return \%contents;
}

# Read the contents of the nagios.cfg file.
# NOTE: This function does not recursively read the contents of configuration
# files specified in the cfg_file and cfg_dir directives.
sub readCfgFile {
	my $file = shift;

	my %cfg;

	open( FILE, "$file") || die "Unable to open $file for reading";
	while( <FILE>) {
		chomp;
		next if( /^\s*#/);
		next if( /^\s*$/);
		if( /^([^=]+)\s*=\s*(.*)$/) {
			my( $key, $value) = ( $1, $2);
			if( $key =~ /cfg_file|cfg_dir/) {
				$cfg{ $key} = [] unless( exists( $cfg{ $key}));
				push( @{ $cfg{ $key}}, $value);
			}
			else {
				die "Parameter $key already exists" if( exists( $cfg{ $key}));
				$cfg{ $key} = $value;
			}
		}
		else {
			die "Unknown formatting for line: $_";
		}
	}

	return \%cfg;
}

# Send a command to Nagios to schedule downtime
sub scheduleDowntime {
	my $service = shift;
	my $start = shift;
	my $end = shift;
	my $fixed = shift;
	my $duration = shift;
	my $comment = shift;

	my $cmd = sprintf( "%s;%s;%s;%lu;%lu;%d;%u;%u;%s;%s", 
			"SCHEDULE_SVC_DOWNTIME", $service->{ "host_name"}, 
			$service->{ "service_description"}, $start, $end, $fixed, 
			0, $duration, "Downtime Testing", $comment);

	my $cmdfile = "/usr/local/nagios/var/rw/nagios.cmd";
	open( CMD, ">$cmdfile") || die "Cannot open $cmdfile for writing";
	print CMD sprintf( "[%lu] %s\n", time, $cmd);
	close( CMD) || die "Unable to close $cmdfile";
	sleep 1;
}

# Send a command to Nagios to delete downtime
sub deleteDowntime {
	my $id = shift;
	my $cfg = shift;
	my $service = shift;

	# There is a bug in Nagios that currently causes Nagios to crash when 
	# deleting a downtime. Instead of deleting the downtime, we'll just 
	# have to wait until it ends.
	my $datfile = readDatFile( $cfg->{ "status_file"});
	my $downtimes = findServiceDowntimes( $datfile, $service);
	while( scalar( @$downtimes) > 0) {
		sleep 1;
		$datfile = readDatFile( $cfg->{ "status_file"});
		$downtimes = findServiceDowntimes( $datfile, $service);
	}
	return;
	
	my $cmd = sprintf( "%s;%u", "DEL_SVC_DOWNTIME", $id);
	my $cmdfile = "/usr/local/nagios/var/rw/nagios.cmd";
	open( CMD, ">$cmdfile") || die "Cannot open $cmdfile for writing";
	print CMD sprintf( "[%lu] %s\n", time, $cmd);
	close( CMD) || die "Unable to close $cmdfile";
}

# Send a command to Nagios to process passive service check results
sub sendPassiveResults {
	my $service = shift;
	my $returnCode = shift;
	my $pluginOutput = shift;

	my $cmd = sprintf( "%s;%s;%s;%u;%s", "PROCESS_SERVICE_CHECK_RESULT", 
			$service->{ "host_name"}, $service->{ "service_description"}, 
			$returnCode, $pluginOutput);

	my $cmdfile = "/usr/local/nagios/var/rw/nagios.cmd";
	open( CMD, ">$cmdfile") || die "Cannot open $cmdfile for writing";
	print CMD sprintf( "[%lu] %s\n", time, $cmd);
	close( CMD) || die "Unable to close $cmdfile";
}

# Determine the amount of time to discover a service failure above
# the normal check results reaper frequency
sub maxDiscoveryTime {
	my $cfg = shift;
	my $service = shift;
	my $versionMajor = shift;

	my $maxDiscoveryTime = 0;
	if( $service->{ "active_checks_enabled"}) {
		$maxDiscoveryTime += ( $service->{ "check_interval"} * 
				$cfg->{ "interval_length"});
		$maxDiscoveryTime += ((( $service->{ "retry_interval"} + 
				( $versionMajor < 4 ? 
				$cfg->{ "check_result_reaper_frequency"} : 0)) * 
				$cfg->{ "interval_length"}) * 
				( $service->{ "max_check_attempts"} - 1));
	}
	return $maxDiscoveryTime;
}

# Return information about a particular service recorded in a .dat file
sub findServiceStatus {
	my $datfile = shift;
	my $service = shift;

	my $found;

	if( exists( $datfile->{ "servicestatus"})) {
		for( my $x = 0; $x < @{ $datfile->{ "servicestatus"}}; $x++) {
			if(( $datfile->{ "servicestatus"}->[ $x]->{ "host_name"} eq 
					$service->{ "host_name"}) &&
					( $datfile->{ "servicestatus"}->[ $x]->{ "service_description"} eq 
					$service->{ "service_description"})) {
				return $datfile->{ "servicestatus"}->[ $x];
			}
		}
	}

	return \$found;
}

sub findServiceNextCheck {
	my $cfg = shift;
	my $service = shift;

	my $datfile = readDatFile( $cfg->{ "status_file"});
	my $serviceStatus = findServiceStatus( $datfile, $service);
	return $serviceStatus->{ "next_check"};
}

# Return information about downtimes for a particular service recorded in a 
# .dat file
sub findServiceDowntimes {
	my $datfile = shift;
	my $service = shift;

	my @found;

	if( exists( $datfile->{ "servicedowntime"})) {
		for( my $x = 0; $x < @{ $datfile->{ "servicedowntime"}}; $x++) {
			if(( $datfile->{ "servicedowntime"}->[ $x]->{ "host_name"} eq 
					$service->{ "host_name"}) &&
					( $datfile->{ "servicedowntime"}->[ $x]->{ "service_description"} eq 
					$service->{ "service_description"})) {
				push( @found, $datfile->{ "servicedowntime"}->[ $x]);
			}
		}
	}

	return \@found;
}

# Fine a particular service in a config (.cfg) file or the objects cache.
sub findService {
	my $cfgfile = shift;
	my $host = shift;
	my $service = shift;

	if( exists( $cfgfile->{ "service"})) {
		for( my $x = 0; $x < @{ $cfgfile->{ "service"}}; $x++) {
			if(( $cfgfile->{ "service"}->[ $x]->{ "host_name"} eq $host) &&
					( $cfgfile->{ "service"}->[ $x]->{ "service_description"} eq $service)) {
				return $cfgfile->{ "service"}->[ $x];
			}
		}
	}

	return undef;
}

# Sleep for a given amount of time displaying a message while sleeping
sub noisySleep {
	my $length = shift;
	my $message = shift;

	print $message;
	print "  " if( $USESLEEPTICKS);
	for( my $x = 0; $x < $length; $x++) {
		if( $USESLEEPTICKS) {
			printf( "\b%s", $SLEEPTICKS[ $x % scalar( @SLEEPTICKS)]);
		}
		else {
			print ".";
		}
		sleep 1;
	}
	print "\b " if( $USESLEEPTICKS);
	print "\n";
}

# Sleep until a given time displaying a message while sleeping
sub noisySleepUntilTime {
	my $time = shift;
	my $message = shift;

	print $message;
	print "  " if( $USESLEEPTICKS);
	while( time < $time) {
		if( $USESLEEPTICKS) {
			printf( "\b%s", $SLEEPTICKS[ time % scalar( @SLEEPTICKS)]);
		}
		else {
			print ".";
		}
		sleep 1;
	}
	print "\b " if( $USESLEEPTICKS);
	print "\n";
}

# Sleep until a specified file is updated, displaying a message while sleeping
sub noisySleepUntilFileUpdate {
	my $file = shift;		# File whose mtime should be watched
	my $after = shift;		# Time after which file must be updated
	my $maxtime = shift;	# Maximum number of seconds after $after to wait
	my $message = shift;	# Message to display

	print $message;
	print "  " if( $USESLEEPTICKS);
	while(( ! -f $file) && ( time < ( $after + $maxtime))) {
		if( $USESLEEPTICKS) {
			printf( "\b%s", $SLEEPTICKS[ time % scalar( @SLEEPTICKS)]);
		}
		else {
			print ".";
		}
		sleep 1;
	}
	my ( $dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $mtime, 
			$ctime, $blksize, $blocks) = stat( $file);
	while(( $mtime <= $after) && ( time < ( $after + $maxtime))) {
		if( $USESLEEPTICKS) {
			printf( "\b%s", $SLEEPTICKS[ time % scalar( @SLEEPTICKS)]);
		}
		else {
			print ".";
		}
		sleep 1;
		( $dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $mtime, 
				$ctime, $blksize, $blocks) = stat( $file);
	}
	print "\b " if( $USESLEEPTICKS);
	print "\n";

	if( time >= ( $after + $maxtime)) {
		die "Maximum time exceeded waiting for $file to update\n";
	}
}

# Verify that a downtime is recorded correctly in a .dat file
sub verifyDowntime {
	my $service = shift;
	my $cfg = shift;
	my $downtimeFixed = shift;
	my $downtimeStart = shift;
	my $downtimeEnd = shift;
	my $downtimeDuration = shift;

	# Grab the status data
	print statusString( 2, "Reading status data.\n");
	my $datfile = readDatFile( $cfg->{ "status_file"});

	# Find the downtimes
	print statusString( 2, "Looking for downtimes.\n");
	my $downtimes = findServiceDowntimes( $datfile, $service);

	# Check to see if we found the correct number of downtimes (1)
	print statusString( 2, "Verifying the downtime.\n");
	if( scalar( @$downtimes) != 1) {
		print STDERR "Incorrect number of downtimes found: " . 
				scalar( @downtimes) . ".\n";
		return 0;
	}

	# Verify that the aspects of the downtime match
	if( $downtimes->[ 0]->{ "fixed"} != $downtimeFixed) {
		print STDERR sprintf( "Downtime type incorrect: should be %s, is %s.\n",
				( $downtimes->[ 0]->{ "fixed"} ? "Fixed" : "Flexible"),
				( $downtimeFixed ? "Fixed" : "Flexible"));
		return 0;
	}
	if( $downtimes->[ 0]->{ "start_time"} != $downtimeStart) {
		print STDERR sprintf( "Downtime start time incorrect: should be %lu, is %lu.\n",
				$downtimes->[ 0]->{ "start_time"}, $downtimeStart);
		return 0;
	}
	if( $downtimes->[ 0]->{ "end_time"} != $downtimeEnd) {
		print STDERR sprintf( "Downtime end time incorrect: should be %lu, is %lu.\n",
				$downtimes->[ 0]->{ "end_time"}, $downtimeEnd);
		return 0;
	}
	if( ! $downtimeFixed && ( $downtimes->[ 0]->{ "duration"} != 
			$downtimeDuration)) {
		print STDERR sprintf( "Downtime duration incorrect: should be %lu, is %lu.\n",
				$downtimes->[ 0]->{ "duration"}, $downtimeDuration);
		return 0;
	}
	return 1;
}

sub recoverService {
	my $service = shift;
	my $cfg = shift;
	my $message = shift;
	my $nagiosRunning = shift;
	my $versionMajor = shift;

	print statusString( 1, sprintf( "%s\n", $message));
	if( $service->{ "active_checks_enabled"}) {
		if(( $service->{ "host_name"} eq "localhost") && 
				( $service->{ "service_description"} eq "HTTP")) {
			system( "/sbin/service httpd start");
		}
		elsif(( $service->{ "host_name"} eq "localhost") && 
				( $service->{ "service_description"} eq "SSH")) {
			system( "/sbin/service sshd start");
		}
		else {
			die sprintf( "Don't know how to start service %s:%s",
					$service->{ "host_name"}, 
					$service->{ "service_description"});
		}
	}
	else {
		die "Nagios is not running" unless( $nagiosRunning);
		sendPassiveResults( $service, 0, "OK - Everything Okey Dokey");
	}

	# If Nagios is running, sleep so an undetected recovery doesn't 
	# interfere with future operations
	if( $nagiosRunning) {
		if( $service->{ "active_checks_enabled"}) {
			my $nextCheck = findServiceNextCheck( $cfg, $service);
			noisySleepUntilTime( $nextCheck, statusString( 2, 
					sprintf( "Waiting for next check to run at %s",
					humanTime( $nextCheck))));
		}
		if( $versionMajor < 4) {
			noisySleep( $cfg->{ "check_result_reaper_frequency"} + 2,
					statusString( 2, "Waiting for check results reaper to run"));
		}
		noisySleepUntilFileUpdate( $cfg->{ "status_file"}, time,
				$cfg->{ "status_update_interval"} * 
				$cfg->{ "StatusUpdateIntervalMultiplier"}, 
				statusString( 2, "Waiting for status to update")); 
	}

	return 1;
}

sub failService {
	my $service = shift;
	my $cfg = shift;
	my $schedule = shift;
	my $downtimeStart = shift;
	my $nagiosRunning = shift;
	my $versionMajor = shift;

	# downtimeEndCheckIncrement is how much time we're going to add before the
	# DowntimeEndCheck event due to a service check being delayed after the
	# service fail command is sent.
	my $downtimeEndCheckIncrement = 0;

	print statusString( 1, "Sending service failure.\n");
	if( $service->{ "active_checks_enabled"}) {
		if(( $service->{ "host_name"} eq "localhost") && 
				( $service->{ "service_description"} eq "HTTP")) {
			system( "/sbin/service httpd stop");
		}
		elsif(( $service->{ "host_name"} eq "localhost") && 
				( $service->{ "service_description"} eq "SSH")) {
			system( "/sbin/service sshd stop");
		}
		else {
			die sprintf( "Don't know how to stop service %s:%s",
					$service->{ "host_name"}, 
					$service->{ "service_description"});
		}
	}
	else {
		die "Nagios is not running" unless( $nagiosRunning);
		sendPassiveResults( $service, 2, "CRITICAL - Danger Will Robinson");
	}
	if( $nagiosRunning) {
		if( $service->{ "active_checks_enabled"}) {
			my $nextCheck = findServiceNextCheck( $cfg, $service);
			$downtimeEndCheckIncrement = $nextCheck - time - 
					maxDiscoveryTime( $cfg, $service, $versionMajor);
			$downtimeEndCheckIncrement = 0 if( $downtimeEndCheckIncrement < 0);
			noisySleepUntilTime( $nextCheck, statusString( 2, 
					sprintf( "Waiting for next check to run at %s",
					humanTime( $nextCheck))));
		}
		if( $versionMajor < 4) {
			noisySleep( $cfg->{ "check_result_reaper_frequency"} + 2,
					statusString( 2, "Waiting for check results reaper to run"));
		}
		noisySleepUntilFileUpdate( $cfg->{ "status_file"}, time,
				$cfg->{ "status_update_interval"} * 
				$cfg->{ "StatusUpdateIntervalMultiplier"}, 
				statusString( 2, "Waiting for status to update"));

		if( time < $downtimeStart + $schedule->{ "DowntimeEnd"}) {
			# We have not passed the end of the downtime waiting for 
			# a check that would trigger a downtime
			print statusString( 2, 
					"Verifying that flexible downtime is in effect.\n");
			my $datfile = readDatFile( $cfg->{ "status_file"});
			my $downtimes = findServiceDowntimes( $datfile, $service);
			if( scalar( @$downtimes) != 1) {
				print STDERR "Incorrect number of downtimes found: " . 
						scalar( @$downtimes) . "\n";
				return ( -1, undef);
			}
			if( ! $downtimes->[ 0]->{ "is_in_effect"}) {
				print STDERR "Downtime is not in effect\n";
				return ( -1, undef);
			}
			print statusString( 2, 
					sprintf( "The flexible downtime was triggered at %s.\n", 
					humanTime( $downtimes->[ 0]->{ "flex_downtime_start"})));
			return ( $downtimeEndCheckIncrement, $downtimes->[ 0]);
		}
		else {
			return ( $downtimeEndCheckIncrement, undef);
		}
	}
	else { 
		return ( 0, undef);
	}
}

sub checkStateAfterNagiosRestart {
	my $cfg = shift;
	my $service = shift;
	my $schedule = shift;
	my $downtimeStart = shift;
	my $downtimeFixed = shift;
	my $downtimeAfterServiceFail = shift;
	my $downtimesAtNagiosStop = shift;

	# If a flexible downtime was triggered and completed before the
	# Nagios restart, we have nothing to do.
	my $durationEndBeforeNagiosStop = ( ! $downtimeFixed && 
			( $schedule->{ "ServiceFail"} > 0) &&
			( $schedule->{ "NagiosStop"} > 0) &&
			( ref( $downtimeAfterServiceFail) eq "HASH") &&
			( $downtimeAfterServiceFail->{ "flex_downtime_start"} > 0) &&
			( $downtimeAfterServiceFail->{ "flex_downtime_start"} + 
			$schedule->{ "DowntimeDuration"} < $schedule->{ "NagiosStart"} + 
			$downtimeStart));
	print statusString( 3, 
			sprintf( "Downtime duration %s before Nagios was restarted\n", 
			( $durationEndBeforeNagiosStop ? "ended" : "did not end")));
	return 0 if ( $durationEndBeforeNagiosStop);

	# If the service failed AND recovered while Nagios was down, a flexible 
	# downtime would never be trigged.
	my $serviceCycleWhileNagiosDown = ( ! $downtimeFixed && 
			( $schedule->{ "ServiceFail"} > 0) &&
			( $schedule->{ "NagiosStop"} > 0) && 
			( $schedule->{ "NagiosStop"} < $schedule->{ "ServiceFail"}) && 
			( $schedule->{ "ServiceRecover"} < $schedule->{ "NagiosStart"}));
	print statusString( 3, sprintf( "Service %s while Nagios was down\n", 
			( $serviceCycleWhileNagiosDown ? "cycled" : "did not cycle")));
	return 0 if( $serviceCycleWhileNagiosDown);

	# downtimeEndCheckIncrement is how much time we're going to add before the
	# DowntimeEndCheck event due to the downtime not being triggered until
	# after a Nagios restart, if that even happens
	my $downtimeEndCheckIncrement = 0;

	# Determine whether the Nagios restart occurred after the end of the
	# downtime.
	my $nagiosRestartAfterDowntimeEnd = ( $schedule->{ "NagiosStart"} >
			$schedule->{ "DowntimeEnd"});

	# Use the above information and the schedule to determine whether a 
	# flexible downtime should have been triggered or should yet be triggered.
	my $downtimeTriggeredBeforeNagiosStop = 0;
	my $downtimeTriggeredAfterNagiosStart = 0;
	if( ! $downtimeFixed) {
		if(( $schedule->{ "ServiceFail"} > 0) && 
				( $schedule->{ "ServiceFail"} < $schedule->{ "NagiosStop"})) {
			$downtimeTriggeredBeforeNagiosStop = 1;
			print statusString( 3, "Downtime should have been triggered " .
					"before Nagios stopped\n");
		}
		else {
			print statusString( 3, "Downtime would not have been triggered " .
					"before Nagios stopped\n");
		}
		if( ! $nagiosRestartAfterDowntimeEnd && 
				( $schedule->{ "ServiceFail"} > 0 ) &&
				( $schedule->{ "NagiosStop"} < $schedule->{ "ServiceFail"}) &&
				( $schedule->{ "ServiceFail"} < $schedule->{ "NagiosStart"})) {
			$downtimeTriggeredAfterNagiosStart = 1;
			print statusString( 3, "Downtime should be triggered after " .
					"Nagios start\n");
		}
		else {
			print statusString( 3, "Downtime would not be triggered after " .
					"Nagios start\n");
		}
	}


	# If the downtime should have been triggered, but was not detected prior
	# to the Nagios stop, wait for it to be detected.
	if( $downtimeTriggeredAfterNagiosStart) {
		if( $service->{ "active_checks_enabled"}) {
			my $nextCheck = findServiceNextCheck( $cfg, $service);
			$downtimeEndCheckIncrement = ( $nextCheck - time) +
					( $schedule->{ "NagiosStart"} - 
					$schedule->{ "ServiceFail"});
			noisySleepUntilTime( $nextCheck, statusString( 1, 
					sprintf( "Waiting for next check to run at %s",
					humanTime( $nextCheck))));
		}
		if( time > $downtimeStart + $schedule->{ "DowntimeEnd"}) {
			# We passed the end of the downtime waiting for a check that
			# would trigger a downtime
			print statusString( 2, 
					"Downtime ended while waiting for next check.\n");
			return 0;
		}
	}

	# Next determine whether the downtime should still exist
	my $downtimeShouldExist = 0;
	if( $downtimeFixed) {
		if( time < $downtimeStart + $schedule->{ "DowntimeEnd"}) {
			# If the downtime is fixed and we have not reached the 
			# downtime end, it should still exists so check it below
			$downtimeShouldExist = 1;
		}
	}
	else {
		if( ! $downtimeTriggeredBeforeNagiosStop && 
				! $downtimeTriggeredAfterNagiosStart) {
			if( time < $downtimeStart + $schedule->{ "DowntimeEnd"}) {
				# If the downtime is flexible, it was never triggered 
				# and we have not reached the downtime end, it should 
				# still exist so check it below
				print statusString( 2, "We have not reached the downtime " .
						"end, so it should still exist.\n");
				$downtimeShouldExist = 1;
			}
		}
		else {
			# In this case the downtime is flexible and should have been
			# triggered.
			print statusString( 3, 
					sprintf( "There were %d downtime(s) when Nagios stopped.\n",
					scalar( @$downtimesAtNagiosStop)));
			if( scalar( @$downtimesAtNagiosStop) == 1) {
				print statusString( 3, 
						sprintf( "The flex downtime start for the downtime " .
						"existing when\nNagios stopped was %s\n", 
						humanTime( $downtimesAtNagiosStop->[ 0]->{ "flex_downtime_start"})));
			}
			if(( scalar( @$downtimesAtNagiosStop) == 1) && 
					( $downtimesAtNagiosStop->[ 0]->{ "flex_downtime_start"} > 0)) {
				print statusString( 2, 
						"The downtime was triggered before Nagios stopped.\n");
				if( time < 
					( $downtimesAtNagiosStop->[ 0]->{ "flex_downtime_start"} + 
					$downtimesAtNagiosStop->[ 0]->{ "duration"})) {
					# If the trigger was detected before Nagios was stopped and
					# we are still in the flexible downtime, it should exist
					print statusString( 2, "We have not reached the duration " .
							"end; it should still exist.\n");
					$downtimeShouldExist = 1;
				}
				else {
					print statusString( 2, "We have reached the duration " .
							"end; it should not exist.\n");
				}
			}
			elsif( time < ( $downtimeStart + $schedule->{ "NagiosStart"} + 
					$schedule->{ "DowntimeDuration"})) {
				# If the downtime was not triggered until after Nagios restarted
				# and we have not reached the duration end, it should still 
				# exist so check it below
				print statusString( 2, "We have not reached the duration " .
						"end; it should still exist.\n");
				$downtimeShouldExist = 1;
			}
		}
	}


	# If the downtime should exist, verify the downtime is back. If it should
	# not exist, verification that the downtime is gone happens at the 
	# DowntimeEndCheck event.
	if( $downtimeShouldExist) {
		print statusString( 1, "Verifying downtime after Nagios restart.\n");
		if( !verifyDowntime( $service, $cfg, $downtimeFixed, 
				$downtimeStart,
				$downtimeStart + ( $schedule->{ "DowntimeEnd"} - 
				$schedule->{ "DowntimeStart"}), 
				$schedule->{ "DowntimeDuration"})) {
			return -1;
		}
	}
	else {
		print statusString( 1, 
				"The downtime should have ended; it will be checked later.\n");
	}

	return $downtimeEndCheckIncrement;
}

# Perform the check of a downtime sequence
sub checkDowntime {
	my $testComment = shift;
	my $service = shift;
	my $cfg = shift;
	my $downtimeFixed = shift;
	my $schedule = shift;
	my $versionMajor = shift;

	my $startTime = time;
	my $datfile;
	my $nagiosRunning = 1;
	my $downtimesAtNagiosStop;
	my $downtimeAfterServiceFail;
	my $downtimeEndCheckIncrement = 0;

	print "$testComment\n";

	# Verify parameters
	die "No downtime start specified" unless( exists( $schedule->{ "DowntimeStart"}));
	die "No downtime end specified" unless( $schedule->{ "DowntimeEnd"} >= 0);
	die "No downtime duration specified" unless( $downtimeFixed || 
			$schedule->{ "DowntimeDuration"} >= 0);

	# Determine the amount of time to discover a service failure above
	# the normal check results reaper frequency
	my $maxDiscoveryTime = maxDiscoveryTime( $cfg, $service, $versionMajor);

	# Create an events list
	my @events;
	my $eventTime;

	# Figure out the time for the check at the end of the downtime
	if( ! $downtimeFixed && $schedule->{ "ServiceFail"} > 0) {
		if( $schedule->{ "NagiosStart"} > $schedule->{ "ServiceFail"} + 
				$schedule->{ "DowntimeDuration"} + $maxDiscoveryTime) {
			$eventTime = $schedule->{ "NagiosStart"} + 1;
		} 
		else {
			$eventTime = $schedule->{ "ServiceFail"} + 
					$schedule->{ "DowntimeDuration"};
			if( $service->{ "active_checks_enabled"}) {
				$eventTime += $maxDiscoveryTime;
			}
		}
	}
	else {
		if( $schedule->{ "NagiosStart"} > ( $schedule->{ "DowntimeEnd"} - 
				$schedule->{ "DowntimeStart"})) {
			$eventTime = $schedule->{ "NagiosStart"} + 1;
		}
		else {
			$eventTime = ( $schedule->{ "DowntimeEnd"} - 
					$schedule->{ "DowntimeStart"});
		}
	}
	push( @events, { "time" => $eventTime + $cfg->{ "status_update_interval"}, 
			"type" => "DowntimeEndCheck"});

	# Add the Nagios stop and start events if they're in the permutation
	if( $schedule->{ "NagiosStop"} > 0) {
		push( @events, { "time" => $schedule->{ "NagiosStop"}, 
				"type" => "NagiosStop"});
		if( $schedule->{ "NagiosStart"} > 0) {
			push( @events, { "time" => $schedule->{ "NagiosStart"}, 
					"type" => "NagiosStart"});
		}
		else {
			die "No Nagios restart time specified";
		}
	}

	# Add the service failure and recover events if they're in the permutation
	if( $schedule->{ "ServiceFail"} > 0) {
		push( @events, { "time" => $schedule->{ "ServiceFail"}, 
				"type" => "ServiceFail"});
		if( $schedule->{ "ServiceRecover"} > 0) {
			push( @events, { "time" => $schedule->{ "ServiceRecover"}, 
					"type" => "ServiceRecover"});
		}
		else {
			die "No service recovery time specified";
		}
	}

	# Sort the events in chronological order
	my @sortedEvents = sort { $a->{ "time"} <=> $b->{ "time"}} @events;

	# Make sure the service starts in a good state
	if( ! recoverService( $service, $cfg, 
			"Making sure the service is in a good state.", $nagiosRunning,
			$versionMajor)) {
		return -1;
	}

	# Cancel all current downtimes
	print statusString( 1, "Deleting current downtimes: ");
	$datfile = readDatFile( $cfg->{ "status_file"});
	$downtimes = findServiceDowntimes( $datfile, $service);
	for( my $x = 0; $x < @$downtimes; $x++) {
		print ", " if( $x > 0);
		print $downtimes->[ $x]->{ "downtime_id"};
		deleteDowntime( $downtimes->[ $x]->{ "downtime_id"}, $cfg, $service);
	}
	print "\n";

	# Schedule the downtime
	my $downtimeStart = time + $schedule->{ "DowntimeStart"};
	if( $downtimeFixed) {
		print statusString( 1, 
				sprintf( "Scheduling fixed downtime\n\t\tfrom %s to %s.\n",
				humanTime( $downtimeStart), 
				humanTime( $downtimeStart + ( $schedule->{ "DowntimeEnd"} - 
				$schedule->{ "DowntimeStart"}))));
	}
	else {
		print statusString( 1, sprintf( "Scheduling flexible downtime of " .
				"%d seconds\n\t\tfrom %s to %s.\n",
				$schedule->{ "DowntimeDuration"}, humanTime( $downtimeStart),
				humanTime( $downtimeStart + ( $schedule->{ "DowntimeEnd"} - 
				$schedule->{ "DowntimeStart"}))));
	}
	scheduleDowntime( $service, $downtimeStart,
			$downtimeStart + ( $schedule->{ "DowntimeEnd"} - 
			$schedule->{ "DowntimeStart"}), $downtimeFixed, 
			$schedule->{ "DowntimeDuration"}, $testComment);
	
	# Wait for the status file to update
	noisySleepUntilFileUpdate( $cfg->{ "status_file"}, time,
			$cfg->{ "status_update_interval"} * 
			$cfg->{ "StatusUpdateIntervalMultiplier"}, 
			statusString( 2, "Waiting for status to update"));

	# Verify the downtime 
	if( !verifyDowntime( $service, $cfg, $downtimeFixed, $downtimeStart,
			$downtimeStart + ( $schedule->{ "DowntimeEnd"} - 
			$schedule->{ "DowntimeStart"}), 
			$schedule->{ "DowntimeDuration"})) {
		return -1;
	}

	# Enter the event loop
	my $eventIndex = 0;
	while( $eventIndex < @sortedEvents) {

		# Sleep until the next event is to to be executed
		if( time < ( $downtimeStart + $sortedEvents[ $eventIndex]->{ "time"})) {
			my $message = statusString( 1, 
					sprintf( "Waiting for %s event at %s", 
					$sortedEvents[ $eventIndex]->{ "type"},
					humanTime( $downtimeStart + 
					$sortedEvents[ $eventIndex]->{ "time"})));
			noisySleepUntilTime(( $downtimeStart + 
					$sortedEvents[ $eventIndex]->{ "time"}), $message);
		}

		# Verify the downtime is gone at the end of the check
		if( $sortedEvents[ $eventIndex]->{ "type"} eq "DowntimeEndCheck") {
			noisySleepUntilFileUpdate( $cfg->{ "status_file"}, time,
					$cfg->{ "status_update_interval"} * 
					$cfg->{ "StatusUpdateIntervalMultiplier"}, 
					statusString( 2, "Waiting for status to update"));
					# Grab the status data again
			print statusString( 2, "Rereading status data.\n");
			$datfile = readDatFile( $cfg->{ "status_file"});

			# Find the downtimes
			print statusString( 2, "Looking for downtimes again.\n");
			$downtimes = findServiceDowntimes( $datfile, $service);
			print statusString( 1, "Verifying the downtime has ended.\n");
			if( scalar( @$downtimes) > 0) {
				for( my $y = 0; $y < @$downtimes; $y++) {
					if( $downtimes->[ $y]->{ "is_in_effect"}) {
						print STDERR "Downtime still exists.\n";
						return -1;
					}
				}
			}
		}

		# Restart Nagios
		elsif( $sortedEvents[ $eventIndex]->{ "type"} eq "NagiosStart") {
			print statusString( 1, "Starting Nagios.\n");
			system( "/sbin/service nagios start");
			system( "/bin/chgrp nagcmd /usr/local/nagios/var/rw/nagios.cmd");

			# Wait for the status file to update
			noisySleepUntilFileUpdate( $cfg->{ "status_file"}, time,
					$cfg->{ "status_update_interval"} * 
					$cfg->{ "StatusUpdateIntervalMultiplier"},
					statusString( 2, "Waiting for status to update"));

			# Check the status of things after the Nagios restart	
			$downtimeEndCheckIncrement = checkStateAfterNagiosRestart( $cfg, 
					$service, $schedule, $downtimeStart, $downtimeFixed, 
					$downtimeAfterServiceFail, $downtimesAtNagiosStop);
			if( $downtimeEndCheckIncrement < 0) {
				return -1;
			}
			elsif( $downtimeEndCheckIncrement > 0) {
				print statusString( 3, 
					sprintf( "Extending the DowntimeEndCheck time by %d " .
					"seconds\n", $downtimeEndCheckIncrement));
				$sortedEvents[ @sortedEvents - 1]->{ "time"} += 
						$downtimeEndCheckIncrement;
			}
			$nagiosRunning = 1;
		}

		# Stop Nagios
		elsif( $sortedEvents[ $eventIndex]->{ "type"} eq "NagiosStop") {
			$datfile = readDatFile( $cfg->{ "status_file"});
			$downtimesAtNagiosStop = findServiceDowntimes( $datfile, $service);
			print statusString( 3, sprintf( 
					"There are %d downtime(s) just before stopping Nagios.\n",
					scalar( @$downtimesAtNagiosStop)));
			print statusString( 1, "Stopping Nagios.\n");
			system( "/sbin/service nagios stop");
			$nagiosRunning = 0;
		}

		# Cause the service to fail
		elsif( $sortedEvents[ $eventIndex]->{ "type"} eq "ServiceFail") {
			( $downtimeEndCheckIncrement, $downtimeAfterServiceFail) = 
					failService( $service, $cfg, $schedule, $downtimeStart, 
					$nagiosRunning, $versionMajor);
			if( $downtimeEndCheckIncrement < 0) {
				return -1;
			}
			elsif( $downtimeEndCheckIncrement > 0) {
				print statusString( 3, 
					sprintf( "Extending the DowntimeEndCheck time by %d " .
					"seconds\n", $downtimeEndCheckIncrement));
				$sortedEvents[ @sortedEvents - 1]->{ "time"} += 
						$downtimeEndCheckIncrement;
			}
		}

		# Cause the service to recover
		elsif( $sortedEvents[ $eventIndex]->{ "type"} eq "ServiceRecover") {
			if( !recoverService( $service, $cfg, "Sending service recovery.", 
					$nagiosRunning, $versionMajor)) {
				return -1;
			}
		}

		# Something is dreadfully wrong
		else {
			die "Unhandled event type: " . $sortedEvents[ $eventIndex]->{ "type"};
		}
		$eventIndex++;
	}

	return( time - $startTime);
}

# Determine whether a event permutation is valid
sub isValidEventPermutation {
	my $permutation = shift;
	my $activeChecks = shift;

	# Create a hash of the events with the event name as the key and the order
	# as the value.
	my %permutation;
	for( my $x = 0; $x < @$permutation; $x++) {
		$permutation{ $permutation->[ $x]} = $x;
	}

	# The downtime cannot be created after it ends
	if( exists( $permutation{ "DowntimeEnd"}) &&
			exists( $permutation{ "EntryTime"}) && 
			( $permutation{ "DowntimeEnd"} < $permutation{ "EntryTime"})) {
		return 0;
	}

	# The downtime cannot end before it starts
	if( exists( $permutation{ "DowntimeEnd"}) &&
			exists( $permutation{ "DowntimeStart"}) && 
			( $permutation{ "DowntimeEnd"} < $permutation{ "DowntimeStart"})) {
		return 0;
	}

	# The service cannot recover before it fails
	if( exists( $permutation{ "ServiceFail"}) && 
			exists( $permutation{ "ServiceRecover"}) && 
			( $permutation{ "ServiceRecover"} < $permutation{ "ServiceFail"})) {
		return 0;
	}

	# Nagios cannot be restarted before it is initially stopped
	if( exists( $permutation{ "NagiosStop"}) && 
			exists( $permutation{ "NagiosStart"}) && 
			( $permutation{ "NagiosStart"} < $permutation{ "NagiosStop"})) {
		return 0;
	}

	# It doesn't make sense to test when a service failure occurs after the 
	# end of the downtime
	if( exists( $permutation{ "ServiceFail"}) &&
			exists( $permutation{ "DowntimeEnd"}) && 
			( $permutation{ "ServiceFail"} > $permutation{ "DowntimeEnd"})) {
		return 0;
	}

	# It doesn't make sense to test when a service failure occurs before the 
	# downtime is created
	if( exists( $permutation{ "ServiceFail"}) &&
			exists( $permutation{ "EntryTime"}) && 
			( $permutation{ "ServiceFail"} < $permutation{ "EntryTime"})) {
		return 0;
	}

	# It doesn't make sense to test when a service failure occurs before the 
	# downtime starts
	if( exists( $permutation{ "ServiceFail"}) &&
			exists( $permutation{ "DowntimeStart"}) && 
			( $permutation{ "ServiceFail"} < $permutation{ "DowntimeStart"})) {
		return 0;
	}

	# It doesn't make sense to test when a Nagios shutdown occurs after the 
	# end of the downtime
	if( exists( $permutation{ "NagiosStop"}) &&
			exists( $permutation{ "DowntimeEnd"}) && 
			( $permutation{ "NagiosStop"} > $permutation{ "DowntimeEnd"})) {
			return 0;
	}

	# A downtime cannot be created when Nagios is shutdown and it doesn't 
	# make sense to test when Nagios is shutdown and restarted before the
	# downtime is created
	if( exists( $permutation{ "NagiosStop"}) &&
			exists( $permutation{ "EntryTime"}) && 
			( $permutation{ "NagiosStop"} < $permutation{ "EntryTime"})) {
		return 0;
	}

	# It doesn't make sense for the duration to be so short it ends before
	# any real testing occurs
	if( exists( $permutation{ "DurationEnd"}) &&
			( $permutation{ "DurationEnd"} < 1)) {
		return 0;
	}

	# The following only applies to passive checks
	unless( $activeChecks) {
		# A passive service failure cannot be sent to Nagios when it is not
		# running
		if( exists( $permutation{ "ServiceFail"}) &&
				exists( $permutation{ "NagiosStop"}) && 
				exists( $permutation{ "NagiosStart"}) && 
				( $permutation{ "ServiceFail"} > $permutation{ "NagiosStop"}) &&
				( $permutation{ "ServiceFail"} < $permutation{ "NagiosStart"})) {
			return 0;
		}

		# A passive service recovery cannot be sent to Nagios when it is not
		# running
		if( exists( $permutation{ "ServiceRecover"}) &&
				exists( $permutation{ "NagiosStop"}) && 
				exists( $permutation{ "NagiosStart"}) && 
				( $permutation{ "ServiceRecover"} > $permutation{ "NagiosStop"}) &&
				( $permutation{ "ServiceRecover"} < $permutation{ "NagiosStart"})) {
			return 0;
		}
	}
	return 1;
}

# Display a time in human-readable format
sub humanTime {
	my $epoch = shift;
	my $longFormat = 0;
	$longFormat = shift if( @_);

	my ( $sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) =
			localtime( $epoch);	
	if( $longFormat) {
		return sprintf( "%04d-%02d-%02d %02d:%02d:%02d", $year+1900, $mon+1, 
				$mday, $hour, $min, $sec);
	}
	else {
		return sprintf( "%02d:%02d:%02d", $hour, $min, $sec);
	}
}

# Display a time duration in human-readable format
sub humanDuration {
	my $duration = shift;
	my $wordyFormat = 0;
	$wordyFormat = shift if( @_);

	my ( $durationLeft, $hours, $minutes, $seconds);
	$durationLeft = $duration;
	$hours = int( $durationLeft / ( 60 * 60));
	$durationLeft -= $hours * 60 * 60;
	$minutes = int( $durationLeft / 60);
	$durationLeft -= $minutes * 60;
	$seconds = $durationLeft;

	if( $wordyFormat) {
		if( $hours > 0) {
			return sprintf( "%d hour%s, %d minute%s, %d second%s", 
					$hours, ( $hours == 1 ? "" : "s"), 
					$minutes, ( $minutes == 1 ? "" : "s"), 
					$seconds, ( $seconds == 1 ? "" : "s"));
		}
		elsif( $minutes > 0) {
			return sprintf( "%d minute%s, %d second%s", 
					$minutes, ( $minutes == 1 ?  "" : "s"), 
					$seconds, ( $seconds == 1 ? "" : "s"));
		}
		else {
			return sprintf( "%d second%s", $duration, 
					( $duration == 1 ?  "" : "s"));
		}
	}
	else {
		return sprintf( "%02d:%02d:%02d (%ds)", $hours, $minutes, $seconds,
				$duration);
	}
}

# Schedule a downtime series' event times
sub scheduleDowntimePermutation {
	my $service = shift;
	my $cfg = shift;
	my $permutation = shift;
	my $versionMajor = shift;

	# Initialize times
	my %schedule;
	$schedule{ "DowntimeStart"} = -1;
	$schedule{ "DowntimeEnd"} = -1;
	$schedule{ "DowntimeDuration"} = -1;
	$schedule{ "NagiosStop"} = -1;
	$schedule{ "NagiosStart"} = -1;
	$schedule{ "ServiceFail"} = -1;
	$schedule{ "ServiceRecover"} = -1;

	# Determine the amount of time to discover a service failure above
	# the normal check results reaper frequency
	my $maxDiscoveryTime = maxDiscoveryTime( $cfg, $service, $versionMajor);

	# Get the current time, to be used for all future times
	my $currentTime = time;

	# Last event time
	my $lastEventTime;

	if( $permutation->[ 0] eq "EntryTime") {
		$schedule{ "DowntimeStart"} = 10;
		$lastEventTime = $schedule{ "DowntimeStart"};
	}
	elsif( $permutation->[ 0] eq "DowntimeStart") {
		$schedule{ "DowntimeStart"} = -10;
		$lastEventTime = $schedule{ "DowntimeStart"} + 
				( $versionMajor < 4 ? 
				$cfg->{ "check_result_reaper_frequency"} : 0) + 2 ;
	}
	else {
		die "Unsupported permutation order: " . $permutation->[ 0] . " => " .
				$permutation->[ 1];
	}

	print join( " => ", @$permutation) . "\n";
	printf( "    Downtime Start: %s\n", $schedule{ "DowntimeStart"});
	# Iterate through the remaining events, determining the time for each	
	for( my $x = 0; $x < @$permutation; $x++) {
		if( $permutation->[ $x] eq "DowntimeEnd") {
			$schedule{ "DowntimeEnd"} = $lastEventTime + 
					( $cfg->{ "status_update_interval"} * 
					$cfg->{ "StatusUpdateIntervalMultiplier"});
			$lastEventTime = $schedule{ "DowntimeEnd"};
			printf( "    Downtime End: %s\n", $schedule{ "DowntimeEnd"});
		}
		elsif( $permutation->[ $x] eq "DowntimeStart") {
			; # Do nothing - Downtime Start was determined earlier
		}
		elsif( $permutation->[ $x] eq "DurationEnd") {
			$schedule{ "DowntimeDuration"} = ( $lastEventTime - 
					$schedule{ "DowntimeStart"}) + 
					( $cfg->{ "status_update_interval"} * 
					$cfg->{ "StatusUpdateIntervalMultiplier"} + 
					$maxDiscoveryTime);
			$lastEventTime = $schedule{ "DowntimeDuration"} + 
					$schedule{ "DowntimeStart"};
			printf( "    Duration: %d\n", $schedule{ "DowntimeDuration"});
		}
		elsif( $permutation->[ $x] eq "EntryTime") {
			; # Do nothing
		}
		elsif( $permutation->[ $x] eq "NagiosStop") {
			$schedule{ "NagiosStop"} = $lastEventTime + 
					( $cfg->{ "status_update_interval"} * 
					$cfg->{ "StatusUpdateIntervalMultiplier"});
			$lastEventTime = $schedule{ "NagiosStop"};
			printf( "    Nagios Stop: %s\n", $schedule{ "NagiosStop"});
		}
		elsif( $permutation->[ $x] eq "NagiosStart") {
			$schedule{ "NagiosStart"} = $lastEventTime + 
					( $cfg->{ "status_update_interval"} * 
					$cfg->{ "StatusUpdateIntervalMultiplier"});
			$lastEventTime = $schedule{ "NagiosStart"};
			printf( "    Nagios Start: %s\n", $schedule{ "NagiosStart"});
		}
		elsif( $permutation->[ $x] eq "ServiceFail") {
			$schedule{ "ServiceFail"} = $lastEventTime + 
					( $cfg->{ "status_update_interval"} * 
					$cfg->{ "StatusUpdateIntervalMultiplier"});
			$lastEventTime = $schedule{ "ServiceFail"} + 
					( $versionMajor < 4 ? 
					$cfg->{ "check_result_reaper_frequency"} : 0) + 
					$maxDiscoveryTime;
			printf( "    Service Fail: %s\n", $schedule{ "ServiceFail"});
		}
		elsif( $permutation->[ $x] eq "ServiceRecover") {
			$schedule{ "ServiceRecover"} = $lastEventTime + 
					( $cfg->{ "status_update_interval"} * 
					$cfg->{ "StatusUpdateIntervalMultiplier"});
			$lastEventTime = $schedule{ "ServiceRecover"} +
					( $versionMajor < 4 ? 
					$cfg->{ "check_result_reaper_frequency"} : 0) + 
					$maxDiscoveryTime;
			printf( "    Service Recover: %s\n", $schedule{ "ServiceRecover"});
		}
		else {
			die "Unhandled event type: " . $permutation->[ $x];
		}
	}

	return \%schedule;
}

# Generate a list of valid permutations for fixed downtime events
sub fixedDowntimeTestPermutations {
	my $service = shift;

	my @allPermutations;

	my @noRestart = ( "EntryTime", "DowntimeStart", "DowntimeEnd");
	my $iteratorNoRestart= Algorithm::Permute->new ( \@noRestart);
	while (my @perm1 = $iteratorNoRestart->next) {
		if( isValidEventPermutation( \@perm1, 
				$service->{ "active_checks_enabled"})) {
#			print "(@perm1)\n";
			push( @allPermutations, \@perm1);
		}
	}

	my @withRestart = ( "EntryTime", "DowntimeStart", "NagiosStop",
			"NagiosStart", "DowntimeEnd");
	my $iteratorWithRestart = 
		Algorithm::Permute->new ( \@withRestart);
	while (my @perm2 = $iteratorWithRestart->next) {
		if( isValidEventPermutation( \@perm2, 
				$service->{ "active_checks_enabled"})) {
#			print "(@perm2)\n";
			push( @allPermutations, \@perm2);
		}
	}

	return \@allPermutations;

}

# Generate a list of valid permutations for flexible downtime events
sub flexibleDowntimeTestPermutations {
	my $service = shift;

	my @allPermutations;

	my @noRestartOrTrigger = ( "EntryTime", "DowntimeStart", "DurationEnd", 
			"DowntimeEnd");
	my $iteratorNoRestartOrTrigger = 
			Algorithm::Permute->new ( \@noRestartOrTrigger );
	while (my @perm1 = $iteratorNoRestartOrTrigger->next) {
		if( isValidEventPermutation( \@perm1, 
				$service->{ "active_checks_enabled"})) {
			push( @allPermutations, \@perm1);
		}
	}

	my @noRestart = ( "EntryTime", "DowntimeStart", "DurationEnd", 
			"DowntimeEnd", "ServiceFail", "ServiceRecover");
	my $iteratorNoRestart = Algorithm::Permute->new ( \@noRestart );
	while (my @perm2 = $iteratorNoRestart->next) {
		if( isValidEventPermutation( \@perm2, 
				$service->{ "active_checks_enabled"})) {
			push( @allPermutations, \@perm2);
		}
	}

	my @noTrigger = ( "EntryTime", "DowntimeStart", "DurationEnd", 
			"DowntimeEnd", "NagiosStop", "NagiosStart");
	my $iteratorNoTrigger = Algorithm::Permute->new ( \@noTrigger );
	while (my @perm3 = $iteratorNoTrigger->next) {
		if( isValidEventPermutation( \@perm3, 
				$service->{ "active_checks_enabled"})) {
			push( @allPermutations, \@perm3);
		}
	}

	my @allEvents = ( "EntryTime", "DowntimeStart", "DurationEnd", 
			"DowntimeEnd", "NagiosStop", "NagiosStart", "ServiceFail", 
			"ServiceRecover");
	my $iteratorAllEventns = Algorithm::Permute->new ( \@allEvents );
	while (my @perm4 = $iteratorAllEventns->next) {
		if( isValidEventPermutation( \@perm4, 
				$service->{ "active_checks_enabled"})) {
			push( @allPermutations, \@perm4);
		}
	}

	return \@allPermutations;
}

# This function verifies the global Nagios configuration
sub verifyConfig {

	my $cfg = shift;
	my $versionMajor = shift;

	# Verify the status update interval
	if( $cfg->{ "status_update_interval"} < 5) {
		print STDERR sprintf( "status_update_interval (%d) is too short\n",
				$cfg->{ "status_update_interval"});
		return 0;
	}
	if( $cfg->{ "status_update_interval"} >= 10) {
		print sprintf( "The status_update_interval (%d) is long. " .
				"It should optimally be 5 seconds.\n", 
				$cfg->{ "status_update_interval"});
		print "Press ENTER to continue...";
		<STDIN>;
	}

	if( $versionMajor < 4) {
		# Verify the check_result_reaper_frequency
		if( $cfg->{ "check_result_reaper_frequency"} < 5) {
			print STDERR sprintf( "check_result_reaper_frequency (%d) is too " .
					"short\n", $cfg->{ "check_result_reaper_frequency"});
			return 0;
		}
		if( $cfg->{ "check_result_reaper_frequency"} >= 10) {
			print sprintf( "The check_result_reaper_frequency (%d) is long. " .
					"It should optimally be 5 seconds.\n", 
					$cfg->{ "check_result_reaper_frequency"});
			print "Press ENTER to continue...";
			<STDIN>;
		}
	}

	return 1;
}

sub verifyService {
	my $cfg = shift;
	my $hostName = shift;
	my $serviceDescription = shift;

	# Make sure there is an appropriately configured service before we 
	# start testing.
	my $objects = readObjectCache( $cfg->{ "object_cache_file"});
	my $service = findService( $objects, $hostName, $serviceDescription);
	if( !defined( $service)) {
		print STDERR sprintf( "No service found for host '%s' and " .
				"service description '%s'.\n", $hostName, $serviceDescription);
		return undef;
	}
	if( !( $service->{ "active_checks_enabled"} || 
			$service->{ "passive_checks_enabled"})) {
		print STDERR sprintf( "Either active or passive checks must be " .
				"enabled for %s:%s\n", $hostName, $serviceDescription);
		return undef;
	}
	if( $service->{ "notifications_enabled"}) {
		print STDERR sprintf( "Notifications must not be enabled for %s:%s\n",
				$hostName, $serviceDescription);
		return undef;
	}
	if( $service->{ "flap_detection_enabled"}) {
		print STDERR sprintf( "Flap detection must not be enabled for %s:%s\n",
				$hostName, $serviceDescription);
		return undef;
	}
	if( $service->{ "process_perf_data"}) {
		print STDERR sprintf( "Performance data processing must not be " .
				"enabled for %s:%s\n", $hostName, $serviceDescription);
		return undef;
	}
	if( $service->{ "active_checks_enabled"}) {
		if( $service->{ "max_check_attempts"} > 1) {
			print sprintf( "The max_check_attempts (%d) is high. " .
					"It should optimally be 1.\n", 
					$cfg->{ "max_check_attempts"});
			print "Press ENTER to continue...";
			<STDIN>;
		}
		if( $service->{ "check_interval"} > 1) {
			print sprintf( "The normal_check_interval (%d) is long. " .
					"It should optimally be 1 minute.\n", 
					$cfg->{ "normal_interval"});
			print "Press ENTER to continue...";
			<STDIN>;
		}
		if(( $service->{ "max_check_attempts"} > 1) && 
				( $service->{ "retry_interval"} > 1)) {
			print sprintf( "The retry_check_interval (%d) is long. " .
					"It should optimally be 1 minute.\n", 
					$cfg->{ "retry_interval"});
			print "Press ENTER to continue...";
			<STDIN>;
		}
	}
	else {
		if( $service->{ "check_freshness"}) {
			print STDERR sprintf( "check_freshness must not be enabled " .
					"for %s:%s\n", $hostName, $serviceDescription);
			return undef;
		}
		if( $service->{ "max_check_attempts"} != 1) {
			print STDERR sprintf( "max_check_attempts must be 1 for %s:%s\n",
					$hostName, $serviceDescription);
			return undef;
		}
	}
	return $service;
}

sub getVersion {
	my $cfg = shift;

	my $status = readDatFile( $cfg->{ "status_file"});
	my @versionparts = split( /\./, $status->{ "info"}->{ "version"});
	my %version;
	$version{ "major"} = $versionparts[ 0];
	$version{ "minor"} = $versionparts[ 1];
	$version{ "micro"} = $versionparts[ 2];

	return \%version;
}

sub isInArray {
	my $string = shift;
	my $array = shift;

	for( my $x = 0; $x < @$array; $x++) {
		return 1 if( $array->[ $x] eq $string);
	}
	return 0;
}

# This is the SIGUSR1 handler - it sets a flag that causes the test to 
# pause.
sub pauseTest {
	$testPaused = 1; 
}

sub usage {
	my $cmd = shift;

	print <<EOU;
This command runs one or more downtime checks. With only the required 
options, the test will run all appropriate permutations of downtime 
event sequences (see --single-run below for a list of events).

The service checked must be an existing service and can be either an 
actively or passively checked service. Note that this test can run a 
very long time, over 24 hours for active checks.

Sending the program the USR1 single will cause it to pause after the
current permutation has completed testing.

Usage: $cmd --host-name <host> --service-description <service>
       [--single-run <test>] [--no-fixed-tests] [--no-flexible-tests]
       [--skip-logged <logfile>] [--help|-?]

Where:
    --host-name <host> specifies the name of the host whose service 
        is to be used for the downtime check.
    --service-description <service> specifies the name of the service 
        to be used for the downtime check.
    --single-run <test> indicates that the single specified test is 
        to be run.  <test> must be of the form "(Fixed|Flexible) 
        downtime: <eventlist>" <eventlist> is a comma and space 
        separated list of the following events: EntryTime, 
        DowntimeStart, [NagiosStop, NagiosStart,] [ServiceFail, 
        ServiceRecover,] DurationEnd, DowntimeEnd. The events within 
        the brackets are optional, but if one of the pair is used, 
        both must be used. Hint: the format of <test> is the same as 
        is displayed at the beginning of each downtime check when 
        --single-run is not specified.
    --no-fixed-tests indicates that no fixed tests should be run. 
        This option is ignored is --single-run is specified.
    --no-flexible-tests indicates that no flexible tests should be 
        run.  This option is ignored is --single-run is specified.
    --skip-logged <logfile> indicates that any tests logged in 
        <logfile> should be skipped. This is useful for start over 
        where you left off if the test had been aborted prematurely.
    --help|-? displays this help message.
EOU
}

# Host and service to be used for testing
my $hostName = "localhost";
my $serviceDescription = "Downtime Test";
my $singleRun = undef;
my $logFile = undef;
my $noFixedTests = 0;
my $noFlexibleTests = 0;
my $showUsage = 0;

$result = GetOptions (	"host-name=s"			=> \$hostName,
						"service-description=s"	=> \$serviceDescription,
						"single-run=s"			=> \$singleRun,
						"no-fixed-tests"		=> \$noFixedTests,
						"no-flexible-tests"		=> \$noFlexibleTests,
						"skip-logged=s"			=> \$logFile,
						"help|?"				=> \$showUsage);

die "Error processing command line options" unless( $result);

if( $showUsage) {
	usage( $PROGRAM_NAME);
	exit( 0);
}

# Install signal handle to enable test pausing
$SIG{ "USR1"} = "pauseTest";

# Make sure we're running as root
if( $EUID != 0) {
	print STDERR "This script must be run as root.\n";
	exit 1;
}

# Make sure Nagios is running
my $status = `/sbin/service nagios status`;
if( $status !~ /nagios \(pid( \d+)+\) is running.../) {
	print STDERR "Nagios must be running in order to run this test.\n";
	exit 1;
}

# Read the Nagios configuration
my $cfg = readCfgFile( "/usr/local/nagios/etc/nagios.cfg");
exit 1 unless( defined( $cfg));

# Get the version
my $version = getVersion( $cfg);
printf( "Nagios version is %d.%d.%d\n", $version->{ "major"}, 
		$version->{ "minor"}, $version->{ "micro"});

# Verify the global configuration
unless( verifyConfig( $cfg, $version->{ "major"}) > 0) {
	exit 1;
}
$cfg->{ "StatusUpdateIntervalMultiplier"} = 4;

# Verify the service configuration
my $service = verifyService( $cfg, $hostName, $serviceDescription);
exit 1 unless( defined( $service));

# Enable autoflush on STDOUT so updates occur
my $old_fh = select(STDOUT);
$| = 1;
select($old_fh); 

my $activeChecks = 0;
my $permutationTime = 0;
my $elapsedTime = 0;
my $schedule;
my $comment;

#my @testPerm = ( "DowntimeStart", "DurationEnd", "EntryTime", "ServiceFail", "NagiosStop", "DowntimeEnd", "NagiosStart", "ServiceRecover");
#my $testSchedule = scheduleDowntimePermutation( $service, $cfg, \@testPerm, 
#		$version->{ "major"});
#my $testComment = join( ", ", @testPerm);
#checkDowntime( "Flexible downtime: $testComment", $service, $cfg, 0, 
#		$testSchedule, $version->{ "major"});
#exit( 0);

if( defined( $singleRun)) {
	if( $singleRun =~ /^(Fixed|Flexible) downtime: (.*)/) {
		my $downtimeType = $1;
		my @singleRunPerm = split( /, /, $2);
		my $fixed = (( $singleRun =~ /^Fixed/) ? 1 : 0);
		my $testSchedule = scheduleDowntimePermutation( $service, $cfg, 
				\@singleRunPerm, $version->{ "major"});
		my $testComment = join( ", ", @singleRunPerm);
		checkDowntime( "$downtimeType downtime: $testComment", $service, $cfg, 
				$fixed, $testSchedule, $version->{ "major"});
		exit 0;
	}
	else {
		die "Unrecognized single run format: $singleRun";
	}
}
else {
	my @skipTests = ();
	if( defined( $logFile)) {
		open( LOG, "$logFile") || 
				die "Unable to open log file $logFile for reading";
		while( <LOG>) {
			chomp;
			push( @skipTests, $_) if( /^(Fixed|Flexible) downtime: /);
		}
		close( LOG) || die "Unable to close $logFile";
	}

	my $lastTriedPermutation = -1;
	unless( $noFixedTests) {
		my $fixedDowntimeTests = fixedDowntimeTestPermutations( $service);
		for( my $x = 0; $x < @$fixedDowntimeTests; $x++) {
			if( $testPaused) {
				print "Testing paused as requested.\n";
				print "Press ENTER to continue...";
				<STDIN>;
				$testPaused = 0;
			}
			$comment = join( ", ", @{ $fixedDowntimeTests->[ $x]});
			unless( isInArray( "Fixed downtime: $comment", \@skipTests)) {
				printf( "Fixed Test %d of %d\n", $x + 1, 
						scalar( @$fixedDowntimeTests));
				$schedule = scheduleDowntimePermutation( $service, $cfg, 
						$fixedDowntimeTests->[ $x], $version->{ "major"});
				$permutationTime = checkDowntime( "Fixed downtime: $comment", 
						$service, $cfg, 1, $schedule, $version->{ "major"});
				if( $permutationTime == -1) {
					if( $lastTriedPermutation != $x) {
						print "Downtime test failed: retrying\n";
						$lastTriedPermutation = $x;
						$x--;
					}
					else {
						die "Downtime test failed after retry\n";
					}
				}
				else {
					$lastTriedPermutation = $x;
					$elapsedTime += $permutationTime;
					printf( "Last test time: %s\n", 
							humanDuration( $permutationTime));
					printf( "Elapsed time thus far: %s\n", 
							humanDuration( $elapsedTime));
				}
			}
			else {
				$lastTriedPermutation = $x;
				printf( "Skipping fixed downtime: %s\n", $comment);
			}
		}
	}
	
	$lastTriedPermutation = -1;
	unless( $noFlexibleTests) {
		my $flexibleDowntimeTests = flexibleDowntimeTestPermutations( $service);
		for( my $y = 0; $y < @$flexibleDowntimeTests; $y++) {
			if( $testPaused) {
				print "Testing paused as requested.\n";
				print "Press ENTER to continue...";
				<STDIN>;
				$testPaused = 0;
			}
			$comment = join( ", ", @{ $flexibleDowntimeTests->[ $y]});
			unless( isInArray( "Flexible downtime: $comment", \@skipTests)) {
				printf( "Flexible Test %d of %d\n", $y + 1, 
						scalar( @$flexibleDowntimeTests));
				$schedule = scheduleDowntimePermutation( $service, $cfg, 
						$flexibleDowntimeTests->[ $y], $version->{ "major"});
				$permutationTime = checkDowntime( "Flexible downtime: $comment", 
						$service, $cfg, 0, $schedule, $version->{ "major"});
				if( $permutationTime == -1) {
					if( $lastTriedPermutation != $y) {
						print "Downtime test failed: retrying\n";
						$lastTriedPermutation = $y;
						$y--;
					}
					else {
						die "Downtime test failed after retry\n";
					}
				}
				else {
					$lastTriedPermutation = $y;
					$elapsedTime += $permutationTime;
					printf( "Last test time: %s\n", 
							humanDuration( $permutationTime));
					printf( "Elapsed time thus far: %s\n", 
							humanDuration( $elapsedTime));
				}
			}
			else {
				$lastTriedPermutation = $y;
				printf( "Skipping flexible downtime: %s\n", $comment);
			}
		}
	}
}
