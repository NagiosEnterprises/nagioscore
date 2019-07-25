<!DOCTYPE html>
<html ng-app="mapApp">

<?php
	include_once(dirname(__FILE__).'/includes/utils.inc.php');
	get_update_information();
?>

	<head>
		<meta charset="utf-8"/>
<?php
	$img = "";
	if (isset($cfg["statusmap_background_image"])) {
		$img = "images/" . $cfg["statusmap_background_image"];
		echo "\t\t<style>\n";
		echo "\t\t\tbody.hasBgImage { background: url('$img');\n";
		echo "\t\t</style>\n";
	}

	$layout = 6;
	if (isset($cfg["default_statusmap_layout"])) {
		$layout = $cfg["default_statusmap_layout"];
		if (is_numeric($layout))
			$layout = $layout + 0;
		else
			$layout = -1;
		if ($layout > 10 || $layout < 0)
			$layout = 6;
	}

	/* This allows a user supplied layout */
	if (
		filter_input(INPUT_GET, 'layout', FILTER_VALIDATE_INT) === 0 ||
		filter_input(INPUT_GET, 'layout', FILTER_VALIDATE_INT, array(
			"options" => array("min_range"=>1, "max_range"=>10)
			)
		)
	) {
		$layout = $_GET['layout'];
	}

	if ($layout == 4)
		$layout = 6;
?>
		<script type="text/javascript">
		map_layout=<?php echo $layout; ?>;
		</script>
		<title>Nagios Map</title>
		<link type="image/ico" rel="shortcut icon" href="images/favicon.ico"/>
		<link type="text/css" rel="stylesheet"
				href="bootstrap-3.3.7/css/bootstrap.min.css">
		<link type="text/css" rel="stylesheet"
				href="bootstrap-3.3.7/css/bootstrap-theme.min.css">
		<link type='text/css' rel='stylesheet' href='stylesheets/common.css'/>
		<link type='text/css' rel='stylesheet' href='stylesheets/map.css'/>
		<link type='text/css' rel='stylesheet' href='stylesheets/map-directive.css'/>
		<link type='text/css' rel='stylesheet' href='stylesheets/nag_funcs.css'/>
		<script type="text/javascript" src="d3/d3.min.js"></script>
		<script type="text/javascript"
				src="angularjs/angular-1.3.9/angular.min.js"></script>
		<script type="text/javascript"
				src="angularjs/ui-bootstrap-tpls-0.14.3.min.js"></script>
		<script type="text/javascript"
				src="angularjs/ui-utils-0.2.3/ui-utils.js"></script>
		<script type="text/javascript" src="spin/spin.min.js"></script>
		<script type="text/javascript" src="js/map.js"></script>
		<script type="text/javascript" src="js/map-directive.js"></script>
		<script type="text/javascript" src="js/map-form.js"></script>
		<script type="text/javascript" src="js/nagios-decorations.js"></script>
		<script type="text/javascript" src="js/nagios-time.js"></script>
		<script type="text/javascript" src="js/jquery-1.12.4.min.js"></script>
		<script type="text/javascript" src="js/nag_funcs.js"></script>

		<?php if ($cfg["enable_page_tour"]) { ?>
			<script type='text/javascript'>
				var vbox;
				var vBoxId = "map";
				var vboxText = "<a href=https://www.nagios.com/tours target=_blank>" +
							"Click here to watch the entire Nagios Core 4 Tour!</a>";
				$(document).ready(function() {
					var user = "<?php echo htmlspecialchars($_SERVER['REMOTE_USER']); ?>";

					vBoxId += ";" + user;
					vbox = new vidbox({pos:'lr',vidurl:'https://www.youtube.com/embed/leaRdb3BElI',
										text:vboxText,vidid:vBoxId});
				});
			</script>
		<?php } ?>

	</head>
	<body ng-controller="mapCtrl" <?php echo $img; ?>>
		<div id="image-cache" style="display: none;"></div>
		<div id="header-container">
			<div info-box cgiurl="{{params.cgiurl}}"
					decoration-title="{{infoBoxTitle()}}"
					update-interval="10"
					last-update="lastUpdate"
					initial-state="collapsed"
					collapsible="true"
					include-partial="map-links.html"
					root="{{params.root}}">
			</div>
		</div>
		<div id="map-container" ng-hide="formDisplayed"
			nagios-map
			cgiurl="{{params.cgiurl}}"
			layout="{{params.layout}}"
			dimensions="{{params.dimensions}}"
			ulx="{{params.ulx}}"
			uly="{{params.uly}}"
			lrx="{{params.lrx}}"
			lry="{{params.lry}}"
			root="params.root"
			maxzoom="params.maxzoom"
			nolinks="{{params.nolinks}}"
			notext="{{params.notext}}"
			nopopups="{{params.nopopups}}"
			noresize="{{params.noresize}}"
			noicons="{{params.noicons}}"
			iconurl="{{params.iconurl}}"
			reload="{{reload}}"
			update-interval="10"
			last-update="lastUpdate"
			map-width="svgWidth"
			map-height="svgHeight"
			build="canBuildMap()">
		</div>

		<div id="menubutton" ng-style="menuButtonStyle()"
				ng-hide="params.nomenu">
			<button type="button" class="btn"
					ng-click="displayForm()">
				<img src="images/menu.png"/>
			</button>
		</div>
	</body>
</html>
