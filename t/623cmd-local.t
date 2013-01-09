#!/usr/bin/perl
# 
# Local Checks for cmd.cgi

use warnings;
use strict;
use Test::More qw(no_plan);
use FindBin qw($Bin);

chdir $Bin or die "Cannot chdir";

my $topdir = "$Bin/..";
my $cgi_dir = "$topdir/cgi";
my $local_cgi = "$cgi_dir/cmd.cgi";

my $output;

my $cmd_typ = '';
my $remote_user = 'REMOTE_USER=nagiosadmin';



$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET $local_cgi`;
like( $output, "/<P><DIV CLASS='errorMessage'>Error: No command was specified</DIV></P>/", "$local_cgi without params shows an error" );


# Run many tests against commands which are not supportet by cmd.cgi
for (8, 18, 19, 31, 32, 53, 54, 69..77, 97, 98, 103..108, 115..120, 123..158, 161..169 ){
	$cmd_typ=$_;
	$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
	unlike( $output, "/You are requesting to execute an unknown command. Shame on you!/", "$local_cgi with cmd_typ=$cmd_typ results in an error" );
}

# Tests against command type '1'
$cmd_typ=1;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to add a host comment/", "$local_cgi with cmd_typ=$cmd_typ shows host comment form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Author \\(Your Name\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Author Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '2'
$cmd_typ=2;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to delete a host comment/", "$local_cgi with cmd_typ=$cmd_typ shows host comment form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment ID:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment ID field" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '3'
$cmd_typ=3;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to add a service comment/", "$local_cgi with cmd_typ=$cmd_typ shows service comment form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Author \\(Your Name\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Author Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '4'
$cmd_typ=4;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to delete a service comment/", "$local_cgi with cmd_typ=$cmd_typ shows service comment delete form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment ID:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment ID field" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '5'
$cmd_typ=5;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable active checks of a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows enable active service checks form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '6'
$cmd_typ=6;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable active checks of a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows disable active service checks form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '7'
$cmd_typ=7;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to schedule a service check/", "$local_cgi with cmd_typ=$cmd_typ shows request service check form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Check Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Check Time in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '9'
$cmd_typ=9;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to delay a service notification/", "$local_cgi with cmd_typ=$cmd_typ shows request to delay service notification" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Notification Delay \\(minutes from now\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Notification Delay form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '10'
$cmd_typ=10;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to delay a host notification/", "$local_cgi with cmd_typ=$cmd_typ shows request to delay host notification" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Notification Delay \\(minutes from now\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Notification Delay form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '11'
$cmd_typ=11;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable notification/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable notification" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '12'
$cmd_typ=12;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable notification/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable notification" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '13'
$cmd_typ=13;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to restart the Nagios process/", "$local_cgi with cmd_typ=$cmd_typ shows request to restart Nagios" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '14'
$cmd_typ=14;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to shutdown the Nagios process/", "$local_cgi with cmd_typ=$cmd_typ shows request to shutdown Nagios" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '15'
$cmd_typ=15;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable active checks of all services on a host/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable active checks of all services on a host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '16'
$cmd_typ=16;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable active checks of all services on a host/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable active checks of all services on a host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '17'
$cmd_typ=17;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to schedule a check of all services for a host/", "$local_cgi with cmd_typ=$cmd_typ shows request check all services on a host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Check Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Check Time in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );



# Tests against command type '20'
$cmd_typ=20;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to delete all comments for a host/", "$local_cgi with cmd_typ=$cmd_typ shows request to delete all comments for a host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '21'
$cmd_typ=21;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to delete all comments for a service/", "$local_cgi with cmd_typ=$cmd_typ shows request to delete all comments for a service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '22'
$cmd_typ=22;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable notifications for a service/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable notifications for a service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '23'
$cmd_typ=23;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable notifications for a service/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable notifications for a service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '24'
$cmd_typ=24;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable notifications for a host/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable notifications for a host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '25'
$cmd_typ=25;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable notifications for a host/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable notifications for a host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '26'
$cmd_typ=26;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable notifications for all hosts and services beyond a host/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable notifications for all hosts and services beyond a host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '27'
$cmd_typ=27;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable notifications for all hosts and services beyond a host/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable notifications for all hosts and services beyond a host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '28'
$cmd_typ=28;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable notifications for all services on a host/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable notifications for all services on a host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '29'
$cmd_typ=29;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable notifications for all services on a host/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable notifications for all services on a host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '30'
$cmd_typ=30;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to submit a passive check result for a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows request to submit passive check result for a particular service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Check Result:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Check Result in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Check Output:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Check Output in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );



# Tests against command type '33'
$cmd_typ=33;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to acknowledge a host problem/", "$local_cgi with cmd_typ=$cmd_typ shows request to acknowledge a host problem" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Author \\(Your Name\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Author in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment in form" );
like( $output, "/<td CLASS='optBoxItem'>Sticky Acknowledgement:</td>/", "$local_cgi with cmd_typ=$cmd_typ allows Sticky Acknowledge in form" );
like( $output, "/<td CLASS='optBoxItem'>Send Notification:</td>/", "$local_cgi with cmd_typ=$cmd_typ allows to send notification in form" );
like( $output, "/<td CLASS='optBoxItem'>Persistent Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ allows to set Persistent Comment in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '34'
$cmd_typ=34;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to acknowledge a service problem/", "$local_cgi with cmd_typ=$cmd_typ shows request to acknowledge a service problem" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Author \\(Your Name\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Author in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment in form" );
like( $output, "/<td CLASS='optBoxItem'>Sticky Acknowledgement:</td>/", "$local_cgi with cmd_typ=$cmd_typ allows Sticky Acknowledge in form" );
like( $output, "/<td CLASS='optBoxItem'>Send Notification:</td>/", "$local_cgi with cmd_typ=$cmd_typ allows to send notification in form" );
like( $output, "/<td CLASS='optBoxItem'>Persistent Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ allows to set Persistent Comment in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '35'
$cmd_typ=35;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to start executing active service checks/", "$local_cgi with cmd_typ=$cmd_typ shows request to start executing system wide active service checks" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '36'
$cmd_typ=36;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to stop executing active service checks/", "$local_cgi with cmd_typ=$cmd_typ shows request to stop executing system wide active service checks" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '37'
$cmd_typ=37;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to start accepting passive service checks/", "$local_cgi with cmd_typ=$cmd_typ shows request to start accepting executing system wide passive service checks" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '38'
$cmd_typ=38;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to stop accepting passive service checks/", "$local_cgi with cmd_typ=$cmd_typ shows request to stop accepting system wide passive service checks" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '39'
$cmd_typ=39;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to start accepting passive service checks for a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows request to start accepting passive service checks for a particular service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '40'
$cmd_typ=40;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to stop accepting passive service checks for a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows request to stop accepting passive service checks for a particular service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '41'
$cmd_typ=41;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable event handlers/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable system wide event handlers" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '42'
$cmd_typ=42;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable event handlers/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable system wide event handlers" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '43'
$cmd_typ=43;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable the event handler for a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable event handler of a particular host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '44'
$cmd_typ=44;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable the event handler for a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable event handler of a particular host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '45'
$cmd_typ=45;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable the event handler for a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable event handler of  a particular service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '46'
$cmd_typ=46;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable the event handler for a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable event handler   of a particular service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '47'
$cmd_typ=47;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable active checks of a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable active checks of a particular host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '48'
$cmd_typ=48;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable active checks of a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable active checks of a particular host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '49'
$cmd_typ=49;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to start obsessing over service checks/", "$local_cgi with cmd_typ=$cmd_typ shows request to start obsessing over service checks" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '50'
$cmd_typ=50;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to stop obsessing over service checks/", "$local_cgi with cmd_typ=$cmd_typ shows request to stop obsessing over service checks" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '51'
$cmd_typ=51;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to remove a host acknowledgement/", "$local_cgi with cmd_typ=$cmd_typ shows request to remove a host acknowledgement" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );

# Tests against command type '52'
$cmd_typ=52;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to remove a service acknowledgement/", "$local_cgi with cmd_typ=$cmd_typ shows request to remove a service acknowledgement" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '55'
$cmd_typ=55;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to schedule downtime for a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to schedule downtime for a particular Host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Author \\(Your Name\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Author Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Start Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Start Time in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>End Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires End Time in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '56'
$cmd_typ=56;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to schedule downtime for a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows request to schedule downtime for a particular Service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Author \\(Your Name\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Author Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Start Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Start Time in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>End Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires End Time in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '57'
$cmd_typ=57;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable flap detection for a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable flap detection for a particular host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '58'
$cmd_typ=58;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable flap detection for a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable flap detection for a particular host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '59'
$cmd_typ=59;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable flap detection for a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable flap detection for a particular service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '60'
$cmd_typ=60;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable flap detection for a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable flap detection for a particular service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '61'
$cmd_typ=61;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable flap detection for hosts and services/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable flap detection for hosts and services" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '62'
$cmd_typ=62;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable flap detection for hosts and services/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable flap detection for hosts and services" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '63'
$cmd_typ=63;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable notifications for all services in a particular hostgroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable notifications for all services in a particular hostgroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Hostgroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Hostgroup Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '64'
$cmd_typ=64;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable notifications for all services in a particular hostgroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable notifications for all services in a particular hostgroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Hostgroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Hostgroup Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '65'
$cmd_typ=65;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable notifications for all hosts in a particular hostgroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable notifications for all hosts in a particular hostgroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Hostgroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Hostgroup Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '66'
$cmd_typ=66;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable notifications for all hosts in a particular hostgroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable notifications for all hosts in a particular hostgroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Hostgroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Hostgroup Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '67'
$cmd_typ=67;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable active checks of all services in a particular hostgroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable active checks of all services in a particular hostgroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Hostgroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Hostgroup Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '68'
$cmd_typ=68;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable active checks of all services in a particular hostgroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable active checks of all services in a particular hostgroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Hostgroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Hostgroup Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '78'
$cmd_typ=78;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to cancel scheduled downtime for a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to cancel scheduled downtime for a particular host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Scheduled Downtime ID:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Scheduled Downtime ID in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '79'
$cmd_typ=79;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to cancel scheduled downtime for a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows request to cancel scheduled downtime for a particular service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Scheduled Downtime ID:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Scheduled Downtime ID in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '82'
$cmd_typ=82;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable performance data processing for hosts and services/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable performance data processing for hosts and services" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '83'
$cmd_typ=83;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable performance data processing for hosts and services/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable performance data processing for hosts and services" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '84'
$cmd_typ=84;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to schedule downtime for all hosts in a particular hostgroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to schedule downtime for all hosts in a particular hostgroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Hostgroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Hostgroup Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Author \\(Your Name\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Author Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Start Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Start Time in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>End Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires End Time in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '85'
$cmd_typ=85;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to schedule downtime for all services in a particular hostgroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to schedule downtime for all services in a particular hostgroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Hostgroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Hostgroup Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Author \\(Your Name\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Author Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Start Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Start Time in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>End Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires End Time in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '86'
$cmd_typ=86;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to schedule downtime for all services for a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to schedule downtime for all services for a particular host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Author \\(Your Name\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Author Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Start Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Start Time in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>End Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires End Time in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '87'
$cmd_typ=87;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to submit a passive check result for a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to submit a passive check result for a particular host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Check Result:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Check Result in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Check Output:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Check Output in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '88'
$cmd_typ=88;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to start executing host checks/", "$local_cgi with cmd_typ=$cmd_typ shows request to start executing host checks" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '89'
$cmd_typ=89;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to stop executing host checks/", "$local_cgi with cmd_typ=$cmd_typ shows request to stop executing host checks" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '90'
$cmd_typ=90;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to start accepting passive host checks/", "$local_cgi with cmd_typ=$cmd_typ shows request to start accepting passive host checks" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '91'
$cmd_typ=91;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to stop accepting passive host checks/", "$local_cgi with cmd_typ=$cmd_typ shows request to stop accepting passive host checks" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '92'
$cmd_typ=92;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to start accepting passive checks for a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to start accepting passive checks for a particular host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '93'
$cmd_typ=93;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to stop accepting passive checks for a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to stop accepting passive checks for a particular host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '94'
$cmd_typ=94;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to start obsessing over host checks/", "$local_cgi with cmd_typ=$cmd_typ shows request to start obsessing over host checks" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '95'
$cmd_typ=95;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to stop obsessing over host checks/", "$local_cgi with cmd_typ=$cmd_typ shows request to stop obsessing over host checks" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '96'
$cmd_typ=96;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to schedule a host check/", "$local_cgi with cmd_typ=$cmd_typ shows request to schedule a host check" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Check Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Check Time in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '99'
$cmd_typ=99;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to start obsessing over a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows request to start obsessing over a particular service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '100'
$cmd_typ=100;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to stop obsessing over a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows request to stop obsessing over a particular service" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '101'
$cmd_typ=101;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to start obsessing over a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to start obsessing over a particular host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '102'
$cmd_typ=102;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to stop obsessing over a particular host/", "$local_cgi with cmd_typ=$cmd_typ shows request to stop obsessing over a particular host" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '109'
$cmd_typ=109;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable notifications for all services in a particular servicegroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable notifications for all services in a particular servicegroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Servicegroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Servicegroup Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '110'
$cmd_typ=110;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable notifications for all services in a particular servicegroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable notifications for all services in a particular servicegroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Servicegroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Servicegroup Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '111'
$cmd_typ=111;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable notifications for all hosts in a particular servicegroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable notifications for all hosts in a particular servicegroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Servicegroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Servicegroup Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '112'
$cmd_typ=112;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable notifications for all hosts in a particular servicegroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable notifications for all hosts in a particular servicegroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Servicegroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Servicegroup Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '113'
$cmd_typ=113;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to enable active checks of all services in a particular servicegroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to enable active checks of all services in a particular servicegroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Servicegroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Servicegroup Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '114'
$cmd_typ=114;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable active checks of all services in a particular servicegroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to disable active checks of all services in a particular servicegroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Servicegroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Servicegroup Name in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '121'
$cmd_typ=121;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to schedule downtime for all hosts in a particular servicegroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to schedule downtime for all hosts in a particular servicegroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Servicegroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Servicegroup Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Start Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Start Time in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>End Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires End Time in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Author \\(Your Name\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Author Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '122'
$cmd_typ=122;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to schedule downtime for all services in a particular servicegroup/", "$local_cgi with cmd_typ=$cmd_typ shows request to schedule downtime for all services in a particular servicegroup" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Servicegroup Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Servicegroup Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Start Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Start Time in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>End Time:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires End Time in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Author \\(Your Name\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Author Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '159'
$cmd_typ=159;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to send a custom host notification/", "$local_cgi with cmd_typ=$cmd_typ shows request to send a custom host notification" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Author \\(Your Name\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Author Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );


# Tests against command type '160'
$cmd_typ=160;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to send a custom service notification/", "$local_cgi with cmd_typ=$cmd_typ shows request to send a custom service notification" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Author \\(Your Name\\):</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Author Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Comment:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Comment in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );



