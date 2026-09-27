<?php
// Standalone navigation. The index.php shell renders the same menu inline;
// this page is kept for old bookmarks and custom framesets.
include_once(dirname(__FILE__).'/includes/utils.inc.php');
include_once(dirname(__FILE__).'/includes/nav.inc.php');

$this_version = '4.5.14';
$link_target = 'main';
// empty theme class: follow the operating system setting
$theme = isset($cfg['theme']) ? $cfg['theme'] : 'auto';
if ($theme != 'dark' && $theme != 'light') {
	$theme = '';
}
?>
<!DOCTYPE html>
<html id="side" class="<?= $theme ?>">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="ROBOTS" content="NOINDEX, NOFOLLOW">
<title>Nagios Core</title>
<link href="stylesheets/common.css?<?php echo $this_version; ?>" type="text/css" rel="stylesheet">
</head>
<body class="navbar">

<div class="navbarlogo">
	<a href="https://www.nagios.org" target="_blank"><div class="fulllogo nagioslogo"></div></a>
</div>

<?php print_nav($cfg, $link_target); ?>

</body>
</html>
