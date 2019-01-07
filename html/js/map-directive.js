angular.module("mapApp")
	.directive("nagiosMap", function() {
		return {
			templateUrl: "map-directive.html",
			restrict: "AE",
			scope: {
				cgiurl: "@cgiurl",
				layoutIndex: "@layout",
				dimensions: "@dimensions",
				ulx: "@ulx",
				uly: "@uly",
				lrx: "@lrx",
				lry: "@lry",
				root: "=root",
				maxzoom: "=maxzoom",
				nolinks: "@nolinks",
				notext: "@notext",
				nopopups: "@nopopups",
				noresize: "@noresize",
				noicons: "@noicons",
				iconurl: "@iconurl",
				updateIntervalValue: "@updateInterval",
				lastUpdate: "=lastUpdate",
				reload: "@reload",
				svgWidth: "=mapWidth",
				svgHeight: "=mapHeight",
				build: "&build"
			},
			controller: function($scope, $element, $attrs, $http,
					nagiosProcessName, layouts) {

				// Contents of the popup
				$scope.popupContents = {};

				// Layout variables
				$scope.diameter = Math.min($scope.svgHeight,
						$scope.svgWidth);
				$scope.mapZIndex = 20;
				$scope.popupZIndex = 40;
				$scope.popupPadding = 10;
				$scope.fontSize = 10; // px
				$scope.minRadius = 5;
					// radius of node with zero services
				$scope.maxRadiusCount = 20;
					// number of services at which to max radius
				$scope.maxRadius = 12;
					// radius of node with maxRadiusCount+ services
				$scope.swellRadius = 4;
					// amount by which radius swells when updating
				$scope.nodeID = 0;
					// Incrementing unique node ID for each node

				// Display variables
				$scope.layout = parseInt($scope.layoutIndex);
				$scope.ulx = parseInt($scope.ulxValue);
				$scope.uly = parseInt($scope.ulyValue);
				$scope.lrx = parseInt($scope.lrxValue);
				$scope.lry = parseInt($scope.lryValue);
				$scope.showText = $scope.notext == "false";
				$scope.showLinks = $scope.nolinks == "false";
				$scope.showPopups = $scope.nopopups == "false";
				$scope.allowResize = $scope.noresize == "false";
				$scope.showIcons = $scope.noicons == "false";

				// Resize handle variables
				$scope.handleHeight = 8;
				$scope.handleWidth = 8;
				$scope.handlePadding = 2;

				// Host node tree - initialize the root node
				$scope.hostTree = {
					hostInfo: {
						name: nagiosProcessName,
						objectJSON: {
							name: nagiosProcessName,
							icon_image: "",
							icon_image_alt: "",
							x_2d: 0,
							y_2d: 0
						},
						serviceCount: 0
					},
					saveArc: {
						x: 0,
						dx: 0,
						y: 0,
						dy: 0
					},
					saveLabel: {
						x: 0,
						dx: 0,
						y: 0,
						dy: 0
					}
				};
				$scope.hostList = new Object;

				// Icon information
				$scope.iconList = new Object;
				$scope.iconsLoading = 0;

				// Update frequency
				$scope.updateStatusInterval =
						parseInt($scope.updateIntervalValue) * 1000;

				// Map update variables
				$scope.updateDuration = 0;

				// Date format for popup dates
				$scope.popupDateFormat = d3.time.format("%m-%d-%Y %H:%M:%S");

				// Root node name
				$scope.rootNodeName = nagiosProcessName;
				$scope.rootNode = null;

				// Application state variables
				$scope.fetchingHostlist = false;
				$scope.displayPopup = false;
				var previousLayout = -1;
				var statusTimeout = null;
				var displayMapDone = false;

				// Placeholder for saving icon url
				var previousIconUrl;

				// User-supplied layout information
				var userSuppliedLayout = {
					dimensions: {
						upperLeft: {},
						lowerRight: {}
					},
					xScale: d3.scale.linear(),
					yScale: d3.scale.linear()
				}

				// Force layout information
				var forceLayout = new Object;

				// Watch for noresize update
				$scope.$watch("noresize", function() {
					$scope.allowResize = $scope.noresize == "false";
					if ($scope.allowResize) {
						d3.select("#resize-handle").style("visibility", "visible");
					} else {
						d3.select("#resize-handle").style("visibility", "hidden");
					}
				})

				// Watch for changes on the reload value
				$scope.$watch("reload", function(newValue) {

					// Cancel the timeout if necessary
					if (statusTimeout != null) {
						clearTimeout(statusTimeout);
					}

					// Clean up after previous maps
					var selectionExit;
					switch (previousLayout) {
					case layouts.UserSupplied.index:
						selectionExit = d3.select("g#container")
							.selectAll("g.node")
							.data([])
							.exit();
						selectionExit.selectAll("circle").remove();
						selectionExit.selectAll("text").remove();
						selectionExit.remove();
						break;
					case layouts.DepthLayers.index:
					case layouts.DepthLayersVertical.index:
					case layouts.CollapsedTree.index:
					case layouts.CollapsedTreeVertical.index:
					case layouts.BalancedTree.index:
					case layouts.BalancedTreeVertical.index:
					case layouts.CircularBalloon.index:
						selectionExit = d3.select("g#container")
							.selectAll(".node")
							.data([])
							.exit();
						selectionExit.selectAll("circle").remove();
						selectionExit.selectAll("text").remove();
						selectionExit.remove();
						d3.select("g#container")
							.select("g#links")
							.selectAll(".link")
							.data([])
							.exit()
							.remove();
						d3.select("g#links").remove();
						break;
					case layouts.CircularMarkup.index:
						d3.select("g#container")
							.select("g#paths")
							.selectAll("path")
							.data([])
							.remove();
						selectionExit = d3.select("g#container")
							.selectAll("g.label")
							.data([])
							.exit();
						selectionExit.selectAll("rect").remove();
						selectionExit.selectAll("text").remove();
						selectionExit.remove();
						d3.select("g#paths").remove();
						break;
					case layouts.Force.index:
						selectionExit = d3.select("g#container")
							.selectAll(".link")
							.data([])
							.exit();
						selectionExit.selectAll("line").remove();
						selectionExit.remove();
						selectionExit = d3.select("g#container")
							.selectAll("g.node")
							.data([])
							.exit();
						selectionExit.selectAll("circle").remove();
						selectionExit.selectAll("text").remove();
						selectionExit.remove();
						d3.select("g#links").remove();
						break;
					}

					// Clean up the host list
					$scope.hostList = {};

					// Clean up the icon image cache if the icon url
					// has changed
					if (previousIconUrl != $scope.iconurl) {
						d3.selectAll("div#image-cache img").remove();
						$scope.iconList = new Object;
						$scope.iconsLoading = 0;
					}
					previousIconUrl = $scope.iconurl;

					// Reset the zoom and pan
					$scope.zoom.translate([0,0]).scale(1);
					// Show the map
					if ($scope.build()) {

						// Determine the new layout
						$scope.layout = parseInt($scope.layoutIndex);

						// Adjust the container appropriately
						d3.select("svg#map g#container")
							.attr({
								transform: function() {
									return getContainerTransform();
								}
							});

						// Layout-specific steps
						switch ($scope.layout) {
						case layouts.UserSupplied.index:
							userSuppliedLayout.dimensionType = $scope.dimensions
							break;
						case layouts.DepthLayers.index:
						case layouts.DepthLayersVertical.index:
						case layouts.CollapsedTree.index:
						case layouts.CollapsedTreeVertical.index:
						case layouts.BalancedTree.index:
						case layouts.BalancedTreeVertical.index:
						case layouts.CircularBalloon.index:
						case layouts.CircularMarkup.index:
						case layouts.Force.index:
							break;
						}
						previousLayout = $scope.layout;

						// Start the spinner
						$scope.spinnerdiv = d3.select("div#spinner");
						$scope.fetchingHostlist = true;
						$scope.spinner = new Spinner($scope.spinnerOpts)
								.spin($scope.spinnerdiv[0][0]);

						// Get the host list and move forward
						getHostList();
					}
				});

				// Watch for changes in the size of the map
				$scope.$watch("svgWidth", function(newValue) {
					if (displayMapDone) {
						updateOnResize(d3.select("#resize-handle").node());
					}
				});
				$scope.$watch("svgHeight", function(newValue) {
					if (displayMapDone) {
						updateOnResize(d3.select("#resize-handle").node());
					}
				});

				// Get the services of the children of a specific node
				var getServiceList = function() {

					var parameters = {
						query: "servicelist",
						formatoptions: "enumerate bitmask",
						details: false
					};

					var getConfig = {
						params: parameters,
						withCredentials: true
					};

					if ($scope.showIcons && $scope.iconsLoading > 0) {
						setTimeout(function() {
								getServiceList()
								}, 10);
						return;
					}

					// Send the JSON query
					$http.get($scope.cgiurl + "objectjson.cgi", getConfig)
						.error(function(err) {
							console.warn(err);
						})
						.success(function(json) {
							// Record the time of the last update
							$scope.lastUpdate = json.result.query_time;

							for(var host in json.data.servicelist) {
								$scope.hostList[host].serviceCount =
										json.data.servicelist[host].length;
							}
						});
				};

				// Take action on the zoom start
				var onZoomStart = function() {

					// Hide the popup window
					$scope.displayPopup = false;
					$scope.$apply("displayPopup");
				};

				// Take action on the zoom
				var onZoom = function() {

					// Get the event parameters
					var zoomTranslate = $scope.zoom.translate();
					var zoomScale = $scope.zoom.scale();

					var translate = [];
					switch($scope.layout) {
					case layouts.UserSupplied.index:
						d3.selectAll("g.node")
							.attr({
								transform: function(d) {
									return getNodeTransform(d);
								}
							});
						d3.selectAll("g.node text")
							.each(function(d) {
								setTextAttrs(d, this);
							});
						break;
					case layouts.DepthLayers.index:
					case layouts.DepthLayersVertical.index:
					case layouts.CollapsedTree.index:
					case layouts.CollapsedTreeVertical.index:
					case layouts.BalancedTree.index:
					case layouts.BalancedTreeVertical.index:
						d3.selectAll("path.link")
							.attr({
								d: $scope.diagonal
							});
						d3.selectAll("g.node")
							.attr({
								transform: function(d) {
									return getNodeTransform(d);
								}
							});
						break;
					case layouts.CircularBalloon.index:
						// Calculate the real translation taking
						// into account the centering
						translate = [zoomTranslate[0] +
								($scope.svgWidth / 2) * zoomScale,
								zoomTranslate[1] +
								($scope.svgHeight / 2) * zoomScale];
						d3.select("svg#map g#container")
							.attr("transform", "translate(" + translate +
									")");
						d3.selectAll("path.link")
							.attr({
								d: $scope.diagonal
							});
						d3.selectAll("g.node")
							.attr({
								transform: function(d) {
									return getNodeTransform(d);
								}
							});
						break;
					case layouts.CircularMarkup.index:
						// Calculate the real translation taking
						// into account the centering
						translate = [zoomTranslate[0] +
								($scope.svgWidth / 2) * zoomScale,
								zoomTranslate[1] +
								($scope.svgHeight / 2) * zoomScale];
						// Update the group with the new calculated values
						d3.select("svg#map g#container")
							.attr("transform",
									"translate(" + translate + ")");
						d3.selectAll("path")
							.attr("transform", "scale(" + zoomScale + ")");
						d3.selectAll("g.label")
							.attr({
								transform: function(d) {
									return getPartitionLabelGroupTransform(d);
								}
							});
						break;
					case layouts.Force.index:
						d3.selectAll("line.link")
							.attr({
								x1: function(d) {
									return $scope.xZoomScale(d.source.x);
								},
								y1: function(d) {
									return $scope.yZoomScale(d.source.y);
								},
								x2: function(d) {
									return $scope.xZoomScale(d.target.x);
								},
								y2: function(d) {
									return $scope.yZoomScale(d.target.y);
								}
							});
						d3.selectAll("g.node")
							.attr({
								transform: function(d) {
									return "translate(" +
											$scope.xZoomScale(d.x) + ", " +
											$scope.yZoomScale(d.y) + ")";
								}
							});
						break;
					}
				};

				// Get the tree size
				var getTreeSize = function() {

					switch($scope.layout) {
					case layouts.DepthLayers.index:
						return [$scope.svgWidth, $scope.svgHeight -
								layouts.DepthLayers.topPadding -
								layouts.DepthLayers.bottomPadding];
						break;
					case layouts.DepthLayersVertical.index:
						return [$scope.svgHeight, $scope.svgWidth -
								layouts.DepthLayersVertical.leftPadding -
								layouts.DepthLayersVertical.rightPadding];
						break;
					case layouts.CollapsedTree.index:
						return [$scope.svgWidth, $scope.svgHeight -
								layouts.CollapsedTree.topPadding -
								layouts.CollapsedTree.bottomPadding];
						break;
					case layouts.CollapsedTreeVertical.index:
						return [$scope.svgHeight, $scope.svgWidth -
								layouts.CollapsedTreeVertical.leftPadding -
								layouts.CollapsedTreeVertical.rightPadding];
						break;
					case layouts.BalancedTree.index:
						return [$scope.svgWidth, $scope.svgHeight -
								layouts.BalancedTree.topPadding -
								layouts.BalancedTree.bottomPadding];
						break;
					case layouts.BalancedTreeVertical.index:
						return [$scope.svgHeight, $scope.svgWidth -
								layouts.BalancedTreeVertical.leftPadding -
								layouts.BalancedTreeVertical.rightPadding];
						break;
					case layouts.CircularBalloon.index:
						return [360, $scope.diameter / 2 -
								layouts.CircularBalloon.outsidePadding];
						break;
					}
				};

				// Get the node transform
				var getNodeTransform = function(d) {

					switch($scope.layout) {
					case layouts.UserSupplied.index:
						var x1 = d.hostInfo.objectJSON.x_2d;
						var x2 = userSuppliedLayout.xScale(x1);
						var x3 = $scope.xZoomScale(x2);
						var y1 = d.hostInfo.objectJSON.y_2d;
						var y2 = userSuppliedLayout.yScale(y1);
						var y3 = $scope.yZoomScale(y2);
						return "translate(" + x3 + "," + y3 + ")";
						break;
					case layouts.DepthLayers.index:
					case layouts.CollapsedTree.index:
					case layouts.BalancedTree.index:
						return "translate(" + $scope.xZoomScale(d.x) + "," +
								$scope.yZoomScale(d.y) + ")";
						break;
					case layouts.DepthLayersVertical.index:
					case layouts.CollapsedTreeVertical.index:
					case layouts.BalancedTreeVertical.index:
						return "translate(" + $scope.xZoomScale(d.y) + "," +
								$scope.yZoomScale(d.x) + ")";
						break;
					case layouts.CircularBalloon.index:
						if(d.y == 0) return "";
						var rotateAngle = d.x +
								layouts.CircularBalloon.rotation;
						var translate = d.y * $scope.zoom.scale();
						return "rotate(" + rotateAngle + ") translate(" +
								translate + ")";
						break;
					}
				};

				// Determine the amount of text padding due to an icon
				var getIconTextPadding = function(d) {
					var iconHeight = 0, iconWidth = 0;
					if (d.hostInfo.hasOwnProperty("iconInfo")) {
						iconHeight = d.hostInfo.iconInfo.height;
						iconWidth = d.hostInfo.iconInfo.width;
					}
					else {
						return 0;
					}
					switch($scope.layout) {
					case layouts.UserSupplied.index:
						switch(layouts.UserSupplied.textAlignment) {
						case "above":
						case "below":
							return iconHeight / 2;
							break;
						case "left":
						case "right":
							return iconWidth / 2;
							break;
						}
						break;
					case layouts.DepthLayers.index:
					case layouts.CollapsedTree.index:
					case layouts.BalancedTree.index:
						return iconHeight / 2;
						break;
					case layouts.DepthLayersVertical.index:
					case layouts.CollapsedTreeVertical.index:
					case layouts.BalancedTreeVertical.index:
						return iconWidth / 2;
						break;
					case layouts.CircularBalloon.index:
						var rotateAngle = d.x +
								layouts.CircularBalloon.rotation;
						var angle; // angle used to calculate distance
						var r; // radius used to calculate distance
						if (rotateAngle < 45.0) {
							// Text is right of icon
							angle = rotateAngle;
							r = iconWidth / 2;
						}
						else if(rotateAngle < 135.0) {
							// Text is below icon
							angle = Math.abs(90.0 - rotateAngle);
							r = iconHeight / 2;
						}
						else if(rotateAngle < 225.0) {
							// Text is left icon
							angle = Math.abs(180.0 - rotateAngle);
							r = iconWidth / 2;
						}
						else if(rotateAngle < 315.0) {
							// Text is above icon
							angle = Math.abs(270.0 - rotateAngle);
							r = iconHeight / 2;
						}
						else {
							// Text is right of icon
							angle = 360.0 - rotateAngle;
							r = iconWidth / 2;
						}
						var radians = angle * Math.PI / 180.0;
						var cos = Math.cos(radians);
						return r + (r - r * cos) * cos;
						break;
					case layouts.CircularMarkup.index:
						return 0;
						break;
					case layouts.Force.index:
						return iconWidth / 2;
						break;
					}
				};

				// Set the node label attributes
				var setTextAttrs = function(d, domNode) {

					// Placeholder for attributes
					var attrs = new Object;
					var state = "ok";
					var stateCounts = {};

					// Variables used for all layouts
					var serviceCount = getObjAttr(d, ["serviceCount"], 0);
					var iconTextPadding = getIconTextPadding(d);
					var fontSize = $scope.fontSize + "px";

					if (d.hostInfo.name == $scope.$parent.search.host)
						fontSize = ($scope.fontSize * 2) + "px";
					attrs["font-size"] = fontSize;
					attrs["font-weight"] = "normal";
					attrs["text-decoration"] = "none";
					attrs["fill"] = "#000000";

					if (d.hostInfo.name != $scope.$parent.search.host && d.hostInfo.hasOwnProperty("serviceStatusJSON")) {
						for (var service in d.hostInfo.serviceStatusJSON) {
							var state = d.hostInfo.serviceStatusJSON[service];
							if(!stateCounts.hasOwnProperty(state))
								stateCounts[state] = 0;
							stateCounts[state]++;
						}
						if (stateCounts["critical"])
							state = "critical";
						else if (stateCounts["warning"])
							state = "warning";
						else if (stateCounts["unknown"])
							state = "unknown";
						else if (stateCounts["pending"])
							state = "pending";
					}

					switch($scope.layout) {
					case layouts.UserSupplied.index:
						var textPadding =
								layouts.UserSupplied.textPadding[layouts.UserSupplied.textAlignment];
						if (!d.hostInfo.hasOwnProperty("iconInfo")) {
							textPadding += $scope.nodeScale(serviceCount);
						}
						var x = 0;
						var y = 0;
						switch(layouts.UserSupplied.textAlignment) {
						case "above":
							y = -(textPadding + iconTextPadding);
							attrs["text-anchor"] = "middle";
							break;
						case "left":
							x = -(textPadding + iconTextPadding);
							attrs["text-anchor"] = "end";
							attrs.dy = ".4em";
							break;
						case "right":
							x = textPadding + iconTextPadding;
							attrs["text-anchor"] = "start";
							attrs.dy = ".4em";
							break;
						case "below":
							y = textPadding + iconTextPadding;
							attrs["text-anchor"] = "middle";
							break;
						}
						attrs.transform = "translate(" + x + "," + y + ")";
						break;
					case layouts.DepthLayers.index:
						var textPadding = $scope.nodeScale(serviceCount) +
								layouts.DepthLayers.dyText + iconTextPadding;
						attrs.dy = d.children ? -textPadding : 0;
						attrs.transform = d.children ? "" :
								"rotate(90) translate(" + textPadding +
								", " + (($scope.fontSize / 2) - 1) + ")";
						attrs["text-anchor"] = d.children ? "middle" :
								"start";
						break;
					case layouts.DepthLayersVertical.index:
						var textPadding = $scope.nodeScale(serviceCount) +
								layouts.DepthLayersVertical.dxText +
								iconTextPadding;
						attrs.dx = d.children ? -textPadding : textPadding;
						attrs.dy = layouts.DepthLayersVertical.dyText;
						attrs["text-anchor"] = d.children ? "end" : "start";
						break;
					case layouts.CollapsedTree.index:
						var textPadding = $scope.nodeScale(serviceCount) +
								layouts.CollapsedTree.dyText +
								iconTextPadding;
						attrs.dy = d.children ? -textPadding : 0;
						attrs.transform = d.children ? "" :
								"rotate(90) translate(" + textPadding +
								", " + (($scope.fontSize / 2) - 1) + ")";
						attrs["text-anchor"] = d.children ? "middle" :
								"start";
						break;
					case layouts.CollapsedTreeVertical.index:
						var textPadding = $scope.nodeScale(serviceCount) +
								layouts.CollapsedTreeVertical.dxText +
								iconTextPadding;
						attrs.dx = d.children ? -textPadding : textPadding;
						attrs.dy = layouts.CollapsedTreeVertical.dyText;
						attrs["text-anchor"] = d.children ? "end" : "start";
						break;
					case layouts.BalancedTree.index:
						var textPadding = $scope.nodeScale(serviceCount) +
								layouts.BalancedTree.dyText +
								iconTextPadding;
						attrs.dy = d.children ? -textPadding : 0;
						attrs.transform = d.children ? "" :
								"rotate(90) translate(" + textPadding +
								", " + (($scope.fontSize / 2) - 1) + ")";
						attrs["text-anchor"] = d.children ? "middle" :
								"start";
						break;
					case layouts.BalancedTreeVertical.index:
						var textPadding = $scope.nodeScale(serviceCount) +
								layouts.BalancedTreeVertical.dxText +
								iconTextPadding;
						attrs.dx = d.children ? -textPadding : textPadding;
						attrs.dy = layouts.BalancedTreeVertical.dyText;
						attrs["text-anchor"] = d.children ? "end" : "start";
						break;
					case layouts.CircularBalloon.index:
						var textPadding = $scope.nodeScale(serviceCount) +
								layouts.CircularBalloon.textPadding +
								iconTextPadding;
						if(d.y == 0) {
							attrs["text-anchor"] = "middle";
							attrs.transform = "translate(0,-" +
									$scope.fontSize + ")";
						}
						else if(d.x < 180) {
							attrs["text-anchor"] = "start";
							attrs.transform = "translate(" + textPadding +
									")";
						}
						else {
							attrs["text-anchor"] = "end";
							attrs.transform = "rotate(180) translate(-" +
									textPadding + ")";
						}
						attrs.dy = layouts.CircularBalloon.dyText;
						break;
					case layouts.CircularMarkup.index:
						attrs["alignment-baseline"] = "middle";
						attrs["text-anchor"] = "middle";
						attrs["transform"] = "";
						if (d.hostInfo.hasOwnProperty("iconInfo")) {
							var rotateAngle = (d.x + d.dx / 2) * 180 / Math.PI +
									layouts.CircularBalloon.rotation;
							var translate = (d.hostInfo.iconInfo.height +
									layouts.CircularMarkup.textPadding) / 2;
							attrs["transform"] = "rotate(" + -rotateAngle +
									") translate(0, " + translate + ")";
						}
						else {
							if (d.depth > 0 && d.x + d.dx / 2 > Math.PI) {
								attrs["transform"] = "rotate(180)";
							}
						}
						break;
					case layouts.Force.index:
						attrs["alignment-baseline"] = "middle";
						attrs["x"] = $scope.nodeScale(serviceCount) +
								layouts.Force.textPadding + iconTextPadding;
						break;
					}

					if (d.hostInfo.name == $scope.$parent.search.host) {
						attrs["font-weight"] = "bold";
						attrs["stroke"] = "red";
						attrs["stroke-width"] = "1";
						attrs["fill"] = "#0000ff";
					} else if (state != "ok") {
						attrs["font-weight"] = "bold";
						attrs["text-decoration"] = "underline";
						switch(state) {
							case "critical":attrs["fill"] = "#ff0000";	break;
							case "warning":	attrs["fill"] = "#b0b214";	break;
							case "unknown":	attrs["fill"] = "#ff6419";	break;
							case "pending":	attrs["fill"] = "#cccccc";	break;
						}
					}
					d3.select(domNode).attr(attrs);
				};

				// Get the quadrant of the mouse pointer within the svg
				var getQuadrant = function(mouse, bcr) {

					var quadrant = 0;

					// mouse is relative to body -
					// convert to relative to svg
					var mouseX = mouse[0] - bcr.left;
					var mouseY = mouse[1] - bcr.top;

					if(mouseX < ((bcr.width - bcr.left) / 2)) {
						// Left half of svg
						if(mouseY < ((bcr.height - bcr.top) / 2)) {
							// Top half of svg
							quadrant = 2;
						}
						else {
							// Bottom half of svg
							quadrant = 3;
						}
					}
					else {
						// Right half of svg
						if(mouseY < ((bcr.height - bcr.top) / 2)) {
							// Top half of svg
							quadrant = 1;
						}
						else {
							// Bottom half of svg
							quadrant = 4;
						}
					}

					return quadrant;
				};

				// Display the popup
				var displayPopup = function(d) {

					// Get the mouse position relative to the body
					var body = d3.select("body");
					var mouse = d3.mouse(body.node());

					// Get the bounding client rect of the div
					// containing the map
					var bcr = d3.select("div#mapsvg")
						.node()
						.getBoundingClientRect();

					// Hide the popup by setting is z-index to
					// less than that of the map div and by
					// centering it under the map div
					var popup = d3.select("#popup")
						.style({
							"z-index": $scope.mapZIndex - 1,
							left: $scope.svgWidth / 2 + "px",
							top: $scope.svgHeight / 2 + "px"
						});

					// Set it's contents and "display" it (it's still not
					// visible because of it's z-index)
					setPopupContents(popup, d);
					$scope.displayPopup = true;
					$scope.$apply("displayPopup");

					// Now that it's "displayed", we can get it's size and
					// calculate it's proper placement. Do so and set it's
					// z-index so it is displayed
					var popupBR = popup[0][0].getBoundingClientRect();
					var left;
					var top;
					switch(getQuadrant(mouse, bcr)) {
					case 1:
						left = mouse[0] - bcr.left - popupBR.width -
								$scope.popupPadding;
						top = mouse[1] - bcr.top + $scope.popupPadding;
						break;
					case 2:
						left = mouse[0] - bcr.left + $scope.popupPadding;
						top = mouse[1] - bcr.top + $scope.popupPadding;
						break;
					case 3:
						left = mouse[0] - bcr.left + $scope.popupPadding;
						top = mouse[1] - bcr.top - popupBR.height -
								$scope.popupPadding;
						break;
					case 4:
						left = mouse[0] - bcr.left - popupBR.width -
								$scope.popupPadding;
						top = mouse[1] - bcr.top - popupBR.height -
								$scope.popupPadding;
						break;
					default:	// use first quadrant settings
						left = mouse[0] - bcr.left - popupBR.width -
								$scope.popupPadding;
						top = mouse[1] - bcr.top + $scope.popupPadding;
						break;
					}
					popup.style({
						"z-index": $scope.popupZIndex,
						left: left + "px",
						top: top + "px"
					});
				};

				// Prune any deleted hosts from the host tree
				var pruneHostTree = function(node) {

					if(node.hasOwnProperty("children")) {
						node.children = node.children.filter(function(e) {
							return e.hostInfo != null;
						});
						node.children.forEach(function(e) {
							pruneHostTree(e);
						});
					}
				};

				// Sort the children of a node recursively
				var sortChildren = function(node) {

					if (node.hasOwnProperty("children")) {
						// First sort the children
						node.children.sort(function(a, b) {
							if (a.hostInfo.objectJSON.name <
									b.hostInfo.objectJSON.name) {
								return -1;
							}
							else if (a.hostInfo.objectJSON.name >
									b.hostInfo.objectJSON.name) {
								return 1;
							}
							return 0;
						});

						// Next sort the children of each of these nodes
						node.children.forEach(function(e, i, a) {
							sortChildren(e);
						});
					}
				};

				// Re-parent the tree with a new root
				var reparentTree = function(node) {

					// The specified node becomes the new node and all
					// it's children remain in place relative to it
					var newTree = node;

					// Visit each parent of the specified node
					var currentNode = node;
					while (!(currentNode === $scope.hostTree)) {
						// First record the parent node of the current node
						var parent = currentNode.parent;

						// Fix root nodes with no parent nodes
						if (parent === undefined) {
							return true;
						}

						// Next remove the current node as a child of
						// the parent node
						parent.children = parent.children.filter(function(e, i, a) {
							if (e === currentNode) {
								return false;
							}
							return true;
						});

						// Finally add the parent as a child
						// to the current node
						if (!currentNode.hasOwnProperty("children")) {
							currentNode.children = new Array;
						}
						currentNode.children.push(parent);

						// Set the current node the former parent of the
						// former current node
						currentNode = parent;
					}

					// Now sort the nodes in the tree
					sortChildren(newTree);

					// Record the host name for the root node
					$scope.rootNodeName = newTree.hostInfo.name;
					$scope.rootNode = newTree;

					// Assign the new tree
					$scope.hostTree = newTree;
					$scope.focalPoint = newTree;
				};

				// Toggle a node
				var toggleNode = function(d) {

					if (d.children) {
						d._children = d.children;
						d.children = null;
						d.collapsed = true;
					}
					else {
						switch($scope.layout) {
						case layouts.CircularMarkup.index:
							updateToggledNodes($scope.hostTree,
									updateDescendantsOnExpand);
							break;
						}
						d.children = d._children;
						d._children = null;
						d.collapsed = false;
					}
				};

				// Interpolate the arcs in data space.
				var arcTween = function(a) {
					var i = d3.interpolate({x: a.saveArc.x,
							dx: a.saveArc.dx, y: a.saveArc.y,
							dy: a.saveArc.dy}, a);
					return function(t) {
						var b = i(t);
						a.saveArc.x = b.x;
						a.saveArc.dx = b.dx;
						a.saveArc.y = b.y;
						a.saveArc.dy = b.dy;
						return $scope.arc(b);
					};
				}

				// Interpolate the node labels in data space.
				var labelGroupTween = function(a) {
					var i = d3.interpolate({x: a.saveLabel.x,
							dx: a.saveLabel.dx, y: a.saveLabel.y,
							dy: a.saveLabel.dy}, a);
					return function(t) {
						var b = i(t);
						a.saveLabel.x = b.x;
						a.saveLabel.dx = b.dx;
						a.saveLabel.y = b.y;
						a.saveLabel.dy = b.dy;
						return getPartitionLabelGroupTransform(b);
					};
				}

				// Get the partition map label group transform
				var getPartitionLabelGroupTransform = function(d) {

					var radians = d.x + d.dx / 2;
					var rotate = (radians * 180 / Math.PI) - 90;
					var exponent = 1 / layouts.CircularMarkup.radialExponent;
					var radius = d.y + (d.y / (d.depth * 2));
					var translate = Math.pow(radius, exponent) * $scope.zoom.scale();
					var transform = "";

					if(d.depth == 0) {
						transform = "";
					}
					else {
						transform = "rotate(" + rotate + ")" +
								" translate(" + translate + ")";
					}
					return transform;
				};

				// Find a host in a sorted array of hosts
				var findElement = function(list, key, accessor) {

					var start = 0;
					var end = list.length - 1;

					while (start < end) {
						var midpoint = parseInt(start +
								(end - start + 1) / 2);
						if (accessor(list, midpoint) == key) {
							return midpoint;
						}
						else if (key < accessor(list, midpoint)) {
							end = midpoint - 1;
						}
						else {
							start = midpoint + 1;
						}
					}
					return null;
				};

				// Update a node in the host tree
				var updateHostTree = function(node, hosts) {

					// Sort the hosts array
					hosts.sort();

					// First remove any children of the node that are not
					// in the list of hosts
					if (node.hasOwnProperty("children") &&
								node.children != null) {
						node.children = node.children.filter(function(e) {
							return findElement(hosts, e.hostInfo.name,
									function(list, index) {
										return list[index];
									}) != null;
						});
						// Sort the remaining children
						node.children.sort(function(a, b) {
							if (a.hostInfo.name == b.hostInfo.name) {
								return 0;
							}
							else if (a.hostInfo.name < b.hostInfo.name) {
								return -1;
							}
							else {
								return 1;
							}
						});
					}

					if (!node.hasOwnProperty("children") || node.children == null) {
						node.children = new Array;
					}

					// Next add any hosts in the list as children
					// of the node, if they're not already
					hosts.forEach(function(e) {
						var childIndex = node.children.findIndex(function(s) {
							return s.hostInfo.name === e;
						});
								
						if ($scope.hostList[e]) {

							if (childIndex === -1) {

								// Create the node object
								var hostNode = new Object;

								// Point the node's host info to the entry in
								// the host list
								hostNode.hostInfo = $scope.hostList[e];

								// And vice versa
								if (!$scope.hostList[e].hasOwnProperty("hostNodes")) {
									$scope.hostList[e].hostNodes = new Array;
								}
								if (!$scope.hostList[e].hostNodes.reduce(function(a, b) {
										return a && b === hostNode; }, false)) {
									$scope.hostList[e].hostNodes.push(hostNode);
								}

								// Set the parent of this node
								hostNode.parent = node;

								// Initialize layout information for transitions
								hostNode.saveArc = new Object;
								hostNode.saveArc.x = 0;
								hostNode.saveArc.dx = 0;
								hostNode.saveArc.y = 0;
								hostNode.saveArc.dy = 0;
								hostNode.saveLabel = new Object;
								hostNode.saveLabel.x = 0;
								hostNode.saveLabel.dx = 0;
								hostNode.saveLabel.y = 0;
								hostNode.saveLabel.dy = 0;

								// Add the node to the parent node's children
								node.children.push(hostNode);

								// Get the index
								childIndex = node.children.length - 1;
							}
							// Recurse to all children of this host
							if ($scope.hostList[e].objectJSON.child_hosts.length > 0) {
								var childHosts = $scope.hostList[e].objectJSON.child_hosts;
								updateHostTree(node.children[childIndex],
										childHosts, hostNode);
							}
						}
					});
				};

				// Create an ID for an img based on a file name
				var imgID = function(image) {
					return "cache-" + image.replace(/\./, "_");
				};

				// Update the image icon cache
				var updateImageIconCache = function() {
					var cache = d3.select("div#image-cache")
					for (var host in $scope.hostList) {
						var image =
								$scope.hostList[host].objectJSON.icon_image;
						if (image != "") {
							if (!$scope.iconList.hasOwnProperty(imgID(image))) {
								$scope.iconList[imgID(image)] = new Object;
								$scope.iconsLoading++;
								cache.append("img")
									.attr({
										id: function() {
											return imgID(image);
										},
										src: $scope.iconurl + image
									})
									.on("load", function() {
										$scope.iconsLoading--;
										var img = d3.select(d3.event.target);
										var image = img.attr("id");
										$scope.iconList[image].width =
												img.node().naturalWidth;
										$scope.iconList[image].height =
												img.node().naturalHeight;
									})
									.on("error", function() {
										$scope.iconsLoading--;
									});
							}
							$scope.hostList[host].iconInfo =
									$scope.iconList[imgID(image)];
						}
					}
				};

				// Build the host list and tree from the hosts returned
				// from the JSON CGIs
				var processHostList = function(json) {

					// First prune any host from the host list that
					// is no longer in the hosts returned from the CGIs
					for (var host in $scope.hostList) {
						if(host != nagiosProcessName &&
								!json.data.hostlist.hasOwnProperty(host)) {
							// Mark the entry as null (deletion is slow)
							$scope.hostList[host] = null;
						}
					}

					// Next prune any deleted hosts from the host tree
					pruneHostTree($scope.hostTree);

					// Now update the host list with the data
					// returned from the CGIs
					for (var host in json.data.hostlist) {
						// If we don't know about the host yet, add it to
						// the host list
						if (!$scope.hostList.hasOwnProperty(host) ||
								$scope.hostList[host] == null) {
							$scope.hostList[host] = new Object;
							$scope.hostList[host].name = host;
							$scope.hostList[host].serviceCount = 0;
						}
						// If a hosts' parent is not in the hostlist (user
						// doesn't have permission to view parent) re-parent the
						// host directly under the nagios process
						for (var parent in json.data.hostlist[host].parent_hosts) {
							var prnt = json.data.hostlist[host].parent_hosts[parent];
							if (!json.data.hostlist[prnt]) {
								var p = json.data.hostlist[host].parent_hosts;
								json.data.hostlist[host].parent_hosts.splice(0, 1);
							}
						}
						// Update the information returned
						$scope.hostList[host].objectJSON =
								json.data.hostlist[host];
					}

					// Now update the host tree
					var rootHosts = new Array;
					for (var host in $scope.hostList) {
						if ($scope.hostList[host] != null &&
								$scope.hostList[host].objectJSON.parent_hosts.length == 0) {
							rootHosts.push(host);
						}
					}
					updateHostTree($scope.hostTree, rootHosts);

					// Update the icon image cache
					if ($scope.showIcons) {
						updateImageIconCache();
					}

					// Finish the host list processing
					finishProcessingHostList();
				};

				var finishProcessingHostList = function() {

					if ($scope.showIcons && $scope.iconsLoading > 0) {
						setTimeout(function() {
								finishProcessingHostList()
								}, 10);
						return;
					}

					// If this is the first time the map has
					// been displayed...
					if($scope.fetchingHostlist) {
						// Stop the spinner
						$scope.spinner.stop();
						$scope.fetchingHostlist = false;

						// Display the map
						displayMap();
					}

					// Reparent the tree to specified root host
					if ($scope.hostList.hasOwnProperty($scope.root) &&
						($scope.rootNode != $scope.hostTree) &&
						$scope.hostList[$scope.root].hasOwnProperty("hostNodes")) {
						reparentTree($scope.hostList[$scope.root].hostNodes[0]);
					}

					// Finally update the map
					updateMap($scope.hostTree);
				};

				// Get list of all hosts
				var getHostList = function() {

					var parameters = {
						query: "hostlist",
						formatoptions: "enumerate bitmask",
						details: true
					};

					var getConfig = {
						params: parameters,
						withCredentials: true
					};

					// Send the JSON query
					$http.get($scope.cgiurl + "objectjson.cgi?", getConfig)
						.error(function(err) {
							// Stop the spinner
							$scope.spinner.stop();
							$scope.fetchingHostlist = false;
		
							console.warn(err);
						})
						.success(function(json) {
							// Record the last time Nagios core was started
							$scope.lastNagiosStart =
									json.result.program_start;

							// Record the time of the last update
							$scope.lastUpdate = json.result.query_time;

							// Process the host list received
							processHostList(json);

							// Get the services for each host
							getServiceList();

							// Get the status of each node
							getAllStatus(0);
						})
				};

				// Get the node's stroke color
				var getNodeStroke = function(hostStatus, collapsed) {

					var stroke;

					if(collapsed) {
						stroke = "blue";
					}
					else {
						switch(hostStatus) {
						case "up":
						case "down":
						case "unreachable":
							stroke = getNodeFill(hostStatus, false);
							break;
						default:
							stroke = "#cccccc";
							break;
						}
					}

					return stroke;
				};

				// Get the node's fill color
				var getNodeFill = function(hostStatus, dark) {

					var fill;

					switch(hostStatus) {
					case "up":
						fill = dark ? "rgb(0, 105, 0)" : "rgb(0, 210, 0)";
						break;
					case "down":
						fill = dark ? "rgb(128, 0, 0)" : "rgb(255, 0, 0)";
						break;
					case "unreachable":
						fill = dark ? "rgb(64, 0, 0)" : "rgb(128, 0, 0)";
						break;
					default:
						switch($scope.layout) {
						case layouts.UserSupplied.index:
						case layouts.DepthLayers.index:
						case layouts.DepthLayersVertical.index:
						case layouts.CollapsedTree.index:
						case layouts.CollapsedTreeVertical.index:
						case layouts.BalancedTree.index:
						case layouts.BalancedTreeVertical.index:
						case layouts.CircularBalloon.index:
						case layouts.Force.index:
							fill = "#ffffff";
							break;
						case layouts.CircularMarkup.index:
							fill = "#cccccc";
							break;
						}
						break;
					}

					return fill;
				};

				// Get the host status for the current node
				var getHostStatus = function(d) {

					var hostStatus = "pending";

					if(d.hasOwnProperty("hostInfo") &&
							d.hostInfo.hasOwnProperty("statusJSON")) {
						hostStatus = d.hostInfo.statusJSON.status;
					}

					return hostStatus;
				};

				// Get the service count for the current node
				var getServiceCount = function(d) {

					var serviceCount = 0;

					if(d.hasOwnProperty("hostInfo") &&
							d.hostInfo.hasOwnProperty("serviceCount")) {
						serviceCount = d.hostInfo.serviceCount;
					}

					return serviceCount;
				};

				// Return the glow filter for an icon
				var getGlowFilter = function(d) {
					if (d.hostInfo.hasOwnProperty("statusJSON")) {
						switch (d.hostInfo.statusJSON.status) {
						case "up":
							return "url(#icon-glow-up)";
							break;
						case "down":
							return "url(#icon-glow-down)";
							break;
						case "unreachable":
							return "url(#icon-glow-unreachable)";
							break;
						default:
							return null;
							break;
						}
					}
					else {
						return null;
					}
				};

				// Get the text filter
				var getTextFilter = function(d) {
					if ($scope.showIcons &&
							d.hostInfo.hasOwnProperty("iconInfo") &&
							d._children) {
						return "url(#circular-markup-text-collapsed)";
					}
					return null;
				};

				// Get the text stroke color
				var getTextStrokeColor = function(d) {
					if ($scope.showIcons &&
							d.hostInfo.hasOwnProperty("iconInfo") &&
							d._children) {
						return "white";
					}
					return null;
				};

				// Update the node's status
				var updateNode = function(domNode) {

					var duration = 750;

					var selection = d3.select(domNode);
					var data = selection.datum();
					var hostStatus = getHostStatus(data);
					var serviceCount = getServiceCount(data);

					switch($scope.layout) {
					case layouts.UserSupplied.index:
					case layouts.DepthLayers.index:
					case layouts.DepthLayersVertical.index:
					case layouts.CollapsedTree.index:
					case layouts.CollapsedTreeVertical.index:
					case layouts.BalancedTree.index:
					case layouts.BalancedTreeVertical.index:
					case layouts.CircularBalloon.index:
					case layouts.Force.index:
						selection.select("circle")
							.transition()
							.duration(duration)
							.attr({
								r: function() {
									return $scope.nodeScale(serviceCount) +
											$scope.swellRadius;
								}
							})
							.style({
								stroke: function() {
									return getNodeStroke(hostStatus,
											selection.datum().collapsed);
								},
								fill: function() {
									return getNodeFill(hostStatus, false);
								}
							})
							.transition()
							.duration(duration)
							.attr({
								r: $scope.nodeScale(serviceCount)
							});

						selection.select("image")
							.style({
								filter: function(d) {
									return getGlowFilter(d);
								}
							});

						selection.select("text")
							.each(function() {
								setTextAttrs(data, this);
							})
							.style({
								filter: function(d) {
									return getTextFilter(d);
								},
								stroke: function(d) {
									return getTextStrokeColor(d);
								}
							});
						break;
					case layouts.CircularMarkup.index:
						selection
							.transition()
							.duration(duration)
							.style({
								fill: function() {
									return getNodeFill(hostStatus, true);
								},
								"fill-opacity": 1,
								"stroke-opacity": 1
							})
							.attrTween("d", arcTween)
							.transition()
							.duration(duration)
							.style({
								fill: function() {
									return getNodeFill(hostStatus, false);
								}
							});
						break;
					}
				};

				// What to do when getAllStatus succeeds
				var onGetAllStatusSuccess = function(json, since) {

					// Record the time of the last update
					$scope.lastUpdate = json.result.query_time;

					// Check whether Nagios has restarted. If so
					// re-read the host list
					if (json.result.program_start >
							$scope.lastNagiosStart) {
						getHostList();
					}
					else {
						// Iterate over all hosts and update their status
						for (var host in json.data.hostlist) {
							if(!$scope.hostList[host].hasOwnProperty("statusJSON") ||
									($scope.hostList[host].statusJSON.last_check <
									json.data.hostlist[host].last_check)) {
								$scope.hostList[host].statusJSON =
										json.data.hostlist[host];
								if($scope.hostList[host].hasOwnProperty("g")) {
									$scope.hostList[host].g.forEach(function(e, i, a) {
										updateNode(e);
									});
								}
							}
						}

						// Send the request for service status
						getServiceStatus(since);

						// Schedule an update
						statusTimeout = setTimeout(function() {
							var newSince = (json.result.last_data_update / 1000) -
									$scope.updateStatusInterval;
							getAllStatus(newSince) },
							$scope.updateStatusInterval);
					}
				};

				// Get status of all hosts and their services
				var getAllStatus = function(since) {

					if ($scope.showIcons && $scope.iconsLoading > 0) {
						setTimeout(function() {
								getAllStatus()
								}, 10);
						return;
					}

					var parameters = {
						query: "hostlist",
						formatoptions: "enumerate bitmask",
						details: true,
						hosttimefield: "lastcheck",
						starttime: since,
						endtime: "-0"
					};

					var getConfig = {
						params: parameters,
						withCredentials: true
					};

					// Send the request for host status
					statusTimeout = null;
					$http.get($scope.cgiurl + "statusjson.cgi", getConfig)
						.error(function(err) {
							console.warn(err);
		
							// Schedule an update
							statusTimeout = setTimeout(function() { getAllStatus(since) },
									$scope.updateStatusInterval);
						})
						.success(function(json) {
							onGetAllStatusSuccess(json, since);
						})
				};

				// What to do when the getting the service status is successful
				var onGetServiceStatusSuccess = function(json) {
					var serviceCountUpdated = false;

					// Record the time of the last update
					$scope.lastUpdate = json.result.query_time;

					for (var host in json.data.servicelist) {
						var serviceStatUpdated = false;
						if (!$scope.hostList[host].hasOwnProperty("serviceStatusJSON")) {
							$scope.hostList[host].serviceCount =
									Object.keys(json.data.servicelist[host]).length;
							serviceCountUpdated = true;
							$scope.hostList[host].serviceStatusJSON = new Object;
							// Since this is the first time we have a
							// service count if we have the host status,
							// update the node(s).
							if ($scope.hostList[host].hasOwnProperty("statusJSON")) {
								switch ($scope.layout) {
								case layouts.UserSupplied.index:
								case layouts.DepthLayers.index:
								case layouts.DepthLayersVertical.index:
								case layouts.CollapsedTree.index:
								case layouts.CollapsedTreeVertical.index:
								case layouts.BalancedTree.index:
								case layouts.BalancedTreeVertical.index:
								case layouts.CircularBalloon.index:
								case layouts.Force.index:
									if($scope.hostList[host].hasOwnProperty("g")) {
										$scope.hostList[host].g.forEach(function(e, i, a) {
											updateNode(e);
										});
									}
								break;
								}
							}
						}
						else if (Object.keys($scope.hostList[host].serviceStatusJSON).length
								< Object.keys(json.data.servicelist[host]).length) {
							$scope.hostList[host].serviceCount =
									Object.keys(json.data.servicelist[host]).length;
							serviceCountUpdated = true;
						}
						for (service in json.data.servicelist[host]) {
							if ($scope.hostList[host].serviceStatusJSON[service] != json.data.servicelist[host][service])
								serviceStatUpdated = true;
							$scope.hostList[host].serviceStatusJSON[service] =
								json.data.servicelist[host][service];
						}
						if ($scope.hostList[host].hasOwnProperty("g") && serviceStatUpdated) {
							$scope.hostList[host].g.forEach(function(e, i, a) {
								updateNode(e);
							});
						}
					}
					if (serviceCountUpdated) {
						switch ($scope.layout) {
						case layouts.CircularMarkup.index:
							updateMap($scope.hostTree);
							break;
						}
					}
				};

				// Get status of all hosts' services
				var getServiceStatus = function(since) {

					var parameters = {
						query: "servicelist",
						formatoptions: "enumerate bitmask",
						servicetimefield: "lastcheck",
						starttime: since,
						endtime: "-0"
					};

					var getConfig = {
						params: parameters,
						withCredentials: true
					};

					// Send the request for service status
					$http.get($scope.cgiurl + "statusjson.cgi", getConfig)
						.error(function(err) {
							console.warn(err);
						})
						.success(function(json) {
							onGetServiceStatusSuccess(json);
						});
				};

				// Get an object attribute in a generic way that checks for
				// the existence of all attributes in the hierarchy
				var getObjAttr = function(d, attrs, nilval) {

					if(d.hasOwnProperty("hostInfo")) {
						var obj = d.hostInfo;
						for(var i = 0; i < attrs.length; i++) {
							if(!obj.hasOwnProperty(attrs[i])) {
								return nilval;
							}
							obj = obj[attrs[i]];
						}
						return obj;
					}
					return nilval;
				};

				// Determine how long an object has been in it's 
				// current state
				var getStateDuration = function(d) {
					var now = new Date;
					var duration;
					var last_state_change = getObjAttr(d,
							["statusJSON", "last_state_change"], null);
					var program_start = getObjAttr(d,
							["statusJSON", "result", "program_start"],
							null);
					if(last_state_change == null) {
						return "unknown";
					}
					else if(last_state_change == 0) {
						duration = now.getTime() - program_start;
					}
					else {
						duration = now.getTime() - last_state_change;
					}
					return duration;
				};

				// Get the display value for a state time
				var getStateTime = function(time) {
					var when = new Date(time);
					if(when.getTime() == 0) {
						return "unknown";
					}
					else {
						return when;
					}
				};

				// Get the list of parent hosts
				var getParentHosts = function(d) {
					var parents = getObjAttr(d,
							["objectJSON", "parent_hosts"], null);
					if(parents == null) {
						return "unknown";
					}
					else if(parents.length == 0) {
						return "None (This is a root host)";
					}
					else {
						return parents.join(", ");
					}
				};

				// Get the number of child hosts
				var getChildHosts = function(d) {
					var children = getObjAttr(d,
							["objectJSON", "child_hosts"], null);
					if(children == null) {
						return "unknown";
					}
					else {
						return children.length;
					}
				};

				// Get a summary of the host's service states
				var getServiceSummary = function(d) {
					var states = ["ok", "warning", "unknown", "critical",
							"pending"];
					var stateCounts = {};
					if(d.hostInfo.hasOwnProperty("serviceStatusJSON")) {
						for (var service in d.hostInfo.serviceStatusJSON) {
							var state = d.hostInfo.serviceStatusJSON[service];
							if(!stateCounts.hasOwnProperty(state)) {
								stateCounts[state] = 0;
							}
							stateCounts[state]++;
						}
					}
					return stateCounts;
				};

				// Set the popup contents
				var setPopupContents = function(popup, d) {

					$scope.popupContents.hostname = getObjAttr(d,
							["objectJSON", "name"], "unknown");
					if($scope.popupContents.hostname == nagiosProcessName) {
						var now = new Date;
						$scope.popupContents.alias = nagiosProcessName;
						$scope.popupContents.address = window.location.host;
						$scope.popupContents.state = "up";
						$scope.popupContents.duration = now.getTime() - $scope.lastNagiosStart;
						$scope.popupContents.lastcheck = $scope.lastUpdate;
						$scope.popupContents.lastchange = $scope.lastNagiosStart;
						$scope.popupContents.parents = "";
						$scope.popupContents.children = "";
						$scope.popupContents.services = null;
					} else {
						$scope.popupContents.alias = getObjAttr(d,
								["objectJSON", "alias"], "unknown");
						$scope.popupContents.address = getObjAttr(d,
								["objectJSON", "address"], "unknown");
						$scope.popupContents.state = getObjAttr(d,
								["statusJSON", "status"], "pending");
						$scope.popupContents.duration = getStateDuration(d);
						$scope.popupContents.lastcheck =
								getStateTime(getObjAttr(d,
								["statusJSON", "last_check"], 0));
						$scope.popupContents.lastchange =
								getStateTime(getObjAttr(d,
								["statusJSON", "last_state_change"], 0));
						$scope.popupContents.parents = getParentHosts(d);
						$scope.popupContents.children = getChildHosts(d);
						$scope.popupContents.services = getServiceSummary(d);
					}
				};

				// Update the map
				var updateMap = function(source, reparent) {

					// Update config variables before updating the map
					$scope.showText = $scope.notext == "false";
					$scope.showLinks = $scope.nolinks == "false";
					$scope.showPopups = $scope.nopopups == "false";
					$scope.allowResize = $scope.noresize == "false";
					$scope.showIcons = $scope.noicons == "false";

					reparent = reparent || false;
					switch($scope.layout) {
					case layouts.UserSupplied.index:
						updateUserSuppliedMap(source);
						break;
					case layouts.DepthLayers.index:
					case layouts.DepthLayersVertical.index:
					case layouts.CollapsedTree.index:
					case layouts.CollapsedTreeVertical.index:
					case layouts.BalancedTree.index:
					case layouts.BalancedTreeVertical.index:
					case layouts.CircularBalloon.index:
						$scope.updateDuration = 500;
						updateTreeMap(source);
						break;
					case layouts.CircularMarkup.index:
						$scope.updateDuration = 750;
						updatePartitionMap(source, reparent);
						break;
					case layouts.Force.index:
						updateForceMap(source);
						break;
					}
				};

				// Update all descendants of a node when collapsing the node
				var updateDescendantsOnCollapse = function(root, x, y,
						member) {

					// Default member to _children
					member = member || "_children";

					if(root.hasOwnProperty(member) && root[member] !=
							null) {
						root[member].forEach(function(e, i, a) {
							e.x = x;
							e.dx = 0;
							updateDescendantsOnCollapse(e, x, y,
									"children");
						});
					}
				};

				// Update all descendants of a node when expanding the node
				var updateDescendantsOnExpand = function(root, x, y,
						member) {

					// Default member to _children
					member = member || "_children";

					if(root.hasOwnProperty(member) && root[member] !=
							null) {
						root[member].forEach(function(e, i, a) {
							e.saveArc.x = x;
							e.saveArc.dx = 0;
							e.saveArc.y = y;
							e.saveArc.dy = 0;
							e.saveLabel.x = x;
							e.saveLabel.dx = 0;
							e.saveLabel.y = y;
							e.saveLabel.dy = 0;
							updateDescendantsOnExpand(e, x, y, "children");
						});
					}
				};

				// Update the layout information for nodes which are/were
				// children of collapsed nodes
				var updateToggledNodes = function(root, updater) {

					if(root.collapsed) {
						if(root.depth == 0) {
							updater(root, 0, 0);
						}
						else {
							updater(root, root.x + root.dx / 2,
									root.y + root.dy / 2);
						}
					}
					else if(root.hasOwnProperty("children")) {
						root.children.forEach(function(e, i, a) {
							updateToggledNodes(e, updater);
						});
					}
				};

				// The on-click function for partition maps
				var onClickPartition = function(d) {

					var evt = d3.event;

					// If something else (like a pan) is occurring,
					// ignore the click
					if(d3.event.defaultPrevented) return;

					// Hide the popup
					$scope.displayPopup = false;
					$scope.$apply("displayPopup");

					if(evt.shiftKey) {

						// Record the new root
						$scope.root = d.hostInfo.name;
						$scope.$apply('root');

						// A shift-click indicates a reparenting
						// of the tree
						if(d.collapsed) {
							// If the node click is collapsed,
							// expand it so the tree will have some
							// depth after reparenting
							toggleNode(d);
						}
						// Collapse the root node for good animation
						toggleNode($scope.hostTree);
						updateMap($scope.hostTree, true);
						// Waiting until the updating is done...
						setTimeout(function() {
							// Re-expand the root node so the
							// reparenting will occur correctly
							toggleNode($scope.hostTree);
							// Reparent the tree and redisplay the map
							reparentTree(d);
							updateMap($scope.hostTree, true);
						}, $scope.updateDuration + 50);
					}
					else {
						// A click indicates collapsing or
						// expanding the node
						toggleNode(d);
						updateMap($scope.hostTree);
					}
				};

				// Recalculate the values of the partition
				var recalculatePartitionValues = function(node) {

					if(node.hasOwnProperty("children") &&
							node.children != null) {
						node.children.forEach(function(e) {
							recalculatePartitionValues(e);
						});
						node.value = node.children.reduce(function(a, b) {
							return a + b.value;
						}, 0);
					}
					else {
						node.value = getPartitionNodeValue(node);
					}
				};

				// Recalculate the layout of the partition
				var recalculatePartitionLayout = function(node, index) {

					index = index || 0;

					if(node.depth > 0) {
						if(index == 0) {
							node.x = node.parent.x;
						}
						else {
							node.x = node.parent.children[index - 1].x +
									node.parent.children[index - 1].dx;
						}
						node.dx = (node.value / node.parent.value) *
								node.parent.dx
					}
					if(node.hasOwnProperty("children") &&
							node.children != null) {
						node.children.forEach(function(e, i) {
							recalculatePartitionLayout(e, i);
						});
					}
				};

				// Text filter for labels
				var textFilter = function(d) {
					return d.collapsed ?
						"url(#circular-markup-text-collapsed)" :
						"url(#circular-markup-text)";
				}

				var addPartitionMapTextGroupContents = function(d, node) {
					var selection = d3.select(node);

					// Append the label
					if($scope.showText) {
						selection.append("text")
							.each(function(d) {
								setTextAttrs(d, this);
							})
							.style({
								"fill-opacity": 1e-6,
								fill: "white",
								filter: function(d) {
									return textFilter(d);
								}
							})
							.text(function(d) {
								return d.hostInfo.objectJSON.name;
							});
					}

					// Display the node icon if it has one
					if($scope.showIcons) {
						var image = d.hostInfo.objectJSON.icon_image;
						if (image != "" && image != undefined) {
							var iconInfo = d.hostInfo.iconInfo;
							selection.append("image")
								.attr({
									"xlink:href": $scope.iconurl + image,
									width: iconInfo.width,
									height: iconInfo.height,
									x: -(iconInfo.width / 2),
									y: -((iconInfo.height +
											layouts.CircularMarkup.textPadding +
											$scope.fontSize) / 2),
									transform: function() {
										var rotateAngle = (d.x + d.dx / 2) *
												180 / Math.PI +
												layouts.CircularBalloon.rotation;
										return "rotate(" + -rotateAngle + ")";
									}
								})
								.style({
									filter: function() {
										return getGlowFilter(d);
									}
								});
						}
					}
				};

				// Update the map for partition displays
				var updatePartitionMap = function(source, reparent) {

					// The svg element that holds it all
					var mapsvg = d3.select("svg#map g#container");

					// The data for the map
					var mapdata = $scope.partition.nodes(source);

					if(reparent) {
						if($scope.hostTree.collapsed) {
							// If this is a reparent operation and
							// we're in the collapse phase, shrink
							// the root node to nothing
							$scope.hostTree.x = 0;
							$scope.hostTree.dx = 0;
							$scope.hostTree.y = 0;
							$scope.hostTree.dy = 0;
						}
						else {
							// Calculate the total value of the 1st level
							// children to determine whether we have
							// the bug below
							var value = $scope.hostTree.children.reduce(function(a, b) {
								return a + b.value;
							}, 0);
							if(value == 2 * $scope.hostTree.value) {
								// This appears to be a bug in the
								// d3 library where the sum of the
								// values of the children of the root
								// node is twice what it should be.
								// Work around the bug by manually
								// adjusting the values.
								recalculatePartitionValues($scope.hostTree);
								recalculatePartitionLayout($scope.hostTree);
							}
						}
					}

					// Update the data for the paths
					var path = mapsvg
						.select("g#paths")
						.selectAll("path")
						.data(mapdata, function(d) {
							return d.id || (d.id = ++$scope.nodeID);
						});

					// Update the data for the labels
					var labelGroup = mapsvg
						.selectAll("g.label")
						.data(mapdata, function(d) {
							return d.id || (d.id = ++$scope.nodeID);
						});

					// Traverse the data, artificially setting the layout
					//for collapsed children
					updateToggledNodes($scope.hostTree,
							updateDescendantsOnCollapse);

					var pathEnter = path.enter()
						.append("path")
							.attr({
								d: function(d) {
									return $scope.arc({x: 0, dx: 0, y: d.y,
											dy: d.dy});
								}
							})
							.style({
								"fill-opacity": 1e-6,
								"stroke-opacity": 1e-6,
								stroke: "#fff",
								fill: function(d) {
									var hostStatus = "pending";
									if(d.hasOwnProperty("hostInfo") &&
											d.hostInfo.hasOwnProperty("statusJSON")) {
										hostStatus = d.hostInfo.statusJSON.status;
									}
									return getNodeFill(hostStatus, false);
								}
							})
							.on("click", function(d) {
								onClickPartition(d);
							})
							.each(function(d) {
								// Traverse each node, saving a pointer
								// to the node in the hostList to
								// facilitate updating later
								if(d.hasOwnProperty("hostInfo")) {
									if(!d.hostInfo.hasOwnProperty("g")) {
										d.hostInfo.g = new Array;
									}
									d.hostInfo.g.push(this);
								}
							});

					if ($scope.showPopups) {
						pathEnter
							.on("mouseover", function(d, i) {
								if($scope.showPopups &&
										d.hasOwnProperty("hostInfo")) {
									displayPopup(d);
								}
							})
							.on("mouseout", function(d, i) {
								$scope.displayPopup = false;
								$scope.$apply("displayPopup");
							});
					}

					labelGroup.enter()
						.append("g")
						.attr({
							class: "label",
							transform: function(d) {
								return "translate(" +
										$scope.arc.centroid({x: 0,
										dx: 1e-6, y: d.y, dy: d.dy}) +
										")";
							}
						})
						.each(function(d) {
							addPartitionMapTextGroupContents(d, this);
						});

					// Update paths on changes
					path.transition()
						.duration($scope.updateDuration)
						.style({
							"fill-opacity": 1,
							"stroke-opacity": 1,
							fill: function(d) {
								var hostStatus = "pending";
								if(d.hasOwnProperty("hostInfo") &&
										d.hostInfo.hasOwnProperty("statusJSON")) {
									hostStatus =
											d.hostInfo.statusJSON.status;
								}
								return getNodeFill(hostStatus, false);
							}
						})
						.attrTween("d", arcTween);

					// Update label groups on change
					labelGroup
						.transition()
						.duration($scope.updateDuration)
						.attrTween("transform", labelGroupTween);

					if($scope.showText) {
						labelGroup
							.selectAll("text")
							.style({
								"fill-opacity": 1,
								filter: function(d) {
									return textFilter(d);
								}
							})
							.each(function(d) {
								setTextAttrs(d, this);
							});
					}

					if($scope.showIcons) {
						labelGroup
							.selectAll("image")
							.attr({
								transform: function(d) {
									var rotateAngle = (d.x + d.dx / 2) * 180 / Math.PI +
											layouts.CircularBalloon.rotation;
									return "rotate(" + -rotateAngle + ")";
								}
							});
					}

					// Remove paths when necessary
					path.exit()
						.transition()
						.duration($scope.updateDuration)
						.style({
							"fill-opacity": 1e-6,
							"stroke-opacity": 1e-6
						})
						.attrTween("d", arcTween)
						.remove();

					// Remove labels when necessary
					if($scope.showText) {
						var labelGroupExit = labelGroup.exit();

						labelGroupExit.each(function(d) {
							var group = d3.select(this);

							group.select("text")
								.transition()
								.duration($scope.updateDuration / 2)
								.style({
									"fill-opacity": 1e-6
								});

							group.select("image")
								.transition()
								.duration($scope.updateDuration)
								.style({
									"fill-opacity": 1e-6
								});

							})
							.transition()
							.duration($scope.updateDuration)
							.attrTween("transform", labelGroupTween)
							.remove();
					}
				};

				// Traverse the tree, building a list of nodes at each depth
				var updateDepthList = function(node) {

					if($scope.depthList[node.depth] == null) {
						$scope.depthList[node.depth] = new Array;
					}
					$scope.depthList[node.depth].push(node);

					if(node.hasOwnProperty("children") &&
							node.children != null) {
						node.children.forEach(function(e) {
							updateDepthList(e);
						});
					}
				};

				// Calculate the layout for the collapsed tree
				var calculateCollapsedTreeLayout = function(root) {

					// First get the list of nodes at each depth
					$scope.depthList = new Array;
					updateDepthList(root);

					// Then determine the widest layer
					var maxWidth = $scope.depthList.reduce(function(a, b) {
						return a > b.length ? a : b.length;
					}, 0);

					// Determine the spacing of nodes based on the max width
					var treeSize = getTreeSize();
					var spacing = treeSize[0] / maxWidth;

					// Re-calculate the layout based on the above
					$scope.depthList.forEach(function(layer, depth) {
						layer.forEach(function(node, index) {
							// Calculate the location index: the
							// "index distance" from the center node
							var locationIndex =
									(index - (layer.length - 1) / 2);
							node.x = (treeSize[0] / 2) +
									(locationIndex * spacing);
						});
					});
				};

				// The on-click function for trees
				var onClickTree = function(d) {

					var evt = d3.event;
					var updateNode = d;

					// If something else (like a pan) is occurring,
					// ignore the click
					if(d3.event.defaultPrevented) return;

					// Hide the popup
					$scope.displayPopup = false;
					$scope.$apply("displayPopup");

					if(evt.shiftKey) {
						// Record the new root
						$scope.root = d.hostInfo.name;
						$scope.$apply('root');

						switch($scope.layout) {
						case layouts.DepthLayers.index:
						case layouts.DepthLayersVertical.index:
							// Expand the children of the focal point
							$scope.focalPoint.children.forEach(function(e) {
								if(e.collapsed) {
									toggleNode(e);
								}
							});
							// If the focal point is not the root node,
							// restore all children of it's parent
							if(!($scope.focalPoint === $scope.hostTree)) {
								$scope.focalPoint.parent.children =
										$scope.focalPoint.parent._children;
								delete $scope.focalPoint.parent._children;
								$scope.focalPoint.parent.collapsed = false;
							}
							break;
						default:
							if(d.collapsed) {
								// If the node click is collapsed,
								// expand it so the tree will have
								// some depth after reparenting
								toggleNode(d);
							}
							break;
						}
						reparentTree(d);
					}
					else {
						switch($scope.layout) {
						case layouts.DepthLayers.index:
						case layouts.DepthLayersVertical.index:
							if((d === $scope.focalPoint) ||
									!(d.hasOwnProperty("children") ||
									d.hasOwnProperty("_children"))) {
								// Nothing to see here, move on
								return;
							}
							// Restore all the children of the current focal
							// point and it's parent (if it is not the root
							// of the tree)
							$scope.focalPoint.children.forEach(function(e) {
								if(e.collapsed) {
									toggleNode(e);
								}
							});
							if(!($scope.focalPoint === $scope.hostTree)) {
								$scope.focalPoint.parent.children =
									$scope.focalPoint.parent._children;
								$scope.focalPoint.parent._children = null;
								$scope.focalPoint.parent.collapsed = false;
							}
							// Set the new focal point
							$scope.focalPoint = d;
							updateNode = (d === $scope.hostTree) ? d :
									d.parent;
							break;
						default:
							toggleNode(d);
							break;
						}
					}
					updateMap(updateNode);
				};

				// Add a node group to the tree map
				var addTreeMapNodeGroupContents = function(d, node) {
					var selection = d3.select(node);

					// Display the circle if the node has no icon or
					// icons are suppressed
					if(!$scope.showIcons ||
							d.hostInfo.objectJSON.icon_image == "") {
						selection.append("circle")
							.attr({
								r: 1e-6
							});
					}

					// Display the node icon if it has one
					if($scope.showIcons) {
						var image = d.hostInfo.objectJSON.icon_image;
						if (image != "" && image != undefined) {
							var iconInfo = d.hostInfo.iconInfo;
							var rotateAngle = null;
							if ($scope.layout == layouts.CircularBalloon.index) {
								rotateAngle = d.x +
										layouts.CircularBalloon.rotation;
							}
							selection.append("image")
								.attr({
									"xlink:href": $scope.iconurl + image,
									width: iconInfo.width,
									height: iconInfo.height,
									x: -(iconInfo.width / 2),
									y: -(iconInfo.height / 2),
									transform: function() {
										return "rotate(" + -rotateAngle + ")";
									}
								})
								.style({
									filter: function() {
										return getGlowFilter(d);
									}
								});
						}
					}

					// Label the nodes with their host names
					if($scope.showText) {
						selection.append("text")
							.each(function(d) {
								setTextAttrs(d, this);
							})
							.style({
								"fill-opacity": 1e-6
							})
							.text(function(d) {
								return d.hostInfo.objectJSON.name;
							});
					}

					// Register event handlers for showing the popups
					if($scope.showPopups) {
						selection
							.on("mouseover", function(d, i) {
								if(d.hasOwnProperty("hostInfo")) {
									displayPopup(d);
								}
							})
							.on("mouseout", function(d, i) {
								$scope.displayPopup = false;
								$scope.$apply("displayPopup");
							});
					}

				};

				// Update the tree map
				var updateTreeMap = function(source) {

					var textAttrs;

					// The svg element that holds it all
					var mapsvg = d3.select("svg#map g#container");

					// Build the nodes from the data
					var nodes;
					switch($scope.layout) {
					case layouts.DepthLayers.index:
					case layouts.DepthLayersVertical.index:
						// If this is a depth layer layout, first update the
						// tree based on the current focused node,
						updateDepthLayerTree();
						// then build the nodes from the data
						var root = ($scope.focalPoint === $scope.hostTree) ?
								$scope.hostTree : $scope.focalPoint.parent;
						nodes = $scope.tree.nodes(root).reverse();
						break;
					case layouts.CollapsedTree.index:
					case layouts.CollapsedTreeVertical.index:
						// If this is a collapsed tree layout,
						// first build the nodes from the data,
						nodes = $scope.tree.nodes($scope.hostTree).reverse();
						// then re-calculate the positions of the nodes
						calculateCollapsedTreeLayout($scope.hostTree);
						break;
					default:
						nodes = $scope.tree.nodes($scope.hostTree).reverse();
						break;
					}

					// ...and the links from the nodes
					var links = $scope.tree.links(nodes);

					// Create the groups to contain the nodes
					var node = mapsvg.selectAll(".node")
						.data(nodes, function(d) {
							return d.id || (d.id = ++$scope.nodeID);
						});

					if($scope.showLinks) {
						// Create the paths for the links
						var link = mapsvg
							.select("g#links")
							.selectAll(".link")
							.data(links, function(d) { return d.target.id; });

						// Enter any new links at the parent's
						// previous position.
						link.enter()
							.append("path")
							.attr({
								class: "link",
								d: function(d) {
									var o = {
										x: (source.hasOwnProperty("xOld") ?
												source.xOld : source.x) *
												$scope.zoom.scale(),
										y: (source.hasOwnProperty("yOld") ?
												source.yOld : source.y) *
												$scope.zoom.scale()
									};
									return $scope.diagonal({source: o,
											target: o});
								}
							})
							.transition()
							.duration($scope.updateDuration)
							.attr({
								d: $scope.diagonal
							});

						// Transition links to their new position.
						link.transition()
							.duration($scope.updateDuration)
							.attr({
								d: $scope.diagonal
							});

						// Transition exiting nodes to the parent's
						// new position.
						link.exit().transition()
							.duration($scope.updateDuration)
							.attr({
								d: function(d) {
									var o = {
										x: source.x * $scope.zoom.scale(),
										y: source.y * $scope.zoom.scale()
									};
									return $scope.diagonal({source: o,
											target: o});
								}
							})
							.remove();
					}

					// Enter any new nodes at the parent's
					// previous position.
					var nodeEnter = node.enter()
						.append("g")
						.attr({
							class: "node",
							transform: function(d) {
								return getNodeTransform(source);
							}
						})
						.on("click", function(d) {
							onClickTree(d);
						})
						.each(function(d) {
							// Traverse each node, saving a pointer to
							// the node in the hostList to facilitate
							// updating later
							if(d.hasOwnProperty("hostInfo")) {
								if(!d.hostInfo.hasOwnProperty("g")) {
									d.hostInfo.g = new Array;
								}
								d.hostInfo.g.push(this);
							}
							addTreeMapNodeGroupContents(d, this);
						});

					// Move the nodes to their final destination
					var nodeUpdate = node.transition()
						.duration($scope.updateDuration)
						.attr({
							transform: function(d) {
								return getNodeTransform(d);
							}
						});

					// Update the node's circle size
					nodeUpdate.select("circle")
						.attr({
							r: function(d) {
								var serviceCount = 0;
								if(d.hasOwnProperty("hostInfo") &&
										d.hostInfo.hasOwnProperty("serviceCount")) {
									serviceCount = d.hostInfo.serviceCount;
								}
								return $scope.nodeScale(serviceCount);
							}
						})
						.style({
							stroke: function(d) {
								var hostStatus = "pending";
								if(d.hasOwnProperty("hostInfo") &&
										d.hostInfo.hasOwnProperty("statusJSON")) {
									hostStatus =
											d.hostInfo.statusJSON.status;
								}
								return getNodeStroke(hostStatus,
										d.collapsed);
							},
							fill: function(d) {
								var hostStatus = "pending";
								if(d.hasOwnProperty("hostInfo") &&
										d.hostInfo.hasOwnProperty("statusJSON")) {
									hostStatus = d.hostInfo.statusJSON.status;
								}
								return getNodeFill(hostStatus, false);
							},
						});

					// Update the images' filters
					nodeUpdate.select("image")
						.style({
							filter: function(d) {
								return getGlowFilter(d);
							}
						});

					// Update the text's opacity
					nodeUpdate.select("text")
						.each(function(d) {
							setTextAttrs(d, this);
						})
						.style({
							"fill-opacity": 1,
							filter: function(d) {
								return getTextFilter(d);
							},
							stroke: function(d) {
								return getTextStrokeColor(d);
							}
						});

					// Transition exiting nodes to the parent's
					// new position.
					var nodeExit = node.exit().transition()
						.duration($scope.updateDuration)
						.attr({
							transform: function(d) {
								return getNodeTransform(source);
							}
						})
						.remove();

					nodeExit.select("circle")
						.attr({
							r: 1e-6
						});

					nodeExit.select("text")
						.style({
							"fill-opacity": 1e-6
						});

					// Update all nodes associated with the source
					if(source.hasOwnProperty("hostInfo") &&
							source.hostInfo.hasOwnProperty("g")) {
						source.hostInfo.g.forEach(function(e, i, a) {
							updateNode(e);
						});
					}

					// Save the old positions for the next transition.
					nodes.forEach(function(e) {
						e.xOld = e.x;
						e.yOld = e.y;
					});
				};

				// Update the tree for the depth layer layout
				var updateDepthLayerTree = function() {

					// In a depth layer layout, the focal point node is the
					// center of the universe; show only it, it's children
					// and it's parent (if the focal point node is not the
					// root node).
					if(!($scope.focalPoint === $scope.hostTree)) {
						// For all cases except where the focal point is the
						// root node make the focal point the only child of
						// it's parent
						$scope.focalPoint.parent._children =
								$scope.focalPoint.parent.children;
						$scope.focalPoint.parent.children = new Array;
						$scope.focalPoint.parent.children.push($scope.focalPoint);
						$scope.focalPoint.parent.collapsed = true;
					}
					// Collapse all the children of the focal point
					if($scope.focalPoint.hasOwnProperty("children") &&
							$scope.focalPoint.children != null) {
						$scope.focalPoint.children.forEach(function(e) {
							if(!e.collapsed &&
									e.hasOwnProperty("children") &&
									(e.children.length > 0)) {
								toggleNode(e);
							}
						});
					}
				};

				var addUserSuppliedNodeGroupContents = function(d, node) {
					var selection = d3.select(node);

					// Display the circle if the node has no icon or
					// icons are suppressed
					if(!$scope.showIcons ||
							d.hostInfo.objectJSON.icon_image == "") {
						selection.append("circle")
							.attr({
								r: 1e-6
							});
					}

					// Display the node icon if it has one
					if($scope.showIcons) {
						var image = d.hostInfo.objectJSON.icon_image;
						if (image != "" && image != undefined) {
							var iconInfo = d.hostInfo.iconInfo
							selection.append("image")
								.attr({
									"xlink:href": $scope.iconurl + image,
									width: iconInfo.width,
									height: iconInfo.height,
									x: -(iconInfo.width / 2),
									y: -(iconInfo.height / 2),
								})
								.style({
									filter: function() {
										return getGlowFilter(d);
									}
								});
						}
					}

					// Label the nodes with their host names
					if($scope.showText) {
						selection.append("text")
							.each(function(d) {
								setTextAttrs(d, this);
							})
							.text(function(d) {
								return d.hostInfo.objectJSON.name;
							});
					}

					// Register event handlers for showing the popups
					if($scope.showPopups) {
						selection
							.on("mouseover", function(d, i) {
								if(d.hasOwnProperty("hostInfo")) {
									displayPopup(d);
								}
							})
							.on("mouseout", function(d, i) {
								$scope.displayPopup = false;
								$scope.$apply("displayPopup");
							});
					}
				};

				// Update the map that uses configuration-specified
				// coordinates
				var updateUserSuppliedMap = function(source) {

					// Update the scales
					calculateUserSuppliedDimensions();
					userSuppliedLayout.xScale
						.domain([userSuppliedLayout.dimensions.upperLeft.x,
								userSuppliedLayout.dimensions.lowerRight.x]);
					userSuppliedLayout.yScale
						.domain([userSuppliedLayout.dimensions.upperLeft.y,
								userSuppliedLayout.dimensions.lowerRight.y]);

					// The svg element that holds it all
					var mapsvg = d3.select("svg#map g#container");

					// Convert the host list into an array
					var mapdata = new Array;
					for(host in $scope.hostList) {
						if(host != null) {
							var tmp = new Object;
							tmp.hostInfo = $scope.hostList[host];
							mapdata.push(tmp);
						}
					}

					// Update the data for the nodes
					var node = mapsvg
						.selectAll("g.node")
						.data(mapdata);

					var nodeEnter = node.enter()
						.append("g")
						.attr({
							class: "node",
							transform: function(d) {
								return getNodeTransform(d);
							}
						})
						.each(function(d) {
							// Traverse each node, saving a pointer
							// to the node in the hostList to
							// facilitate updating later
							if(d.hasOwnProperty("hostInfo")) {
								if(!d.hostInfo.hasOwnProperty("g")) {
									d.hostInfo.g = new Array;
								}
								d.hostInfo.g.push(this);
							}
							addUserSuppliedNodeGroupContents(d, this);
						});
				};

				// Tick function for force layout
				var onForceTick = function(source) {

					if($scope.showLinks) {
						forceLayout.link
							.attr({
								x1: function(d) {
									return $scope.xZoomScale(d.source.x);
								},
								y1: function(d) {
									return $scope.yZoomScale(d.source.y);
								},
								x2: function(d) {
									return $scope.xZoomScale(d.target.x);
								},
								y2: function(d) {
									return $scope.yZoomScale(d.target.y);
								}
							});
					}

					forceLayout.node
						.attr({
							transform: function(d) {
								return "translate(" +
										$scope.xZoomScale(d.x) + ", " +
										$scope.yZoomScale(d.y) + ")";
							}
						});
				};

				// Flatten the map
				var flattenMap = function(root) {
					var nodes = [], i = 0;

					function recurse(node, depth) {
						if(node.children) node.children.forEach(function(e) {
							recurse(e, depth + 1);
						});
						if(!node.id) node.id = ++i;
						node.depth = depth;
						nodes.push(node);
					}

					recurse(root, 0);
					return nodes;
				};

				// Handle a click on a node in the force tree
				var onClickForce = function(d) {

					// Hide the popup
					$scope.displayPopup = false;
					$scope.$apply("displayPopup");

					// Note: reparenting the tree is not implemented
					// because the map doesn't appear any different
					// after reparenting. However, reparenting would
					// affect what is collapsed/expanded when an
					// interior node is click, so it eventually may
					// make sense.
					toggleNode(d);
					updateMap(d);
				};

				// Add the components to the force map node group
				var addForceMapNodeGroupContents = function(d, node) {
					var selection = d3.select(node);

					if(!$scope.showIcons ||
							d.hostInfo.objectJSON.icon_image == "") {
						selection.append("circle")
							.attr({
								r: $scope.minRadius
							});
					}

					// Display the node icon if it has one
					if ($scope.showIcons) {
						var image = d.hostInfo.objectJSON.icon_image;
						if (image != "" && image != undefined) {
							var iconInfo = d.hostInfo.iconInfo;
							var rotateAngle = null;
							if ($scope.layout == layouts.CircularBalloon.index) {
								rotateAngle = d.x +
										layouts.CircularBalloon.rotation;
							}
							selection.append("image")
								.attr({
									"xlink:href": $scope.iconurl + image,
									width: iconInfo.width,
									height: iconInfo.height,
									x: -(iconInfo.width / 2),
									y: -(iconInfo.height / 2),
									transform: function() {
										return "rotate(" + -rotateAngle + ")";
									}
								})
								.style({
									filter: function() {
										return getGlowFilter(d);
									}
								});
						}
					}

					if ($scope.showText) {
						selection.append("text")
							.each(function(d) {
								setTextAttrs(d, this);
							})
							.text(function(d) {
								return d.hostInfo.objectJSON.name;
							})
							.style({
								filter: function(d) {
									return getTextFilter(d);
								},
								stroke: function(d) {
									return getTextStrokeColor(d);
								}
							});
					}

					if ($scope.showPopups) {
						selection
							.on("click", function(d) {
								onClickForce(d);
							})
							.on("mouseover", function(d) {
								if($scope.showPopups) {
									if(d.hasOwnProperty("hostInfo")) {
										displayPopup(d);
									}
								}
							})
							.on("mouseout", function(d) {
								$scope.displayPopup = false;
								$scope.$apply("displayPopup");
							});
					}

				};

				// Update the force map
				var updateForceMap = function(source) {

					// How long must we wait
					var duration = 750;

					// The svg element that holds it all
					var mapsvg = d3.select("svg#map g#container");

					// Build the nodes from the data
					var nodes = flattenMap($scope.hostTree);

					// ...and the links from the nodes
					var links = d3.layout.tree().links(nodes);

					// Calculate the force parameters
					var maxDepth = nodes.reduce(function(a, b) {
						return a > b.depth ? a : b.depth;
					}, 0);
					var diameter = Math.min($scope.svgHeight -
							2 * layouts.Force.outsidePadding,
							$scope.svgWidth -
							2 * layouts.Force.outsidePadding);
					var distance = diameter / (maxDepth * 2);
					var charge = -30 * (Math.pow(distance, 1.2) / 20);

					// Restart the force layout.
					$scope.force
						.linkDistance(distance)
						.charge(charge)
						.nodes(nodes)
						.links(links)
						.start();

					if($scope.showLinks) {
						// Create the lines for the links
						forceLayout.link = mapsvg.select("g#links")
							.selectAll(".link")
							.data(links, function(d) { return d.target.id; });

						// Create new links
						forceLayout.link.enter()
							.append("line")
							.attr({
								class: "link",
								x1: function(d) {
									return $scope.xZoomScale(d.source.x);
								},
								y1: function(d) {
									return $scope.yZoomScale(d.source.y);
								},
								x2: function(d) {
									return $scope.xZoomScale(d.target.x);
								},
								y2: function(d) {
									return $scope.yZoomScale(d.target.y);
								}
							});

						// Remove any old links.
						forceLayout.link.exit().remove();
					}

					// Create the nodes from the data
					forceLayout.node = mapsvg.selectAll("g.node")
						.data(nodes, function(d) { return d.id; });

					// Exit any old nodes.
					forceLayout.node.exit().remove();

					// Create any new nodes
					var nodeEnter = forceLayout.node.enter()
						.append("g")
						.attr({
							class: "node",
							transform: function(d) {
								return "translate(" +
										$scope.xZoomScale(d.x) + ", " +
										$scope.yZoomScale(d.y) + ")";
							}
						})
						.each(function(d) {
							// Traverse each node, saving a pointer
							// to the node in the hostList to
							// facilitate updating later
							if(d.hasOwnProperty("hostInfo")) {
								if(!d.hostInfo.hasOwnProperty("g")) {
									d.hostInfo.g = new Array;
								}
								d.hostInfo.g.push(this);
							}
							addForceMapNodeGroupContents(d, this);
						})
						.call($scope.force.drag);

					// Update existing nodes
					forceLayout.node
						.select("circle")
						.transition()
						.duration(duration)
						.attr({
							r: function(d) {
								return $scope.nodeScale(getServiceCount(d));
							}
						})
						.style({
							stroke: function(d) {
								return getNodeStroke(getHostStatus(d),
										d.collapsed);
							},
							fill: function(d) {
								return getNodeFill(getHostStatus(d), false);
							}
						});

					forceLayout.node
						.select("text")
						.style({
							filter: function(d) {
								return getTextFilter(d);
							},
							stroke: function(d) {
								return getTextStrokeColor(d);
							}
						});
				};

				// Create the value function
				var getPartitionNodeValue = function(d) {

					if(d.hasOwnProperty("hostInfo") &&
							d.hostInfo.hasOwnProperty("serviceCount")) {
						return d.hostInfo.serviceCount == 0 ? 1 :
								d.hostInfo.serviceCount;
					}
					else {
						return 1;
					}
				};

				// Calculate the dimensions for the user supplied layout
				var calculateUserSuppliedDimensions = function() {

					switch ($scope.dimensions) {
					case "auto":
						// Create a temporary array with pointers
						// to the object JSON data
						ojdata = new Array;
						for(var host in $scope.hostList) {
							if(host != null) {
								ojdata.push($scope.hostList[host].objectJSON);
							}
						}
						// Determine dimensions based on included objects
						userSuppliedLayout.dimensions.upperLeft.x =
							ojdata[0].x_2d;
						userSuppliedLayout.dimensions.upperLeft.x =
							ojdata.reduce(function(a, b) {
								return a < b.x_2d ? a : b.x_2d;
							});
						userSuppliedLayout.dimensions.upperLeft.y =
							ojdata[0].y_2d;
						userSuppliedLayout.dimensions.upperLeft.y =
							ojdata.reduce(function(a, b) {
								return a < b.y_2d ? a : b.y_2d;
							});
						userSuppliedLayout.dimensions.lowerRight.x =
							ojdata[0].x_2d;
						userSuppliedLayout.dimensions.lowerRight.x =
							ojdata.reduce(function(a, b) {
								return a > b.x_2d ? a : b.x_2d;
							});
						userSuppliedLayout.dimensions.lowerRight.y =
							ojdata[0].y_2d;
						userSuppliedLayout.dimensions.lowerRight.y =
							ojdata.reduce(function(a, b) {
								return a > b.y_2d ? a : b.y_2d;
							});
						break;
					case "fixed":
						userSuppliedLayout.dimensions.upperLeft.x = 0;
						userSuppliedLayout.dimensions.upperLeft.y = 0;
						userSuppliedLayout.dimensions.lowerRight.x =
								$scope.svgWidth;
						userSuppliedLayout.dimensions.lowerRight.y =
								$scope.svgHeight;
						break;
					case "user":
						userSuppliedLayout.dimensions.upperLeft.x =
								$scope.ulx;
						userSuppliedLayout.dimensions.upperLeft.y =
								$scope.uly;
						userSuppliedLayout.dimensions.lowerRight.x =
								$scope.lrx;
						userSuppliedLayout.dimensions.lowerRight.y =
								$scope.lry;
						break;
					}
				};

				// What to do when the resize handle is dragged
				var onResizeDrag = function() {

					// Get the drag event
					var event = d3.event;

					// Resize the div
					$scope.svgWidth = event.x;
					$scope.svgHeight = event.y;

					// Propagate changes to parent scope (so, for example,
					// menu icon is redrown immediately). Note that it
					// doesn't seem to matter what we apply, so the
					// empty string is applied to decouple this directive
					// from it's parent's scope.
					$scope.$parent.$apply("");

					updateOnResize(this);
				};

				var updateOnResize = function(resizeHandle) {
					d3.select("div#mapsvg")
						.style({
							height: function() {
								return $scope.svgHeight + "px";
							},
							width: function() {
								return $scope.svgWidth + "px";
							}
						})
					$scope.diameter = Math.min($scope.svgHeight,
							$scope.svgWidth);

					// Update the scales
					switch($scope.layout) {
					case layouts.UserSupplied.index:
						switch($scope.dimensions) {
						case "auto":
							userSuppliedLayout.xScale.range([0 +
									layouts.UserSupplied.padding.left,
									$scope.svgWidth -
									layouts.UserSupplied.padding.right]);
							userSuppliedLayout.yScale.range([0 +
									layouts.UserSupplied.padding.top,
									$scope.svgHeight -
									layouts.UserSupplied.padding.bottom]);
							break;
						case "fixed":
							userSuppliedLayout.dimensions.lowerRight.x =
									$scope.svgWidth;
							userSuppliedLayout.dimensions.lowerRight.y =
									$scope.svgHeight;
							// no break;
						case "user":
							userSuppliedLayout.xScale.range([0,
									$scope.svgWidth]);
							userSuppliedLayout.yScale.range([0,
									$scope.svgHeight]);
							break;
						}
						break;
					}

					// Resize the svg
					d3.select("svg#map")
						.style({
							height: $scope.svgHeight,
							width: $scope.svgWidth
						})

					// Update the container transform
					d3.select("svg#map g#container")
						.attr({
							transform: function() {
								return getContainerTransform();
							}
						});

					// Update the appropriate layout
					switch($scope.layout) {
					case layouts.DepthLayers.index:
					case layouts.DepthLayersVertical.index:
					case layouts.CollapsedTree.index:
					case layouts.CollapsedTreeVertical.index:
					case layouts.BalancedTree.index:
					case layouts.BalancedTreeVertical.index:
					case layouts.CircularBalloon.index:
						// Update the tree size
						$scope.tree.size(getTreeSize())
						break;
					case layouts.CircularMarkup.index:
						// Update the partition size
						var radius = $scope.diameter / 2 -
								layouts.CircularMarkup.padding;
						var exponent = layouts.CircularMarkup.radialExponent;
						$scope.partition.size([2 * Math.PI,
								Math.pow(radius, exponent)]);
						break;
					case layouts.Force.index:
						$scope.force.size([$scope.svgWidth -
								2 * layouts.Force.outsidePadding,
								$scope.svgHeight -
								2 * layouts.Force.outsidePadding]);
						break;
					}

					// Move the resize handle
					if($scope.allowResize) {
						d3.select(resizeHandle)
							.attr({
								transform: function() {
									x = $scope.svgWidth -
											($scope.handleWidth +
											$scope.handlePadding);
									y = $scope.svgHeight -
											($scope.handleHeight +
											$scope.handlePadding);
									return "translate(" + x + ", " +
											y + ")";
								}
							});
					}

					// Update the contents
					switch($scope.layout) {
					case layouts.UserSupplied.index:
						d3.selectAll("g.node circle")
							.attr({
								transform: function(d) {
									return getNodeTransform(d);
								}
							});
						d3.selectAll("g.node text")
							.each(function(d) {
								setTextAttrs(d, this);
							});
						break;
					case layouts.DepthLayers.index:
					case layouts.DepthLayersVertical.index:
					case layouts.CollapsedTree.index:
					case layouts.CollapsedTreeVertical.index:
					case layouts.BalancedTree.index:
					case layouts.BalancedTreeVertical.index:
					case layouts.CircularBalloon.index:
						$scope.updateDuration = 0;
						updateTreeMap($scope.hostTree);
						break;
					case layouts.CircularMarkup.index:
						$scope.updateDuration = 0;
						updatePartitionMap($scope.hostTree);
						break;
					case layouts.Force.index:
						updateForceMap($scope.hostTree);
						break;
					}
				};

				// Set up the resize function
				setupResize = function() {

					// Create the drag behavior
					var drag = d3.behavior.drag()
						.origin(function() {
							return { x: $scope.svgWidth,
									y: $scope.svgHeight };
						})
						.on("dragstart", function() {
							 // silence other listeners
							d3.event.sourceEvent.stopPropagation();
						})
						.on("drag", onResizeDrag);

					// Create the resize handle
					d3.select("svg#map")
						.append("g")
						.attr({
							id: "resize-handle",
							transform: function() {
								x = $scope.svgWidth - ($scope.handleWidth +
										$scope.handlePadding);
								y = $scope.svgHeight -
										($scope.handleHeight +
										$scope.handlePadding);
								return "translate(" + x + ", " + y + ")";
							}
						})
						.call(drag)
						.append("path")
						.attr({
							d: function() {
								return "M 0 " + $scope.handleHeight +
										" L " + $scope.handleWidth + " " +
										$scope.handleHeight + " L " +
										$scope.handleWidth + " " + 0 +
										" L " + 0 + " " +
										$scope.handleHeight;
							},
							stroke: "black",
							fill: "black"
						});
				};

				// Get the node container transform
				getContainerTransform = function() {

					switch($scope.layout) {
					case layouts.UserSupplied.index:
					case layouts.Force.index:
						return null;
						break;
					case layouts.DepthLayers.index:
						return "translate(0, " +
								layouts.DepthLayers.topPadding + ")";
						break;
					case layouts.DepthLayersVertical.index:
						return "translate(" +
								layouts.DepthLayersVertical.leftPadding +
								", 0)";
						break;
					case layouts.CollapsedTree.index:
						return "translate(0, " +
								layouts.CollapsedTree.topPadding +
								")";
						break;
					case layouts.CollapsedTreeVertical.index:
						return "translate(" +
								layouts.CollapsedTreeVertical.leftPadding +
								", 0)";
						break;
					case layouts.BalancedTree.index:
						return "translate(0, " +
								layouts.BalancedTree.topPadding + ")";
						break;
					case layouts.BalancedTreeVertical.index:
						return "translate(" +
								layouts.BalancedTreeVertical.leftPadding +
								", 0)";
						break;
					case layouts.CircularBalloon.index:
					case layouts.CircularMarkup.index:
						var zoomTranslate = $scope.zoom.translate();
						var zoomScale = $scope.zoom.scale();
						var translate = [zoomTranslate[0] +
								($scope.svgWidth / 2) * zoomScale,
								zoomTranslate[1] +
								($scope.svgHeight / 2) * zoomScale];
						return "transform", "translate(" + translate +
								") scale(" + zoomScale + ")";
						break;
					default:
						return null;
						break;
					}
				};

				// Display the map
				var displayMap = function() {

					displayMapDone = false;

					// Update the scales
					switch($scope.layout) {
					case layouts.UserSupplied.index:
						switch($scope.dimensions) {
						case "auto":
							userSuppliedLayout.xScale
								.range([0 +
										layouts.UserSupplied.padding.left,
										$scope.svgWidth -
										layouts.UserSupplied.padding.right]);
							userSuppliedLayout.yScale
								.range([0 +
										layouts.UserSupplied.padding.top,
										$scope.svgHeight -
										layouts.UserSupplied.padding.bottom]);
							break;
						case "fixed":
						case "user":
							userSuppliedLayout.xScale
								.range([0, $scope.svgWidth]);
							userSuppliedLayout.yScale
								.range([0, $scope.svgHeight]);
							break;
						}
						break;
					}

					// Resize the svg
					d3.select("svg#map")
						.style({
							height: $scope.svgHeight,
							width: $scope.svgWidth
						});

					var container = d3.select("g#container");

					// Build the appropriate layout
					switch($scope.layout) {
					case layouts.DepthLayers.index:
					case layouts.DepthLayersVertical.index:
					case layouts.CollapsedTree.index:
					case layouts.CollapsedTreeVertical.index:
					case layouts.BalancedTree.index:
					case layouts.BalancedTreeVertical.index:
					case layouts.CircularBalloon.index:
						// Append a group for the links
						container.append("g")
							.attr({
								id: "links"
							});

						// Build the tree
						var treeSize = getTreeSize();
						$scope.tree = d3.layout.tree()
							.size(treeSize)
							.separation(function(a, b) {
								switch($scope.layout) {
								case layouts.DepthLayers.index:
								case layouts.DepthLayersVertical.index:
								case layouts.CollapsedTree.index:
								case layouts.CollapsedTreeVertical.index:
								case layouts.BalancedTree.index:
								case layouts.BalancedTreeVertical.index:
									return a.parent == b.parent ? 1 : 2;
									break;
								case layouts.CircularBalloon.index:
									var d = a.depth > 0 ? a.depth : b.depth, sep;
									if (d <= 0)
										d = 1;
									sep = (a.parent == b.parent ? 1 : 2) / d;
									return sep;
									break;
								}
							});
						break;
					case layouts.CircularMarkup.index:
						// Append a group for the links
						container.append("g")
							.attr({
								id: "paths"
							});

						// Build the partition
						var radius = $scope.diameter / 2 -
								layouts.CircularMarkup.padding;
						var exponent = layouts.CircularMarkup.radialExponent;
						$scope.partition = d3.layout.partition()
//							.sort(cmpHostName)
							.size([2 * Math.PI, Math.pow(radius, exponent)])
							.value(getPartitionNodeValue);
						break;
					case layouts.Force.index:
						// Append a group for the links
						container.append("g")
							.attr({
								id: "links"
							});

						// Build the layout
						$scope.force = d3.layout.force()
							.size([$scope.svgWidth -
									2 * layouts.Force.outsidePadding,
									$scope.svgHeight -
									2 * layouts.Force.outsidePadding])
							.on("tick", onForceTick);
						break;
					}

					// Create the diagonal that will be used to
					// connect the nodes
					switch($scope.layout) {
					case layouts.DepthLayers.index:
					case layouts.CollapsedTree.index:
					case layouts.BalancedTree.index:
						$scope.diagonal = d3.svg.diagonal()
							.projection(function(d) {
								return [$scope.xZoomScale(d.x),
										$scope.yZoomScale(d.y)];
							});
						break;
					case layouts.DepthLayersVertical.index:
					case layouts.CollapsedTreeVertical.index:
					case layouts.BalancedTreeVertical.index:
						$scope.diagonal = d3.svg.diagonal()
							.projection(function(d) {
								return [$scope.xZoomScale(d.y),
										$scope.yZoomScale(d.x)];
							});
						break;
					case layouts.CircularBalloon.index:
						$scope.diagonal = d3.svg.diagonal.radial()
							.projection(function(d) {
								var angle = 0;
								if(!isNaN(d.x)) {
									angle = d.x +
											layouts.CircularBalloon.rotation
											+ 90;
								}
								return [d.y * $scope.zoom.scale(),
										((angle / 180) * Math.PI)];
							});
						break;
					}

					// Create the arc this will be used to display the nodes
					switch($scope.layout) {
					case layouts.CircularMarkup.index:
						$scope.arc = d3.svg.arc()
							.startAngle(function(d) { return d.x; })
							.endAngle(function(d) { return d.x + d.dx; })
							.innerRadius(function(d) {
								return Math.pow(d.y, (1 / exponent));
							})
							.outerRadius(function(d) {
								return Math.pow(d.y + d.dy, (1 / exponent));
							});
						break;
					}

					// Set the focal point to the root
					$scope.focalPoint = $scope.hostTree;

					// Signal the fact that displayMap() is done
					displayMapDone = true;
				};

				// Activities that take place only on
				// directive instantiation
				var onDirectiveInstantiation = function() {

					// Create the zoom behavior
					$scope.xZoomScale = d3.scale.linear();
					$scope.yZoomScale = d3.scale.linear();
					$scope.zoom = d3.behavior.zoom()
						.scaleExtent([1 / $scope.maxzoom, $scope.maxzoom])
						.x($scope.xZoomScale)
						.y($scope.yZoomScale)
						.on("zoomstart", onZoomStart)
						.on("zoom", onZoom);

					// Set up the div containing the map and
					// attach the zoom behavior to the it
					d3.select("div#mapsvg")
						.style({
							"z-index": $scope.mapZIndex,
							height: function() {
								return $scope.svgHeight + "px";
							},
							width: function() {
								return $scope.svgWidth + "px";
							}
						})
						.call($scope.zoom);

					// Set up the resize function
					if($scope.allowResize) {
						setupResize();
					}

					// Create a container group
					d3.select("svg#map")
						.append("g")
						.attr({
							id: "container",
							transform: function() {
								return getContainerTransform();
							}
						});

					// Create scale to size nodes based on
					// number of services
					$scope.nodeScale = d3.scale.linear()
						.domain([0, $scope.maxRadiusCount])
						.range([$scope.minRadius, $scope.maxRadius])
						.clamp(true);

				};

				onDirectiveInstantiation();
			}
		};
	});
