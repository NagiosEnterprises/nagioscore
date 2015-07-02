<?php

// RSS reader
define('MAGPIE_DIR', './includes/rss/');
define('MAGPIE_CACHE_ON', 0);
define('MAGPIE_CACHE_AGE', 0);
define('MAGPIE_CACHE_DIR', '/tmp/magpie_cache');
require_once(MAGPIE_DIR.'rss_fetch.inc');


// Output the inner HTML for a core feed splashbox AJAX request.
do_corebanner_html();

function do_corebanner_html() {
	$html = ''; // Default to empty on error or no results.

	$rss = fetch_rss('https://www.nagios.org/backend/feeds/corebanner');
	// Output the first item's description if any were returned.
	if ($rss && $rss->items) {
		foreach ($rss->items as $item) {
			$html .= $item['description'];
			break;
		}
	}

	print $html;
}
