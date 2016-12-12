<?php
// Allow specifying main window URL for permalinks, etc.
$url = 'main.php';

if (isset($_GET['corewindow'])) {

	// The default window url may have been overridden with a permalink...
	// Parse the URL and remove permalink option from base.
	$a = parse_url($_GET['corewindow']);

	// ignore host and build base url using just the path to avoid malicious host redirect.
	if(isset($a['path']) && $a['path'] != '/'){
		$url = htmlentities($a['path']).'?';
	}else{
		$url = '/main.php?';
	}

	$query = isset($a['query']) ? $a['query'] : '';
	$pairs = explode('&', $query);
	foreach ($pairs as $pair) {
		$v = explode('=', $pair);
		if (is_array($v)) {
			$key = urlencode($v[0]);
			$val = urlencode(isset($v[1]) ? $v[1] : '');
			$url .= "&$key=$val";
		}
	}
	if (preg_match("/^http:\/\/|^https:\/\/|^\//", $url) != 1)
		$url = "main.php";
}

$this_year = '2016';
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Frameset//EN" "http://www.w3.org/TR/html4/frameset.dtd">

<html>
<head>
	<meta name="ROBOTS" content="NOINDEX, NOFOLLOW">
<script LANGUAGE="javascript">
	var n = Math.round(Math.random() * 10000000000);
	document.write("<title>Nagios Core on " + window.location.hostname + "</title>");
	document.cookie = "NagFormId=" + n.toString(16);
</script>
	<link rel="shortcut icon" href="images/favicon.ico" type="image/ico">
</head>

<frameset cols="180,*" style="border: 0px;">
	<frame src="side.php" name="side" frameborder="0" style="">
	<frame src="<?php echo $url; ?>" name="main" frameborder="0" style="">

	<noframes>
		<!-- This page requires a web browser which supports frames. -->
		<h2>Nagios Core</h2>
		<p align="center">
			<a href="https://www.nagios.org/">www.nagios.org</a><br>
			Copyright &copy; 2010-<?php echo $this_year; ?> Nagios Core Development Team and Community Contributors.
			Copyright &copy; 1999-2010 Ethan Galstad<br>
		</p>
		<p>
			<i>Note: These pages require a browser which supports frames</i>
		</p>
	</noframes>
</frameset>

</html>
