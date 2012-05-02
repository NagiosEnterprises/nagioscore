<?php

// RSS reader
define('MAGPIE_DIR', './includes/rss/');
define('MAGPIE_CACHE_ON', 0);
define('MAGPIE_CACHE_AGE', 0);
define('MAGPIE_CACHE_DIR', '/tmp/magpie_cache');
require_once(MAGPIE_DIR.'rss_fetch.inc');


//build splash divs to ajax load 
do_corefeed_html();





function do_corefeed_html() {

	$url="http://www.nagios.org/backend/feeds/corepromo";
	$rss=fetch_rss($url);
	$x=0;
	//build content string
	if($rss) {
		$html =" 
		<ul>"; 
	
		foreach ($rss->items as $item){
			$x++;
			if($x>3)
				break;
			//$href = $item['link'];
			//$title = $item['title'];	
			$desc = $item['description'];
			$html .="<li>{$item['description']}</li>";
			}
		$html .="</ul>"; 	
		
		print $html; 
		}
	else{
		$html = "
		An error occurred while trying to fetch the Nagios Core feed.  Stay on top of what's happening by visiting <a href='http://www.nagios.org/' target='_blank'>http://www.nagios.org/</a>.
		";
		print $html;
		}
	} 


?>