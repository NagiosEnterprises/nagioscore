<?php
//

$cfg['cgi_config_file']='/usr/local/nagios/etc/cgi.cfg';  // location of the CGI config file
$cfg['main_config_file']='/usr/local/nagios/etc/nagios.cfg';  // default location of the main Nagios config file
$cfg['status_file']='/usr/local/nagios/var/status.dat'; // default location of Nagios status file

// utilities
require_once(dirname(__FILE__).'/includes/utils.inc.php');

?>