<?php
// Main navigation, shared by the index.php shell and the standalone side.php.

function nav_icon($name)
{
	static $paths = array(
		'home'     => '<path d="M3 11l9-7 9 7"/><path d="M5 10v10h14V10"/>',
		'docs'     => '<path d="M4 4h11l5 5v11H4z"/><path d="M15 4v5h5"/>',
		'logout'   => '<path d="M15 4h4v16h-4"/><path d="M10 8l-4 4 4 4"/><path d="M6 12h10"/>',
		'tac'      => '<rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/><rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/>',
		'map'      => '<circle cx="12" cy="5" r="2"/><circle cx="5" cy="19" r="2"/><circle cx="19" cy="19" r="2"/><path d="M12 7v5M12 12l-6 5M12 12l6 5"/>',
		'hosts'    => '<rect x="3" y="4" width="18" height="7" rx="1.5"/><rect x="3" y="13" width="18" height="7" rx="1.5"/><path d="M7 7.5h.01M7 16.5h.01"/>',
		'services' => '<path d="M3 12h4l3-8 4 16 3-8h4"/>',
		'groups'   => '<rect x="3" y="3" width="8" height="8" rx="1.5"/><rect x="13" y="3" width="8" height="8" rx="1.5"/><rect x="3" y="13" width="18" height="8" rx="1.5"/>',
		'problems' => '<path d="M12 3l10 18H2z"/><path d="M12 10v4M12 17.5h.01"/>',
		'avail'    => '<path d="M21 12a9 9 0 1 1-9-9"/><path d="M21 3v9h-9"/>',
		'trends'   => '<path d="M3 17l6-6 4 4 8-8"/><path d="M15 7h6v6"/>',
		'alerts'   => '<path d="M6 16V11a6 6 0 0 1 12 0v5l2 2H4z"/><path d="M10 21h4"/>',
		'notify'   => '<path d="M4 5h16v11H8l-4 4z"/>',
		'log'      => '<path d="M5 4h14v16H5z"/><path d="M9 8h6M9 12h6M9 16h4"/>',
		'comments' => '<path d="M21 12a8 8 0 0 1-12 7l-5 1 1-4a8 8 0 1 1 16-4z"/>',
		'downtime' => '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',
		'process'  => '<rect x="5" y="5" width="14" height="14" rx="2"/><path d="M9 1v4M15 1v4M9 19v4M15 19v4M1 9h4M1 15h4M19 9h4M19 15h4"/>',
		'perf'     => '<path d="M12 14l4-4"/><path d="M3.5 18a9 9 0 1 1 17 0"/>',
		'queue'    => '<path d="M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01"/>',
		'config'   => '<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z"/>',
		'outages'  => '<path d="M2 12h5l2-3M22 12h-5l-2 3"/><path d="M9 9l6 6"/>',
		'search'   => '<circle cx="11" cy="11" r="7"/><path d="M20 20l-3.5-3.5"/>',
	);
	return '<svg viewBox="0 0 24 24" aria-hidden="true">' . $paths[$name] . '</svg>';
}

function print_nav($cfg, $target)
{
	$c = htmlspecialchars($cfg['cgi_base_url']);
	$t = htmlspecialchars($target);

	// section title => list of array(label, url, icon, badge id, children)
	$sections = array(
		'General' => array(
			array('Home', 'main.php', 'home'),
			array('Documentation', 'https://assets.nagios.com/downloads/nagioscore/docs/nagioscore/4/en/', 'docs'),
		),
		'Current Status' => array(
			array('Tactical Overview', "$c/tac.cgi", 'tac'),
			array('Map', "$c/statusmap.cgi?host=all", 'map'),
			array('Hosts', "$c/status.cgi?hostgroup=all&amp;style=hostdetail", 'hosts', 'badge-hosts'),
			array('Services', "$c/status.cgi?host=all", 'services', 'badge-services'),
			array('Host Groups', "$c/status.cgi?hostgroup=all&amp;style=overview", 'groups', null, array(
				array('Summary', "$c/status.cgi?hostgroup=all&amp;style=summary"),
				array('Grid', "$c/status.cgi?hostgroup=all&amp;style=grid"),
			)),
			array('Service Groups', "$c/status.cgi?servicegroup=all&amp;style=overview", 'groups', null, array(
				array('Summary', "$c/status.cgi?servicegroup=all&amp;style=summary"),
				array('Grid', "$c/status.cgi?servicegroup=all&amp;style=grid"),
			)),
			array('Problems', "$c/status.cgi?host=all&amp;servicestatustypes=28", 'problems', 'badge-problems', array(
				array('Services', "$c/status.cgi?host=all&amp;servicestatustypes=28"),
				array('Services (Unhandled)', "$c/status.cgi?host=all&amp;type=detail&amp;hoststatustypes=3&amp;serviceprops=10&amp;servicestatustypes=28"),
				array('Hosts', "$c/status.cgi?hostgroup=all&amp;style=hostdetail&amp;hoststatustypes=12"),
				array('Hosts (Unhandled)', "$c/status.cgi?hostgroup=all&amp;style=hostdetail&amp;hoststatustypes=12&amp;hostprops=42"),
				array('Network Outages', "$c/outages.cgi"),
			)),
		),
		'Reports' => array(
			array('Availability', "$c/avail.cgi", 'avail'),
			array('Trends', "$c/trends.cgi", 'trends'),
			array('Alerts', "$c/history.cgi?host=all", 'alerts', null, array(
				array('History', "$c/history.cgi?host=all"),
				array('Summary', "$c/summary.cgi"),
				array('Histogram', "$c/histogram.cgi"),
			)),
			array('Notifications', "$c/notifications.cgi?contact=all", 'notify'),
			array('Event Log', "$c/showlog.cgi", 'log'),
		),
		'System' => array(
			array('Comments', "$c/extinfo.cgi?type=3", 'comments'),
			array('Downtime', "$c/extinfo.cgi?type=6", 'downtime'),
			array('Process Info', "$c/extinfo.cgi?type=0", 'process'),
			array('Performance Info', "$c/extinfo.cgi?type=4", 'perf'),
			array('Scheduling Queue', "$c/extinfo.cgi?type=7", 'queue'),
			array('Configuration', "$c/config.cgi", 'config'),
		),
	);

	echo "<nav class=\"nav\" aria-label=\"Main\">\n";

	echo "\t<form class=\"nav-search\" method=\"get\" action=\"$c/status.cgi\" target=\"$t\" role=\"search\">\n";
	echo "\t\t" . nav_icon('search') . "\n";
	echo "\t\t<input type=\"hidden\" name=\"navbarsearch\" value=\"1\">\n";
	echo "\t\t<input type=\"text\" name=\"host\" placeholder=\"Search hosts\" aria-label=\"Search hosts\" autocomplete=\"off\" autocapitalize=\"none\" spellcheck=\"false\">\n";
	echo "\t</form>\n";

	foreach ($sections as $title => $items) {
		echo "\t<div class=\"nav-section\">\n\t\t<div class=\"nav-section-title\">$title</div>\n\t\t<ul>\n";
		foreach ($items as $item) {
			$external = strpos($item[1], 'http') === 0;
			$tgt = $external ? '_blank' : $t;
			$rel = $external ? ' rel="noopener"' : '';
			$badge = !empty($item[3]) ? "<span class=\"nav-badge\" id=\"{$item[3]}\" hidden></span>" : '';
			echo "\t\t\t<li><a href=\"{$item[1]}\" target=\"$tgt\"$rel>" . nav_icon($item[2]) . "<span>{$item[0]}</span>$badge</a>";
			if (!empty($item[4])) {
				echo "\n\t\t\t\t<ul>\n";
				foreach ($item[4] as $sub)
					echo "\t\t\t\t\t<li><a href=\"{$sub[1]}\" target=\"$t\">{$sub[0]}</a></li>\n";
				echo "\t\t\t\t</ul>\n\t\t\t";
			}
			echo "</li>\n";
		}
		// logout link only exists with form based login (see httpd-formauth.conf)
		if ($title == 'General' && strcasecmp($_SERVER['AUTH_TYPE'] ?? '', 'form') == 0) {
			echo "\t\t\t<li><a href=\"logout\" target=\"_top\">" . nav_icon('logout') . "<span>Logout</span></a></li>\n";
		}
		echo "\t\t</ul>\n\t</div>\n";
	}

	echo "</nav>\n";
}
