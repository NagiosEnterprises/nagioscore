<?php 

// RSS reader
define('MAGPIE_DIR', './includes/rss/');
define('MAGPIE_CACHE_ON', 0);
define('MAGPIE_CACHE_AGE', 0);
define('MAGPIE_CACHE_DIR', '/tmp/magpie_cache');
require_once(MAGPIE_DIR.'rss_fetch.inc');


// Output the inner HTML for a newsfeed splashbox AJAX request.
do_newsfeed_html();

function do_newsfeed_html() {
	$html = '';

	$rss = fetch_rss('https://www.nagios.org/backend/feeds/frontpage');
	// Output an unordered list of the first n items if any were returned.
	if ($rss && $rss->items) {
		$html = '<ul>';

		$x = 0;
		foreach ($rss->items as $item) {
			if (++$x > 3) break; // Only 3 items.

			$href  = $item['link'];
			$title = $item['title'];
			$html .= "<li><a href='$href' target='_blank'>$title</a></li>";
		}

		$html .= '
		<li><a href="https://www.nagios.org/news" target="_blank">More news...</a></li>
		</ul>';
	}
	else {
		$html = "Unable to fetch the latest Nagios news. Stay on top of what's happening by visiting <a href='https://www.nagios.org/news' target='_blank'>https://www.nagios.org/news</a>.";
	}

	print $html;
}
