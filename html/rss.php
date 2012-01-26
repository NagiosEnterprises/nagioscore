<?php //rss.php 

// RSS reader
define('MAGPIE_DIR', './includes/rss/');
define('MAGPIE_CACHE_ON', 0);
define('MAGPIE_CACHE_AGE', 0);
define('MAGPIE_CACHE_DIR', '/tmp/magpie_cache');
require_once(MAGPIE_DIR.'rss_fetch.inc');


//build splash divs to ajax load 
do_splash3_html();
do_splash4_html();





function do_splash3_html() {

	$url="http://www.nagios.org/backend/feeds/corepromo";
	$rss=fetch_rss($url);
	$x=0;
	//build content string
	if($rss) {
		$html =" 
		<!-- splash3 -->
		<div class='splashbox'>
		<h2>Don't Miss...</h2>
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
		$html .="</ul></div>"; 	
		
		print $html; 
		}
	} //end do_splash3_html()


function do_splash4_html() {

	$url="http://www.nagios.org/backend/feeds/frontpage/";
	$rss=fetch_rss($url);
	if($rss) {
		$x=0;
		$html = "
		<!-- splash 4 load -->
		<div class='splashbox'>
		<h2>Latest News</h2>
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
		</ul></div>'; 
		
		print $html; 
		}
	}//end do_splash2_html() 


?>