#!/usr/bin/perl
#
# Integration tests for mcp.cgi, the Model Context Protocol server.
# Runs the CGI like a web server would (environment + request body on
# stdin) against the fixture configuration in etc/ and var/.

use warnings;
use strict;
use Test::More;
use JSON::PP;
use File::Temp qw(tempdir);
use Cwd ();
use File::Copy qw(copy);
use File::Path qw(make_path);

defined($ARGV[0]) or die "Usage: $0 <top build dir>";

my $top_builddir = shift @ARGV;
my $cgi_dir = "$top_builddir/cgi";
my $mcp = "$cgi_dir/mcp.cgi";
my $json = JSON::PP->new->canonical;

plan skip_all => "$mcp not built" unless -x $mcp;

# private cgi.cfg with a token file and an extra read-only user
my $tmp = tempdir(CLEANUP => 1);
# "nagios -v" drops privileges to nagios_user when run as root; let it read
chmod 0755, $tmp;
my $tokens = "$tmp/mcp-tokens.cfg";
my $cgi_cfg = "$tmp/cgi.cfg";
open(my $in, '<', 'etc/cgi.cfg') or die "etc/cgi.cfg: $!";
open(my $out, '>', $cgi_cfg) or die "$cgi_cfg: $!";
while(<$in>) {
	s/^(authorized_for_all_(hosts|services))=nagiosadmin$/$1=nagiosadmin,viewer/;
	print $out $_;
}
print $out "authorized_for_read_only=viewer\nmcp_token_file=$tokens\n";
close($in);
close($out);

# Object files for the configuration tools. The other tests read the
# object cache, so these only need to be a small but valid configuration
# ("nagios -v" must accept it); the fixture minimal.cfg is not.
make_path("$tmp/objects");
open(my $objs, '>', "$tmp/objects/minimal.cfg") or die;
print $objs <<'CFG';
# hand written test objects - keep this comment
define timeperiod {
	timeperiod_name	always
	alias	Always
	monday	00:00-24:00
}
define command {
	command_name	check_me
	command_line	/bin/true
}
define command {
	command_name	with_continuation_lines
	command_line	/bin/echo first \
			second
}
define contact {
	contact_name	nagiosadmin
	host_notifications_enabled	0
	service_notifications_enabled	0
	host_notification_period	always
	service_notification_period	always
	host_notification_options	n
	service_notification_options	n
	host_notification_commands	check_me
	service_notification_commands	check_me
}
define host {
	name	test-host
	check_command	check_me
	max_check_attempts	1
	check_period	always
	notification_interval	0
	notification_period	always
	contacts	nagiosadmin
	register	0
}
define host {
	host_name host1
	use	test-host
	alias	host1 test
	address	192.168.1.1
}
define host {
	host_name host2
	use	test-host
	alias	host2 test
	address	192.168.1.2
}
define service {
	name	test-service
	check_command	check_me
	max_check_attempts	1
	check_interval	5
	retry_interval	1
	check_period	always
	notification_interval	0
	notification_period	always
	contacts	nagiosadmin
	register	0
}
define service {
	use	test-service
	host_name	host1
	service_description	Dummy service
}
CFG
close($objs);

# private nagios.cfg with absolute paths: command_file is not resolved
# relative to the config directory. The external command "pipe" is a
# plain file here, so we can read back what was submitted.
my $cmd_file = "$tmp/nagios.cmd";
my $cwd = Cwd::getcwd();
open($in, '<', 'etc/nagios.cfg') or die "etc/nagios.cfg: $!";
open($out, '>', "$tmp/nagios.cfg") or die "$tmp/nagios.cfg: $!";
while(<$in>) {
	s{^cfg_file=minimal\.cfg}{cfg_file=$tmp/objects/minimal.cfg};
	s{^(cfg_file|resource_file)=(?!/)}{$1=$cwd/etc/};
	s{=\.\./var/}{=$cwd/var/};
	s{^command_file=.*}{command_file=$cmd_file};
	print $out $_;
}
close($in);
close($out);
{
	local @ARGV = ($cgi_cfg);
	local $^I = '';
	while(<>) {
		s{^main_config_file=.*}{main_config_file=$tmp/nagios.cfg};
		print;
	}
}
sub reset_cmd_file { open(my $f, '>', $cmd_file) or die "$cmd_file: $!"; close($f); }
sub last_command {
	open(my $f, '<', $cmd_file) or return '';
	local $/;
	my $c = <$f> // '';
	close($f);
	$c =~ s/^\[\d+\] //;
	$c =~ s/\n\z//;
	return $c;
}
reset_cmd_file();

my %base_env = (
	NAGIOS_CGI_CONFIG => $cgi_cfg,
	SCRIPT_FILENAME   => $mcp,
	PATH              => $ENV{PATH},
);

sub run_cli {
	my @args = @_;
	local %ENV = %base_env;
	my $cmd = join(' ', map { quotemeta } ($mcp, @args));
	return `$cmd 2>&1`;
}

# performs one HTTP request; returns (status, headers, body)
sub request {
	my (%opt) = @_;
	my $body = $opt{body} // '';
	my $bodyfile = "$tmp/body";
	open(my $b, '>', $bodyfile) or die;
	print $b $body;
	close($b);
	local %ENV = (%base_env,
		REQUEST_METHOD => $opt{method} // 'POST',
		CONTENT_LENGTH => length($body),
		CONTENT_TYPE   => 'application/json',
		%{ $opt{env} // {} });
	$ENV{HTTP_AUTHORIZATION} = "Bearer $opt{token}" if defined $opt{token};
	# a user name from the web server must never be trusted
	$ENV{REMOTE_USER} = 'nagiosadmin';
	my $raw = `$mcp < $bodyfile`;
	my ($head, $content) = split(/\r?\n\r?\n/, $raw, 2);
	my ($status) = $head =~ /^Status: (\d+)/m;
	return ($status, $head, $content);
}

my $next_id = 1;
sub rpc {
	my ($token, $method, $params) = @_;
	my $msg = { jsonrpc => '2.0', id => $next_id++, method => $method };
	$msg->{params} = $params if $params;
	my ($status, $head, $body) = request(token => $token, body => $json->encode($msg));
	my $res = eval { decode_json($body) } // { raw => $body };
	return ($status, $res);
}

sub call_tool {
	my ($token, $name, $args) = @_;
	my ($status, $res) = rpc($token, 'tools/call', { name => $name, arguments => $args // {} });
	return $res->{result} // $res;
}

sub new_token {
	my (@args) = @_;
	my $output = run_cli('--create-token', @args);
	my ($token) = $output =~ /(nagmcp_[0-9a-f]{64})/;
	return $token;
}


# ---- token management on the command line

my $admin = new_token('--user', 'nagiosadmin', '--scopes', 'admin', '--label', 'test admin');
ok($admin, 'admin token created on the command line');
is((stat($tokens))[2] & 07777, 0660, 'token file is group writable for the web server');
my $file = do { local (@ARGV, $/) = $tokens; <> };
unlike($file, qr/\Q$admin\E/, 'clear text token is not stored');
like(run_cli('--list-tokens'), qr/nagiosadmin\s+read,write,admin\s+never\s+test admin/, '--list-tokens shows the token');
like(run_cli('--create-token', '--user', 'x;y'), qr/Invalid user name/, 'invalid user name is rejected');
like(run_cli('--create-token', '--user', 'x', '--scopes', 'root'), qr/Usage/, 'invalid scope is rejected');


# ---- HTTP level

my ($status, $head, $body) = request(token => undef, body => '{}');
is($status, 401, 'request without token is rejected');
like($head, qr/WWW-Authenticate: Bearer/, '401 announces bearer authentication');

($status) = request(token => 'nagmcp_' . ('0' x 64), body => '{}');
is($status, 401, 'unknown token is rejected');

($status) = request(token => $admin, method => 'GET');
is($status, 405, 'GET is not allowed');

($status) = request(token => $admin, body => '{"jsonrpc":"2.0","id":1,"method":"ping"}',
	env => { HTTP_ORIGIN => 'http://evil.example', HTTP_HOST => 'nagios.example' });
is($status, 403, 'foreign Origin is rejected');

($status, $head, $body) = request(token => $admin, body => '{"jsonrpc":"2.0","id":1,"method":"ping"}',
	env => { HTTP_ORIGIN => 'https://nagios.example', HTTP_HOST => 'nagios.example' });
is($status, 200, 'same Origin is accepted');

($status, $head, $body) = request(token => $admin, body => '{not json');
is(decode_json($body)->{error}{code}, -32700, 'invalid JSON gives a parse error');

($status, $head, $body) = request(token => $admin, body => '[{"jsonrpc":"2.0","id":1,"method":"ping"}]');
is(decode_json($body)->{error}{code}, -32600, 'batches are rejected');

($status, $head, $body) = request(token => $admin, body => '{"jsonrpc":"2.0","method":"notifications/initialized"}');
is($status, 202, 'notifications are accepted without a body');


# ---- protocol

my $res;
($status, $res) = rpc($admin, 'initialize',
	{ protocolVersion => '2025-06-18', capabilities => {}, clientInfo => { name => 't', version => '1' } });
is($res->{result}{protocolVersion}, '2025-06-18', 'supported protocol version is echoed');
is($res->{result}{serverInfo}{name}, 'nagios-core', 'server name');
ok($res->{result}{capabilities}{tools}, 'tools capability is announced');

($status, $res) = rpc($admin, 'initialize', { protocolVersion => '1999-01-01' });
is($res->{result}{protocolVersion}, '2025-11-25', 'unknown protocol version gets the newest supported one');

($status, $res) = rpc($admin, 'no/such/method');
is($res->{error}{code}, -32601, 'unknown method');

($status, $res) = rpc($admin, 'tools/list');
my %tools = map { $_->{name} => $_ } @{ $res->{result}{tools} };
is(scalar(keys %tools), 29, 'admin token sees all 29 tools');
ok($tools{list_problems}{annotations}{readOnlyHint}, 'read tools are marked read-only');
ok($tools{cancel_downtime}{annotations}{destructiveHint}, 'cancel_downtime is marked destructive');
is($tools{get_host}{inputSchema}{required}[0], 'host', 'input schemas are valid JSON');

$res = call_tool($admin, 'no_such_tool');
is($res->{error}{code}, -32602, 'unknown tool is a protocol error');


# Online help diagnoses missing grants without granting any rights.
$res = call_tool($admin, 'get_help', { topic => 'configuration' });
ok(!$res->{isError}, 'online help works with configuration disabled');
ok(!$res->{structuredContent}{current_access}{can_plan_config}, 'help reports unavailable config access');
like($res->{structuredContent}{guide}{configuration}, qr/plan_config_change.*apply_config_change/s, 'help explains plan and apply');
unlike($json->encode($res), qr/\Q$admin\E/, 'help never returns the bearer token');
$res = call_tool($admin, 'get_help', { topic => 'invalid' });
ok($res->{isError}, 'unknown help topic rejected');

# ---- read tools

$res = call_tool($admin, 'list_hosts');
ok(!$res->{isError}, 'list_hosts succeeds');
is_deeply([ sort map { $_->{name} } @{ $res->{structuredContent}{hosts} } ], [ 'host1', 'host2' ], 'list_hosts returns the fixture hosts');
like($res->{content}[0]{text}, qr/"host1"/, 'text content mirrors the structured content');

$res = call_tool($admin, 'list_services', { host => 'host1' });
is($res->{structuredContent}{services}[0]{description}, 'Dummy service', 'list_services filters by host');
like($res->{structuredContent}{services}[0]{status}, qr/^(ok|pending)$/, 'states are readable words, not bitmasks');

$res = call_tool($admin, 'get_host', { host => 'host1' });
is($res->{structuredContent}{status}{name}, 'host1', 'get_host returns the status');
ok(exists $res->{structuredContent}{services}, 'get_host lists the services');

$res = call_tool($admin, 'get_host', { host => 'nope' });
ok($res->{isError}, 'unknown host is a tool error');

$res = call_tool($admin, 'list_hosts', { status => ['broken'] });
like($res->{content}[0]{text}, qr/Invalid value 'broken' for 'status'/, 'invalid filter value is explained');

$res = call_tool($admin, 'get_overview');
ok($res->{structuredContent}{hosts} && $res->{structuredContent}{services}, 'get_overview returns host and service counts');

$res = call_tool($admin, 'list_problems');
ok(defined $res->{structuredContent}{counts}{service_problems}, 'list_problems returns counts');


# ---- write tools

reset_cmd_file();
$res = call_tool($admin, 'add_comment', { host => 'host1', comment => 'hello; world' });
ok(!$res->{isError}, 'add_comment succeeds');
is(last_command(), 'ADD_HOST_COMMENT;host1;1;nagiosadmin (MCP);hello, world',
	'comment command is written, semicolons in free text are neutralised');

reset_cmd_file();
$res = call_tool($admin, 'add_comment', { host => "host1\nSHUTDOWN_PROCESS", comment => 'x' });
ok($res->{isError}, 'line break in a host name is rejected');
is(last_command(), '', 'nothing was written');

reset_cmd_file();
$res = call_tool($admin, 'add_comment', { host => 'host1', comment => "a\nSHUTDOWN_PROCESS" });
is(last_command(), 'ADD_HOST_COMMENT;host1;1;nagiosadmin (MCP);a SHUTDOWN_PROCESS', 'line breaks in comments cannot inject commands');

reset_cmd_file();
$res = call_tool($admin, 'acknowledge_problem', { host => 'host1', service => 'Dummy service', comment => 'x' });
like($res->{content}[0]{text}, qr/not in a problem state/, 'acknowledging an OK service is refused');

reset_cmd_file();
$res = call_tool($admin, 'check_now', { host => 'host1', service => 'Dummy service' });
like(last_command(), qr/^SCHEDULE_FORCED_SVC_CHECK;host1;Dummy service;\d+$/, 'check_now schedules a forced check');

reset_cmd_file();
$res = call_tool($admin, 'schedule_downtime', { host => 'host2', duration_minutes => 60, comment => 'maint', start => '1790000000' });
is(last_command(), 'SCHEDULE_HOST_DOWNTIME;host2;1790000000;1790003600;1;0;3600;nagiosadmin (MCP);maint', 'schedule_downtime with duration');

$res = call_tool($admin, 'schedule_downtime', { host => 'host2', comment => 'maint' });
ok($res->{isError}, 'downtime without end or duration is refused');

reset_cmd_file();
$res = call_tool($admin, 'submit_check_result', { host => 'host1', service => 'Dummy service', state => 'critical', output => "down\nmore", perfdata => 'x=1' });
is(last_command(), 'PROCESS_SERVICE_CHECK_RESULT;host1;Dummy service;2;down\nmore|x=1', 'passive result with long output and perfdata');

reset_cmd_file();
$res = call_tool($admin, 'set_notifications', { enabled => JSON::PP::false });
is(last_command(), 'DISABLE_NOTIFICATIONS', 'global notification switch');

reset_cmd_file();
$res = call_tool($admin, 'run_external_command', { command => 'DISABLE_HOST_FLAP_DETECTION', arguments => ['host1'] });
is(last_command(), 'DISABLE_HOST_FLAP_DETECTION;host1', 'run_external_command');
$res = call_tool($admin, 'run_external_command', { command => 'CHANGE_HOST_CHECK_COMMAND', arguments => ['host1', 'x'] });
ok($res->{isError}, 'CHANGE_* commands are refused like in cmd.cgi');
$res = call_tool($admin, 'run_external_command', { command => 'DEL_HOST_COMMENT', arguments => ['1;2', 'x'] });
ok($res->{isError}, 'semicolons in non-final arguments are refused');


# ---- scopes and Nagios rights

$res = call_tool($admin, 'create_token', { user => 'viewer', scopes => ['write'], label => 'viewer' });
my $viewer = $res->{structuredContent}{token};
like($viewer // '', qr/^nagmcp_/, 'admin creates a token for another user via MCP');

($status, $res) = rpc($viewer, 'tools/list');
my @viewer_tools = map { $_->{name} } @{ $res->{result}{tools} };
is(scalar(grep { $tools{$_}{annotations}{readOnlyHint} } @viewer_tools), scalar(@viewer_tools),
	'a read-only Nagios user only gets read tools, even with a write token');

reset_cmd_file();
$res = call_tool($viewer, 'check_now', { host => 'host1' });
ok($res->{isError} || $res->{error}, 'read-only Nagios user cannot write');
is(last_command(), '', 'nothing was written');

$res = call_tool($viewer, 'list_hosts');
is(scalar @{ $res->{structuredContent}{hosts} }, 2, 'viewer can read what cgi.cfg allows');

$res = call_tool($admin, 'create_token', { user => 'nobody', label => 'no rights' });
my $nobody = $res->{structuredContent}{token};
ok($res->{structuredContent}{warning}, 'creating a token for an unknown user warns');
$res = call_tool($nobody, 'list_hosts');
is(scalar @{ $res->{structuredContent}{hosts} }, 0, 'user without rights sees no hosts');
$res = call_tool($nobody, 'add_comment', { host => 'host1', comment => 'x' });
is($res->{error}{code}, -32602, 'read token cannot even see write tools');
$res = call_tool($nobody, 'list_tokens');
is($res->{error}{code}, -32602, 'non-admin cannot manage tokens');

$res = call_tool($admin, 'list_tokens');
is(scalar @{ $res->{structuredContent}{tokens} }, 3, 'list_tokens shows all tokens');
ok(!grep({ exists $_->{token} || exists $_->{hash} } @{ $res->{structuredContent}{tokens} }), 'list_tokens never shows secrets');
my ($nobody_info) = grep { $_->{user} eq 'nobody' } @{ $res->{structuredContent}{tokens} };
my ($self_info) = grep { $_->{current} } @{ $res->{structuredContent}{tokens} };

$res = call_tool($admin, 'revoke_token', { id => $self_info->{id} });
ok($res->{isError}, 'revoking the current token needs allow_self');

$res = call_tool($admin, 'revoke_token', { id => $nobody_info->{id} });
ok($res->{structuredContent}{revoked}, 'revoke_token');
($status) = request(token => $nobody, body => '{"jsonrpc":"2.0","id":1,"method":"ping"}');
is($status, 401, 'revoked token stops working immediately');

# expiry: rewrite the viewer token to have expired
{
	local @ARGV = ($tokens);
	local $^I = '';
	while(<>) {
		s/^(\S+ viewer \S+ \d+) \d+/$1 1000/;
		print;
	}
}
($status, $head, $body) = request(token => $viewer, body => '{"jsonrpc":"2.0","id":1,"method":"ping"}');
is($status, 401, 'expired token is rejected');
like($body, qr/expired/, 'the client is told the token expired');

like(run_cli('--revoke-token', 'abc'), qr/at least 8/, 'short token id is rejected');


# ---- configuration changes

sub slurp { my ($f) = @_; open(my $h, '<', $f) or return undef; local $/; my $c = <$h>; close($h); return $c; }
sub enable_config {
	open(my $c, '>>', $cgi_cfg) or die;
	print $c "mcp_allow_config_changes=1\nmcp_nagios_binary=$top_builddir/base/nagios\n";
	close($c);
}

my $cfg_token = new_token('--user', 'nagiosadmin', '--scopes', 'admin,config', '--label', 'config');
($status, $res) = rpc($cfg_token, 'tools/list');
ok(!grep({ $_->{name} eq 'plan_config_change' } @{ $res->{result}{tools} }), 'config tools are hidden while mcp_allow_config_changes is off');
$res = call_tool($cfg_token, 'plan_config_change', { changes => [] });
is($res->{error}{code}, -32602, 'and cannot be called');

enable_config();
$res = call_tool($cfg_token, 'get_help');
ok($res->{structuredContent}{current_access}{can_plan_config}, 'help reflects enabled config rights');
ok(!$res->{structuredContent}{current_access}{command_changes_enabled}, 'command opt-in reported separately');
$res = call_tool($admin, 'get_help');
ok(!$res->{structuredContent}{current_access}{can_plan_config}, 'admin alone does not grant config in help');
($status, $res) = rpc($cfg_token, 'tools/list');
is(scalar @{ $res->{result}{tools} }, 35, 'admin+config token sees 35 tools once enabled');
($status, $res) = rpc($admin, 'tools/list');
ok(!grep({ $_->{name} eq 'plan_config_change' } @{ $res->{result}{tools} }), 'admin scope alone does not include config');

my $minimal = "$tmp/objects/minimal.cfg";
my $minimal_before = slurp($minimal);
my @new_server = (
	{ action => 'create', type => 'host', attributes => { use => 'test-host', host_name => 'web-03', address => '10.0.0.13', alias => 'Web 03' } },
	{ action => 'create', type => 'service', attributes => { use => 'test-service', host_name => 'web-03', service_description => 'SSH' } },
);

SKIP: {
	skip "$top_builddir/base/nagios not built", 1 unless -x "$top_builddir/base/nagios";

	$res = call_tool($cfg_token, 'plan_config_change', { changes => \@new_server });
	ok(!$res->{isError}, 'plan_config_change succeeds') or diag($res->{content}[0]{text});
	my $plan = $res->{structuredContent};
	like($plan->{diff}, qr{\+\+\+ \Q$tmp\E/objects/mcp/hosts\.cfg}, 'new host goes to the MCP directory');
	like($plan->{diff}, qr{\+cfg_dir=\Q$tmp\E/objects/mcp}, 'nagios.cfg gets a cfg_dir for it');
	like($plan->{diff}, qr{\+    host_name\s+web-03}, 'diff shows the new definition');
	ok($plan->{validation}{valid}, 'nagios -v accepts the planned configuration') or diag(explain($plan->{validation}));
	ok(!-e "$tmp/objects/mcp/hosts.cfg", 'planning writes nothing');

	$res = call_tool($cfg_token, 'apply_config_change', { changes => \@new_server, plan_id => 'x' x 64 });
	like($res->{content}[0]{text}, qr/plan_id mismatch/, 'apply needs the confirmed plan id');

	reset_cmd_file();
	$res = call_tool($cfg_token, 'apply_config_change', { changes => \@new_server, plan_id => $plan->{plan_id} });
	ok($res->{structuredContent}{applied}, 'apply_config_change succeeds') or diag($res->{content}[0]{text});
	like(slurp("$tmp/objects/mcp/services.cfg") // '', qr/service_description\s+SSH/, 'service file written');
	like(slurp("$tmp/nagios.cfg"), qr{^cfg_dir=\Q$tmp\E/objects/mcp$}m, 'include written to nagios.cfg');
	is(last_command(), 'RESTART_PROCESS', 'Nagios is asked to reload');
	my $backup_id = $res->{structuredContent}{backup_id};

	$res = call_tool($cfg_token, 'plan_config_change', { changes => [ $new_server[0] ] });
	like($res->{content}[0]{text}, qr/already exists/, 'duplicates are refused');

	# surgical edit of a hand written file
	$res = call_tool($cfg_token, 'get_config_source', { type => 'host', name => 'host1' });
	like($res->{structuredContent}{definition}, qr/^define host \{\n\thost_name host1\n\tuse\ttest-host\n/, 'get_config_source returns the raw block');
	my @upd = ({ action => 'update', type => 'host', name => 'host1', set => { alias => 'First; host' } });
	$res = call_tool($cfg_token, 'plan_config_change', { changes => \@upd });
	$plan = $res->{structuredContent};
	$res = call_tool($cfg_token, 'apply_config_change', { changes => \@upd, plan_id => $plan->{plan_id} });
	ok($res->{structuredContent}{applied}, 'update applied');
	my $minimal_after = slurp($minimal);
	like($minimal_after, qr/alias\s+First\\; host\n/, 'value written with escaped semicolon');
	my @before = split /\n/, $minimal_before;
	my @after = split /\n/, $minimal_after;
	is(scalar(grep { !defined $after[$_] || $before[$_] ne $after[$_] } 0..$#before), 1, 'exactly one line of the hand written file changed');
	like($minimal_after, qr/command_name\twith_continuation_lines/, 'continuation lines elsewhere are untouched');

	# validation failures are reported and not applied
	$res = call_tool($cfg_token, 'plan_config_change', { changes => [ { action => 'delete', type => 'host', name => 'web-03' } ] });
	ok(!$res->{structuredContent}{validation}{valid}, 'deleting a host that still has services is invalid');
	like(join(' ', @{ $res->{structuredContent}{validation}{errors} }), qr/web-03/, 'with the Nagios error message');
	unlike(join(' ', @{ $res->{structuredContent}{validation}{errors} }), qr{/tmp/nagios-mcp-}, 'staging paths are hidden');
	my $invalid_plan = $res->{structuredContent}{plan_id};
	$res = call_tool($cfg_token, 'apply_config_change', { changes => [ { action => 'delete', type => 'host', name => 'web-03' } ], plan_id => $invalid_plan });
	ok($res->{isError}, 'an invalid plan cannot be applied');
	ok(-e "$tmp/objects/mcp/hosts.cfg" && slurp("$tmp/objects/mcp/hosts.cfg") =~ /web-03/, 'and nothing was changed');

	# guards
	$res = call_tool($cfg_token, 'plan_config_change', { changes => [ { action => 'update', type => 'command', name => 'check_me', set => { command_line => '/bin/id' } } ] });
	like($res->{content}[0]{text}, qr/mcp_allow_command_changes/, 'command definitions are read-only by default');
	$res = call_tool($cfg_token, 'plan_config_change', { changes => [ { action => 'update', type => 'host', name => 'host1', set => { notes => "x\ndefine command {" } } ] });
	ok($res->{isError}, 'multi-line values are refused');
	$res = call_tool($cfg_token, 'plan_config_change', { changes => [ { action => 'update', type => 'hostdependency', name => 'x', set => { a => 'b' } } ] });
	ok($res->{isError}, 'types without a name cannot be edited');

	SKIP: {
		skip 'root can write any file', 2 if $> == 0;
		chmod 0444, $minimal;
		my @ro = ({ action => 'update', type => 'host', name => 'host2', set => { alias => 'x' } });
		$res = call_tool($cfg_token, 'plan_config_change', { changes => \@ro });
		$res = call_tool($cfg_token, 'apply_config_change', { changes => \@ro, plan_id => $res->{structuredContent}{plan_id} });
		like($res->{content}[0]{text}, qr/not writable/, 'read-only files are respected');
		is(slurp($minimal), $minimal_after, 'and left unchanged');
		chmod 0664, $minimal;
	}

	# undo the first change from its backup
	$res = call_tool($cfg_token, 'list_config_backups');
	ok((grep { $_->{backup_id} eq $backup_id } @{ $res->{structuredContent}{backups} }), 'backup is listed');
	$res = call_tool($cfg_token, 'restore_config_backup', { backup_id => $backup_id });
	like($res->{structuredContent}{diff}, qr{\+\+\+ /dev/null}, 'restore plan removes the created files');
	$res = call_tool($cfg_token, 'restore_config_backup', { backup_id => $backup_id, plan_id => $res->{structuredContent}{plan_id} });
	ok($res->{structuredContent}{applied}, 'backup restored');
	ok(!-e "$tmp/objects/mcp/hosts.cfg", 'created file removed');
	unlike(slurp("$tmp/nagios.cfg"), qr/objects\/mcp/, 'include removed again');
	$res = call_tool($cfg_token, 'restore_config_backup', { backup_id => '../../etc' });
	ok($res->{isError}, 'backup ids cannot escape the backup directory');
}

done_testing();
