<?php
include_once(dirname(__FILE__).'/includes/utils.inc.php');

$this_version = '4.5.8';
$this_year = '2024';
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">

<html id="main">

<head>

<meta name="ROBOTS" content="NOINDEX, NOFOLLOW" />
<title>Nagios Core</title>
<link rel="stylesheet" type="text/css" href="stylesheets/common.css?<?php echo $this_version; ?>" />
<link rel="stylesheet" type="text/css" href="stylesheets/nag_funcs.css?<?php echo $this_version; ?>" />
<script type="text/javascript" src="js/jquery-3.7.1.min.js"></script>
<script type="text/javascript" src="js/nag_funcs.js"></script>

<script type='text/javascript'>
	var cookie;
	<?php if ($cfg["enable_page_tour"]) { ?>
		var vbox;
		var vBoxId = "main";
		var vboxText = "<a href=https://www.nagios.com/tours target=_blank> " +
						"Click here to watch the entire Nagios Core 4 Tour!</a>";
	<?php } ?>
	$(document).ready(function() {
		var user = "<?php echo htmlspecialchars($_SERVER['REMOTE_USER']) ?? ''); ?>";

		<?php if ($cfg["enable_page_tour"]) { ?>
			vBoxId += ";" + user;
			vbox = new vidbox({pos:'lr',vidurl:'https://www.youtube.com/embed/2hVBAet-XpY',
								text:vboxText,vidid:vBoxId});
		<?php } ?>

		getCoreStatus();
	});

	// Get the daemon status JSON.
	function getCoreStatus() {
		setCoreStatusHTML('passiveonly', 'Checking process status...');

		$.get('<?php echo $cfg["cgi_base_url"];?>/statusjson.cgi?query=programstatus', function(d) {
			d = d && d.data && d.data.programstatus || false;
			if (d && d.nagios_pid) {
				var pid = d.nagios_pid;
				var daemon = d.daemon_mode ? 'Daemon' : 'Process';
				setCoreStatusHTML('enabled', daemon + ' running with PID ' + pid);
			} else {
				setCoreStatusHTML('disabled', 'Not running');
			}
		}).fail(function() {
			setCoreStatusHTML('disabled', 'Unable to get process status');
		});
	}

	function setCoreStatusHTML(image, text) {
		$('#core-status').html(`<span class='dot-${image}'>●</span>&nbsp&nbsp${text}`);
	}
</script>

</head>


<body id="splashpage">

<div id="currentversioninfo">
	<div>
		<div class="version">Version <b><?php echo $this_version; ?></b></div>
		<div class="releasedate">November 19, 2024</div>
	</div>
	<div><span id="core-status"></span></div>
	<a class="checkforupdates" href="https://www.nagios.org/checkforupdates/?version=<?php echo $this_version; ?>&amp;product=nagioscore" target="_blank">Check for updates</a>
</div>


<div id="updateversioninfo">
<?php
	$updateinfo = get_update_information();
	if (!$updateinfo['update_checks_enabled']) {
?>
		<div class="updatechecksdisabled">
			<div class="warningmessage">Warning: Automatic Update Checks are Disabled!</div>
			<div class="submessage">Disabling update checks presents a possible security risk.  Visit <a href="https://www.nagios.org/" target="_blank">nagios.org</a> to check for updates manually or enable update checks in your Nagios config file.</a></div>
		</div>
<?php
	} else if ($updateinfo['update_available'] && $this_version < $updateinfo['update_version']) {
?>
		<div class="updateavailable">
			<div class="updatemessage">A new version of Nagios Core is available!</div>
			<div class="submessage">Visit <a href="https://www.nagios.org/download/" target="_blank">nagios.org</a> to download Nagios <?php echo htmlentities($updateinfo['update_version'], ENT_QUOTES, 'UTF-8');?>.</div>
		</div>
<?php
	}
?>
</div>

<div id="mainsplash">
	<a href="https://www.nagios.org/launch?utm_campaign=csp&utm_source=nagioscore&utm_medium=splash_thumbnail&utm_content=<?php echo $this_version; ?>" target="_blank"><img src="images/csp-dashboard.avif" /></a>
	<div id="splashtext">
		<div id="splashtexttitle">Meet Nagios Core Services Platform</div>
		<div>The next generation of Open Source powered monitoring with advanced dashboards, monitoring wizards, and much more!</div>
		<div id="splashbuttons">
			<a id="splashlearnmore" href="https://www.nagios.org/launch?utm_campaign=csp&utm_source=nagioscore&utm_medium=splash_button&utm_content=<?php echo $this_version; ?>" target="_blank">Learn More</a>
			<a id="splashnewsletter" href="https://www.nagios.org/newsletter?utm_campaign=csp&utm_source=nagioscore&utm_medium=splash_newsletter_link&utm_content=<?php echo $this_version; ?>" target="_blank">Newsletter Sign-Up</a>
		</div>
	</div>
</div>

<div id="mainfooter">
	<div id="maincopy">
		Copyright &copy; 2010-<?php echo $this_year; ?> Nagios Core Development Team and Community Contributors. Copyright &copy; 1999-2009 Ethan Galstad. See the THANKS file for more information on contributors.
	</div>
	<div CLASS="disclaimer">
		Nagios Core is licensed under the GNU General Public License and is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.  Nagios, Nagios Core and the Nagios logo are trademarks, servicemarks, registered trademarks or registered servicemarks owned by Nagios Enterprises, LLC.  Use of the Nagios marks is governed by the <A href="https://www.nagios.com/legal/trademarks/">trademark use restrictions</a>.
	</div>
	<div class="logos">
		<a href="https://www.nagios.org/" target="_blank"><img src="images/logos/nagios-n-logo.svg" title="Nagios.org" /></a>
	</div>
</div>


</body>
</html>
