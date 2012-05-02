<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Frameset//EN" "http://www.w3.org/TR/html4/frameset.dtd">

<html>
<head>
<meta name="ROBOTS" content="NOINDEX, NOFOLLOW">
<title>Nagios Core</title>
<link rel="shortcut icon" href="images/favicon.ico" type="image/ico">
</head>

<?php
 // allow specifying main window URL for permalinks, etc.
$corewindow="main.php";
if(isset($_GET['corewindow'])){
	
	// default window url may have been overridden with a permalink...
	$rawurl=$_GET['corewindow'];
	
	// parse url and remove permalink option from base
	$a=parse_url($rawurl);

	// build base url
	if(isset($a["host"]))
		$windowurl=$a["scheme"]."://".$a["host"].$a["path"]."?";
	else
		$windowurl=$a["path"]."?";
	
	$q="";
	if(isset($a["query"]))
		$q=$a["query"];
		
	$pairs=explode("&",$q);
	foreach($pairs as $pair){
		$v=explode("=",$pair);
		if(is_array($v))
			$windowurl.="&".urlencode($v[0])."=".urlencode(isset($v[1])?$v[1]:"");
		}
	

	$corewindow=$windowurl;
	}
?>


<frameset cols="180,*" style="border: 0px; framespacing: 0px">
<frame src="side.php" name="side" frameborder="0" style="">
<frame src="<?php echo $corewindow;?>" name="main" frameborder="0" style="">

<noframes>
<!-- This page requires a web browser which supports frames. --> 
<h2>Nagios Core</h2>
<p align="center">
<a href="http://www.nagios.org/">www.nagios.org</a><br>
Copyright &copy; 2010-<?php echo date("Y");?> Nagios Core Development Team and Community Contributors.
Copyright &copy; 1999-2010 Ethan Galstad<br>
</p>
<p>
<i>Note: These pages require a browser which supports frames</i>
</p>
</noframes>

</frameset>

</html>

