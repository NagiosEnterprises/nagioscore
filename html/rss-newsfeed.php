<?php 

// RSS reader
define('MAGPIE_DIR', './includes/rss/');
define('MAGPIE_CACHE_ON', 0);
define('MAGPIE_CACHE_AGE', 0);
define('MAGPIE_CACHE_DIR', '/tmp/magpie_cache');
require_once(MAGPIE_DIR.'rss_fetch.inc');


//build splash divs to ajax load 
do_newsfeed_html();



function do_newsfeed_html() {

	$url="http://www.nagios.org/backend/feeds/frontpage/";
	$rss=fetch_rss($url);
	if($rss) {
		$x=0;
		$html = "
		<ul>\n"; 
	
		foreach ($rss->items as $item){
			$x++;
			if($x>3)
				break;
			$href = $item['link'];
			$title = $item['title'];	
			$html .="<li><a href='$href' target='_blank'>$title</a></li>";
			}
		$html .='
		<li><a href="http://www.nagios.org/news" target="_blank">More news...</a></li>
		</ul>'; 
		
		print $html; 
		}
	else{
		$html = "
		An error occurred while trying to fetch the latest Nagios news.  Stay on top of what's happening by visiting <a href='http://www.nagios.org/news' target='_blank'>http://www.nagios.org/news</a>.
		";
		print $html;
		}
	}


?>