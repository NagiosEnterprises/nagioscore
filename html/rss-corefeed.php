<?php

// RSS reader
define('MAGPIE_DIR', './includes/rss/');
define('MAGPIE_CACHE_ON', 0);
define('MAGPIE_CACHE_AGE', 0);
define('MAGPIE_CACHE_DIR', '/tmp/magpie_cache');
require_once(MAGPIE_DIR.'rss_fetch.inc');


// Output the inner HTML for a core feed splashbox AJAX request.
do_corefeed_html();

function do_corefeed_html() {
	$html = '';

	$rss = fetch_rss('https://www.nagios.org/backend/feeds/corepromo');
	// Output an unordered list of the first n items if any were returned.
	if ($rss && $rss->items) {
		$html = '<ul>';

		$x = 0;
		foreach ($rss->items as $item) {
			if (++$x > 3) break; // Only 3 items.

			$html .= "<li>{$item['description']}</li>";
		}

		$html .= '</ul>';
	}
	else {
		$html = "Unable to fetch the Nagios Core feed. Stay on top of what's happening by visiting <a href='https://www.nagios.org/' target='_blank'>https://www.nagios.org/</a>.";
	}

	print $html;
}
