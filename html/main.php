<?php
include_once(dirname(__FILE__).'/includes/utils.inc.php');

$this_version = '4.5.14';
$this_year = '2026';
// empty theme class: follow the operating system setting
$theme = $cfg['theme'] ?? 'auto';
if ($theme != 'dark' && $theme != 'light') {
	$theme = '';
}
$c = htmlspecialchars($cfg['cgi_base_url']);
?>
<!DOCTYPE html>

<html id="main" class="<?= $theme ?>">

<head>

<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
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
		var user = "<?php echo htmlspecialchars($_SERVER['REMOTE_USER'] ?? ''); ?>";

		<?php if ($cfg["enable_page_tour"]) { ?>
			vBoxId += ";" + user;
			vbox = new vidbox({pos:'lr',vidurl:'https://www.youtube.com/embed/2hVBAet-XpY',
								text:vboxText,vidid:vBoxId});
		<?php } ?>

		getCoreStatus();
		getCounts();
		setInterval(function() { getCoreStatus(); getCounts(); }, 60000);
	});

	// Get the daemon status JSON.
	function getCoreStatus() {
		setCoreStatusHTML('passiveonly', 'Checking process status…');

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
		$('#core-status').attr('class', 'core-status ' + image)
			.empty().append($('<span class="pulse">'), $('<span>').text(text));
	}

	// Host and service totals for the overview cards.
	function getCounts() {
		var base = '<?php echo $cfg["cgi_base_url"];?>/statusjson.cgi?query=';
		$.get(base + 'hostcount', function(d) {
			renderCounts('hosts', d && d.data && d.data.count,
				['up', 'down', 'unreachable', 'pending'], ['down', 'unreachable']);
		});
		$.get(base + 'servicecount', function(d) {
			renderCounts('services', d && d.data && d.data.count,
				['ok', 'warning', 'critical', 'unknown', 'pending'], ['warning', 'critical', 'unknown']);
		});
	}

	function renderCounts(id, c, keys, bad) {
		if (!c) return;
		var card = $('#card-' + id), total = 0, problems = 0;
		keys.forEach(function(k) { total += c[k] || 0; });
		bad.forEach(function(k) { problems += c[k] || 0; });
		card.find('.card-total').text(total);
		card.find('.card-problems')
			.text(problems ? problems + (problems == 1 ? ' problem' : ' problems') : 'All good')
			.toggleClass('bad', problems > 0);
		var bar = card.find('.bar').empty(), legend = card.find('.legend').empty();
		keys.forEach(function(k) {
			var n = c[k] || 0;
			if (n)
				bar.append($('<span>').addClass('s-' + k).css('flex-grow', n).attr('title', n + ' ' + k));
			legend.append($('<a>').addClass('s-' + k).attr('href', card.data('link-' + k))
				.append($('<i>'), $('<span>').text(k), $('<b>').text(n)));
		});
	}
</script>

<style>
	body#splashpage {
		min-height: 100vh;
		box-sizing: border-box;
		padding: 32px clamp(16px, 4vw, 48px) 40px;
		text-align: left;
		background:
			radial-gradient(900px 420px at 0% -10%, color-mix(in srgb, var(--ok) 9%, transparent), transparent 70%),
			radial-gradient(700px 380px at 100% 0%, color-mix(in srgb, var(--info) 7%, transparent), transparent 70%),
			var(--bg);
	}
	.home { max-width: 1180px; margin: 0 auto; display: flex; flex-direction: column; gap: 20px; }

	/* hero */
	.hero { display: flex; flex-wrap: wrap; align-items: flex-end; justify-content: space-between; gap: 16px 24px; }
	.hero h1 { margin: 0; font-size: 28px; font-weight: 700; letter-spacing: -0.02em; }
	.hero p { margin: 6px 0 0; color: var(--muted); font-size: 14px; }
	.chip {
		display: inline-flex; align-items: center;
		height: 24px; padding: 0 10px; margin-left: 10px;
		font-size: 12px; font-weight: 600; vertical-align: 5px; letter-spacing: 0;
		color: var(--muted); background: var(--surface-2);
		border: 1px solid var(--border); border-radius: 999px;
	}
	.hero-actions { display: flex; flex-wrap: wrap; align-items: center; gap: 10px; }
	.core-status {
		display: inline-flex; align-items: center; gap: 10px;
		height: 36px; padding: 0 14px; box-sizing: border-box;
		font-size: 13px; font-weight: 500;
		background: var(--surface); border: 1px solid var(--border); border-radius: 999px;
	}
	.core-status .pulse { position: relative; width: 8px; height: 8px; border-radius: 50%; background: var(--pending); }
	.core-status.enabled .pulse { background: var(--ok); }
	.core-status.disabled .pulse { background: var(--critical); }
	.core-status.passiveonly .pulse { background: var(--warning); }
	.core-status.enabled .pulse::after {
		content: ""; position: absolute; inset: -4px; border-radius: 50%;
		border: 2px solid var(--ok); opacity: 0; animation: ping 2.2s ease-out infinite;
	}
	@keyframes ping { 0% { transform: scale(.5); opacity: .8; } 100% { transform: scale(1.6); opacity: 0; } }
	.btn:link, .btn:visited {
		display: inline-flex; align-items: center; gap: 8px;
		height: 36px; padding: 0 14px; box-sizing: border-box;
		font-size: 13px; font-weight: 600; color: var(--text);
		background: var(--surface-2); border: 1px solid var(--border-strong); border-radius: 8px;
	}
	.btn:hover { text-decoration: none; background: var(--surface-hover); color: var(--text); }
	.btn svg, .quick svg { width: 16px; height: 16px; fill: none; stroke: currentColor; stroke-width: 2; stroke-linecap: round; stroke-linejoin: round; }

	/* update notices */
	#updateversioninfo { width: auto; margin: 0; }
	#updateversioninfo:not(:has(div)) { display: none; }
	.updatechecksdisabled, .updateavailable {
		display: grid; grid-template-columns: 20px 1fr; gap: 2px 12px;
		padding: 12px 16px; border-radius: var(--radius); border: 1px solid;
		font-size: 13px; line-height: 1.5; text-align: left;
	}
	.updatechecksdisabled { background: var(--warning-soft); border-color: color-mix(in srgb, var(--warning) 35%, transparent); }
	.updateavailable { background: var(--info-soft); border-color: color-mix(in srgb, var(--info) 35%, transparent); }
	.updatechecksdisabled::before, .updateavailable::before {
		content: "!"; grid-row: span 2; display: grid; place-items: center;
		width: 20px; height: 20px; margin-top: 1px; border-radius: 50%;
		font-size: 12px; font-weight: 800; color: #1c1300; background: var(--warning);
	}
	.updateavailable::before { content: "\2191"; color: #fff; background: var(--info); }
	.updatechecksdisabled .warningmessage, .updateavailable .updatemessage { font-size: 13px; font-weight: 700; color: var(--text); }
	.updatechecksdisabled .submessage, .updateavailable .submessage { color: var(--muted); }

	/* overview cards */
	.cards { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 16px; }
	.card {
		padding: 18px 20px; background: var(--surface);
		border: 1px solid var(--border); border-radius: 14px; box-shadow: var(--shadow);
	}
	.card-head { display: flex; align-items: baseline; justify-content: space-between; gap: 12px; }
	.card-title { font-size: 11px; font-weight: 700; letter-spacing: .08em; text-transform: uppercase; color: var(--muted); }
	.card-title a:link, .card-title a:visited { color: inherit; }
	.card-problems { font-size: 12px; font-weight: 600; color: var(--ok); }
	.card-problems.bad { color: var(--critical); }
	.card-total { margin: 4px 0 14px; font-size: 34px; font-weight: 700; letter-spacing: -0.02em; font-variant-numeric: tabular-nums; }
	.bar { display: flex; gap: 3px; height: 8px; margin-bottom: 14px; border-radius: 999px; overflow: hidden; background: var(--surface-2); }
	.bar span { min-width: 6px; background: var(--c); }
	.legend { display: flex; flex-wrap: wrap; gap: 6px; }
	.legend a:link, .legend a:visited {
		display: inline-flex; align-items: center; gap: 7px;
		height: 28px; padding: 0 10px;
		font-size: 12px; color: var(--muted); text-transform: capitalize;
		background: var(--surface-2); border: 1px solid var(--border); border-radius: 8px;
	}
	.legend a:hover { color: var(--text); text-decoration: none; border-color: var(--border-strong); }
	.legend b { color: var(--text); font-variant-numeric: tabular-nums; }
	.legend i { width: 8px; height: 8px; border-radius: 50%; background: var(--c); }
	.s-up, .s-ok { --c: var(--ok); }
	.s-warning { --c: var(--warning); }
	.s-unknown { --c: var(--unknown); }
	.s-down, .s-critical { --c: var(--critical); }
	.s-unreachable { --c: var(--unreachable); }
	.s-pending { --c: var(--pending); }

	/* quick links */
	.quick { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 12px; }
	.quick a:link, .quick a:visited {
		display: flex; align-items: center; gap: 12px; padding: 14px 16px;
		color: var(--text); font-weight: 600;
		background: var(--surface); border: 1px solid var(--border); border-radius: 12px;
		transition: border-color .15s, transform .15s, background .15s;
	}
	.quick a:hover { text-decoration: none; border-color: color-mix(in srgb, var(--accent) 45%, var(--border)); background: var(--surface-hover); transform: translateY(-1px); }
	.quick .ic { display: grid; place-items: center; flex: none; width: 34px; height: 34px; border-radius: 9px; color: var(--accent); background: color-mix(in srgb, var(--accent) 13%, transparent); }
	.quick small { display: block; font-weight: 400; font-size: 12px; color: var(--muted); }

	/* promo */
	#mainsplash {
		display: grid; grid-template-columns: minmax(0, 1.1fr) minmax(0, 1fr); gap: 32px; align-items: center;
		max-width: none; margin: 0; padding: 24px;
		background: var(--surface); border: 1px solid var(--border); border-radius: 16px;
	}
	#mainsplash img { display: block; width: 100%; border-radius: 10px; border: 1px solid var(--border); }
	#splashtext { display: flex; flex-direction: column; gap: 12px; font-size: 14px; line-height: 1.55; color: var(--muted); }
	#splashtexttitle { font-size: 22px; line-height: 1.25; font-weight: 700; color: var(--text); letter-spacing: -0.01em; }
	.eyebrow { font-size: 11px; font-weight: 700; letter-spacing: .08em; text-transform: uppercase; color: var(--accent); }
	#splashbuttons { display: flex; align-items: center; gap: 18px; flex-wrap: wrap; margin-top: 6px; }
	#splashlearnmore:link, #splashlearnmore:visited {
		display: inline-block; padding: 10px 16px; border-radius: 8px; font-weight: 600;
		color: var(--accent-fg); background: var(--accent);
	}
	#splashlearnmore:hover { filter: brightness(1.08); }
	#splashnewsletter:link, #splashnewsletter:visited { color: var(--muted); text-decoration: none; }
	#splashnewsletter:hover { color: var(--text); }

	/* footer */
	#splashpage #mainfooter {
		width: auto; margin: 8px 0 0; padding: 20px 0 0;
		display: flex; gap: 20px; align-items: flex-start;
		border-top: 1px solid var(--border);
		font-size: 11px; line-height: 1.6; color: var(--muted);
	}
	#splashpage #mainfooter > div:not(.logos) { flex: 1; }
	#maincopy { margin: 0; }
	#splashpage #mainfooter .disclaimer { margin: 0; }
	#splashpage #mainfooter .logos { margin: 0; order: -1; }
	#mainfooter .nlogo { width: 28px; height: 28px; background-color: var(--muted); }
	#mainfooter a:link, #mainfooter a:visited { color: var(--muted); text-decoration: underline; }

	@media (max-width: 1000px) {
		.quick { grid-template-columns: repeat(2, minmax(0, 1fr)); }
		#mainsplash { grid-template-columns: 1fr; }
	}
	@media (max-width: 640px) {
		body#splashpage { padding: 20px 14px 32px; }
		.home { gap: 16px; }
		.hero h1 { font-size: 24px; }
		.hero-actions { width: 100%; }
		.core-status { flex: 1 1 auto; }
		.cards { grid-template-columns: 1fr; }
		.quick { gap: 10px; }
		.quick a:link, .quick a:visited { flex-direction: column; align-items: flex-start; gap: 10px; padding: 14px; }
		#mainsplash { padding: 16px; gap: 18px; }
		#splashpage #mainfooter { flex-direction: column; gap: 10px; }
	}
	@media (prefers-reduced-motion: reduce) {
		.core-status .pulse::after { animation: none; }
	}
</style>

</head>


<body id="splashpage">

<div class="home">

<div class="hero">
	<div>
		<h1>Nagios Core<span class="chip">v<?php echo $this_version; ?></span></h1>
		<p>Released August 05, 2026</p>
	</div>
	<div class="hero-actions">
		<span id="core-status" class="core-status"></span>
		<a class="btn checkforupdates" href="https://www.nagios.org/checkforupdates/?version=<?php echo $this_version; ?>&amp;product=nagioscore" target="_blank" rel="noopener">
			<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M21 12a9 9 0 1 1-3-6.7L21 8"/><path d="M21 3v5h-5"/></svg>Check for updates</a>
	</div>
</div>

<div id="updateversioninfo">
<?php
	$updateinfo = get_update_information();
	if (!$updateinfo['update_checks_enabled']) {
?>
		<div class="updatechecksdisabled">
			<div class="warningmessage">Automatic update checks are disabled</div>
			<div class="submessage">Disabling update checks presents a possible security risk. Visit <a href="https://www.nagios.org/" target="_blank">nagios.org</a> to check for updates manually or enable update checks in your Nagios config file.</div>
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

<div class="cards">
	<div class="card" id="card-hosts"
		data-link-up="<?= $c ?>/status.cgi?hostgroup=all&amp;style=hostdetail&amp;hoststatustypes=2"
		data-link-down="<?= $c ?>/status.cgi?hostgroup=all&amp;style=hostdetail&amp;hoststatustypes=4"
		data-link-unreachable="<?= $c ?>/status.cgi?hostgroup=all&amp;style=hostdetail&amp;hoststatustypes=8"
		data-link-pending="<?= $c ?>/status.cgi?hostgroup=all&amp;style=hostdetail&amp;hoststatustypes=1">
		<div class="card-head">
			<div class="card-title"><a href="<?= $c ?>/status.cgi?hostgroup=all&amp;style=hostdetail">Hosts</a></div>
			<div class="card-problems"></div>
		</div>
		<div class="card-total">&ndash;</div>
		<div class="bar"></div>
		<div class="legend"></div>
	</div>
	<div class="card" id="card-services"
		data-link-ok="<?= $c ?>/status.cgi?host=all&amp;servicestatustypes=2"
		data-link-warning="<?= $c ?>/status.cgi?host=all&amp;servicestatustypes=4"
		data-link-critical="<?= $c ?>/status.cgi?host=all&amp;servicestatustypes=16"
		data-link-unknown="<?= $c ?>/status.cgi?host=all&amp;servicestatustypes=8"
		data-link-pending="<?= $c ?>/status.cgi?host=all&amp;servicestatustypes=1">
		<div class="card-head">
			<div class="card-title"><a href="<?= $c ?>/status.cgi?host=all">Services</a></div>
			<div class="card-problems"></div>
		</div>
		<div class="card-total">&ndash;</div>
		<div class="bar"></div>
		<div class="legend"></div>
	</div>
</div>

<div class="quick">
	<a href="<?= $c ?>/tac.cgi"><span class="ic"><svg viewBox="0 0 24 24" aria-hidden="true"><rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/><rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/></svg></span><span>Tactical Overview<small>Everything at a glance</small></span></a>
	<a href="<?= $c ?>/status.cgi?host=all&amp;type=detail&amp;hoststatustypes=3&amp;serviceprops=10&amp;servicestatustypes=28"><span class="ic"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 3l10 18H2z"/><path d="M12 10v4M12 17.5h.01"/></svg></span><span>Unhandled Problems<small>Needs attention</small></span></a>
	<a href="<?= $c ?>/statusmap.cgi?host=all"><span class="ic"><svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="5" r="2"/><circle cx="5" cy="19" r="2"/><circle cx="19" cy="19" r="2"/><path d="M12 7v5M12 12l-6 5M12 12l6 5"/></svg></span><span>Network Map<small>Host topology</small></span></a>
	<a href="<?= $c ?>/showlog.cgi"><span class="ic"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 4h14v16H5z"/><path d="M9 8h6M9 12h6M9 16h4"/></svg></span><span>Event Log<small>Latest events</small></span></a>
</div>

<div id="mainsplash">
	<a href="https://www.nagios.org/?utm_campaign=csp&utm_source=nagioscore&utm_medium=splash_thumbnail&utm_content=<?php echo $this_version; ?>" target="_blank"><img src="images/csp-dashboard.avif" alt="Nagios Core Services Platform dashboard" loading="lazy" /></a>
	<div id="splashtext">
		<div class="eyebrow">From Nagios</div>
		<div id="splashtexttitle">Meet Nagios Core Services Platform</div>
		<div>The next generation of Open Source powered monitoring with advanced dashboards, monitoring wizards, and much more!</div>
		<div id="splashbuttons">
			<a id="splashlearnmore" href="https://www.nagios.org/?utm_campaign=csp&utm_source=nagioscore&utm_medium=splash_button&utm_content=<?php echo $this_version; ?>#csp-section-home" target="_blank">Learn More</a>
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
		<a href="https://www.nagios.org/" target="_blank"><div class="nlogo nagioslogo"></div></a>
	</div>
</div>

</div>

</body>
</html>
