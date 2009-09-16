#!/usr/bin/perl
# 
# Local Checks for cmd.cgi

use warnings;
use strict;
use Test::More;
use FindBin qw($Bin);

chdir $Bin or die "Cannot chdir";

my $topdir = "$Bin/..";
my $cgi_dir = "$topdir/cgi";
my $local_cgi = "$cgi_dir/cmd.cgi";

my $output;

my $cmd_typ = '';
my $remote_user = 'REMOTE_USER=nagiosadmin';

plan tests => 110;

$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET $local_cgi`;
like( $output, "/<P><DIV CLASS='errorMessage'>Error: No command was specified</DIV></P>/", "$local_cgi without params shows an error" );


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
like( $output, "/You are requesting to enable actice checks of a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows enable active service checks form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Host Name:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Host Name in form" );
like( $output, "/<td CLASS='optBoxRequiredItem'>Service:</td>/", "$local_cgi with cmd_typ=$cmd_typ requires Service in form" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );

# Tests against command type '6'
$cmd_typ=6;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to disable actice checks of a particular service/", "$local_cgi with cmd_typ=$cmd_typ shows disable active service checks form" );
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

# Tests against command type '8'
$cmd_typ=8;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
unlike( $output, "/You are requesting to execute an unknown command. Shame on you!/", "$local_cgi with cmd_typ=$cmd_typ results in an error" );

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
like( $output, "/You are requesting to restart the Nagios process/", "$local_cgi with cmd_typ=$cmd_typ shows request to restart nagios" );
unlike( $output, "/Sorry, but no information is available for this command./", "$local_cgi with cmd_typ=$cmd_typ has a command description" );

# Tests against command type '14'
$cmd_typ=14;
$output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
like( $output, "/You are requesting to shutdown the Nagios process/", "$local_cgi with cmd_typ=$cmd_typ shows request to shutdown nagios" );
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

# Tests against command type '18'
for (18..19){
  $cmd_typ=$_;
  $output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
  unlike( $output, "/You are requesting to execute an unknown command. Shame on you!/", "$local_cgi with cmd_typ=$cmd_typ results in an error" );
}

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

# Tests against command type '31..32'
for (31..32){
  $cmd_typ=$_;
  $output = `NAGIOS_CGI_CONFIG=etc/cgi.cfg $remote_user REQUEST_METHOD=GET QUERY_STRING='cmd_typ=$cmd_typ' $local_cgi`;
  unlike( $output, "/You are requesting to execute an unknown command. Shame on you!/", "$local_cgi with cmd_typ=$cmd_typ results in an error" );
}

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


