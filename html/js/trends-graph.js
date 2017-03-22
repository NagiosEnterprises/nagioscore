angular.module("trendsApp")
	.directive("nagiosTrends", function() {
		return {
			templateUrl: "trends-graph.html",
			restrict: "AE",
			scope: {
				cgiurl: "@cgiurl",
				reporttype: "@reporttype",
				host: "@host",
				service: "@service",
				timeperiod: "@timeperiod",
				t1: "@t1",
				t2: "@t2",
				assumeinitialstates: "@assumeinitialstates",
				assumestateretention: "@assumestateretention",
				assumestatesduringnotrunning: "@assumestatesduringnotrunning",
				includesoftstates: "@includesoftstates",
				initialassumedhoststate: "@initialassumedhoststate",
				initialassumedservicestate: "@initialassumedservicestate",
				backtrack: "@backtrack",
				nopopups: "@nopopups",
				nomap: "@nomap",
				lastUpdate: "=lastUpdate",
				reload: "@reload",
				build: "&build"
			},
			controller: function($scope, $element, $attrs, $http,
					spinnerOpts) {

				// Global variables
				$scope.fontSize = 11;
				$scope.states = null;
				$scope.stateChangeMargin = {
					top: 53,
					right: 280,
					bottom: 156,
					left: 110
				};
				$scope.svgHeight = $scope.svgHostHeight;
				$scope.svgWidth = 900;
				$scope.dateFormat = d3.time.format("%a %b %d %H:%M:%S %Y");
				$scope.showPopups = $scope.nopopups == "false";

				// State information for services
				$scope.serviceStates = {
					"ok": {
						"label": "Ok",
						"popupText": "OK",
						"y": 13,
						"color": { "r": 0, "g": 210, "b": 0 }
					},
					"warning": {
						"label": "Warning",
						"popupText": "WARNING",
						"y": 36,
						"color": { "r": 176, "g": 178, "b": 20 }
					},
					"unknown": {
						"label": "Unknown",
						"popupText": "UNKNOWN",
						"y": 59,
						"color": { "r": 255, "g": 100, "b": 25 }
					},
					"critical": {
						"label": "Critical",
						"popupText": "CRITICAL",
						"y": 82,
						"color": { "r": 255, "g": 0, "b": 0 }
					},
					"nodata": {
						"label": "Indeterminate",
						"popupText": "?",
						"y": 107,
						"color": { "r": 0, "g": 0, "b": 0, "a": 0 }
					}
				};
	
				// State information for hosts
				$scope.hostStates = {
					"up": {
						"label": "Up",
						"popupText": "UP",
						"y": 13,
						"color": { "r": 0, "g": 210, "b": 0 }
					},
					"down": {
						"label": "Down",
						"popupText": "DOWN",
						"y": 38,
						"color": { "r": 255, "g": 0, "b": 0 }
					},
					"unreachable": {
						"label": "Unreachable",
						"popupText": "UNREACHABLE",
						"y": 63,
						"color": { "r": 128, "g": 0, "b": 0 }
					},
					"nodata": {
						"label": "Indeterminate",
						"popupText": "?",
						"y": 88,
						"color": { "r": 0, "g": 0, "b": 0, "a": 0 }
					}
				};

				// Application state variables
				$scope.fetchingStateChanges = false;
				$scope.fetchingAvailability = false;
				$scope.popupDisplayed = false;
				$scope.popupDisplayedOnZoomStart = false;

				$scope.$watch("reload", function(newValue) {
					// Set the graph dimensions
					switch ($scope.reporttype) {
					case "hosts":
						$scope.svgHeight = 300;
						break;
					case "services":
						$scope.svgHeight = 320;
						break;
					default:
						$scope.svgHeight = 0;
						break;
					}
					// Set the start and end times
					$scope.startTime = $scope.t1 * 1000;
					$scope.endTime = $scope.t2 * 1000;
					// Show the trend
					if($scope.build()) {
						displayStateChanges();
						getAvailability($scope.t1, $scope.t2);
					}
				});

				$scope.getYAxisTemplate = function() {
					switch ($scope.reporttype) {
					case "hosts":
						return "trends-host-yaxis.html";
						break;
					case "services":
						return "trends-service-yaxis.html";
						break;
					default:
						return null;
						break;
					}
				};

				// Create a color from a color object
				var genColor = function(color) {
					if(color.hasOwnProperty("a")) {
						return "rgba(" + color.r + "," + color.g + "," +
								color.b + "," + color.a + ")";
					}
					else {
						return "rgb(" + color.r + "," + color.g + "," +
								color.b + ")";
					}
				};

				var setPopupContents = function(popup, previousState,
						currentState) {

					$scope.popupContents = {
						state: $scope.states[previousState.state].popupText,
						start: previousState.timestamp,
						end: currentState.timestamp,
						duration: currentState.timestamp -
								previousState.timestamp,
						info: previousState.plugin_output
					};
					$scope.$apply($scope.popupContents);
				};

				var getTickValues = function(stateChangeList) {

					// Set up tick values, skipping those that are too
					// close together
					var tickValues = [];
					tickValues.push(stateChangeList[0].timestamp);
					for(var i = 1; i < stateChangeList.length; i++) {
						var ts = stateChangeList[i].timestamp;
						if($scope.xScale(ts) >
								$scope.xScale(tickValues[tickValues.length - 1]) +
								$scope.fontSize) {
							tickValues.push(ts);
						}
					}

					return tickValues;
				};

				var displayGrid = function(zoom, sidePadding) {

					// Local layout variables
					var bottomPadding = 3;
					var gridWidth = $scope.svgWidth -
							($scope.stateChangeMargin.left +
							$scope.stateChangeMargin.right);
					var gridHeight = $scope.svgHeight -
							($scope.stateChangeMargin.top +
							$scope.stateChangeMargin.bottom);

					// Adjust the grid group attributes
					d3.select("g#grid").call(zoom);

					// Generate rectangles for state durations
					var rects = d3.select("g#groupRects")
						.selectAll("rect.state")
						.data($scope.stateChangeJSON.data.statechangelist,
								function(d) {
							return d.timestamp;
						})
						.enter()
						.append("rect")
						.attr({
							x: function(d, i) {
								if(i == 0) { return 0; }
								var ts = d.previous.timestamp;
								return $scope.xScale(ts);
							},
							y: function(d, i) {
								if(i == 0) { return 0; }
								var state = d.previous.state;
								return $scope.states[state].y - 1;
							},
							width: function(d, i) {
								if(i == 0) { return 0; }
								var ts = d.previous.timestamp;
								return $scope.xScale(d.timestamp) -
										$scope.xScale(ts);
							},
							height: function(d, i) {
								if(i == 0) { return 0; }
								var state = d.previous.state;
								return gridHeight - $scope.states[state].y -
										bottomPadding;
							},
							fill: function(d, i) {
								if(i == 0) { return "white"; }
								var state = d.previous.state;
								return genColor($scope.states[state].color);
							},
							class: "state"
						});

					if($scope.showPopups) {
						rects.on("mouseover", function(d, i) {
								var mouse = d3.mouse(this);
								var popup = d3.select("#popup")
									.style({
										left: (mouse[0] +
												$scope.stateChangeMargin.left) +
												"px",
										top: (mouse[1] +
												$scope.stateChangeMargin.top) +
												"px"
									});
								setPopupContents(popup, d.previous, d);
								$scope.popupDisplayed = true;
								$scope.$apply("popupDisplayed");
							})
							.on("mouseout", function(d, i) {
								$scope.popupDisplayed = false;
								$scope.$apply("popupDisplayed");
							});
					}

					// Display the x-axis
					var xTranslate = -gridHeight;
					var yTranslate = -($scope.fontSize / 2) - gridHeight;
					var x = d3.select("g#groupXAxis").call($scope.xAxis);
					x.selectAll("text")
						.attr({
							class: "xaxis",
							transform: "rotate(-90) translate(" +
									xTranslate + "," + yTranslate + ")"
						})
						.style({
							"text-anchor": "end"
						});
					x.selectAll("line")
						.attr({
							class: "vLine",
						});
					d3.select("g#groupXAxis").select("path").remove();
				};

				// Handle a succesful availability response
				var onAvailabilitySuccess = function(json) {

					// Local layout variables
					var dataPosition = 100;

					// Attach the json to the current scope
					$scope.availabilityJSON = json;

					var totalTime = (json.data.selectors.endtime -
							json.data.selectors.starttime) / 1000;
					switch ($scope.reporttype) {
					case "services":
						$scope.states = $scope.serviceStates;
						$scope.availabilityStates =
								calculateAvailability(json.data.service,
								$scope.serviceStates, totalTime);
						break;
					case "hosts":
						$scope.states = $scope.hostStates;
						$scope.availabilityStates =
								calculateAvailability(json.data.host,
								$scope.hostStates, totalTime);
						break;
					}
				};

				// Display the availability information
				var getAvailability = function(start, end) {

					// Where to place the spinner
					var spinnerdiv = d3.select("div#availabilityspinner");
					var spinner = null;

					var parameters = {
						query: "availability",
						formatoptions: "enumerate bitmask",
						availabilityobjecttype: $scope.reporttype,
						hostname: $scope.host,
						statetypes: $scope.includesoftstates ?
								"hard soft" : "hard",
						starttime: start,
						endtime: end,
					};
					if($scope.reporttype == "services") {
						parameters.servicedescription = $scope.service;
					}

					var getConfig = {
						params: parameters,
						withCredentials: true
					};

					// Send the query
					$http.get($scope.cgiurl + "/archivejson.cgi?",
							getConfig)
						.error(function(err) {
							// Stop the spinner
							spinner.stop();
							$scope.fetchingAvailability = false;

							console.warn(err);
						})
						.success(function(json) {
							// Stop the spinner
							spinner.stop();
							$scope.fetchingAvailability = false;

							onAvailabilitySuccess(json);
						});

					// Start the spinner
					$scope.fetchingAvailability = true;
					spinner = new Spinner(spinnerOpts)
							.spin(spinnerdiv[0][0]);
				};

				var onStateChangeSuccess = function(json) {

					// Local layout variables
					var sidePadding = 3;

					// Attach the json to the current scope
					$scope.stateChangeJSON = json;

					// Record the time of the query
					$scope.lastUpdate = new Date(json.result.query_time);

					// Add a pseudo end state so drawing will be correct
					var last = $scope.stateChangeJSON.data.statechangelist[json.data.statechangelist.length - 1];
					if(last.timestamp < $scope.stateChangeJSON.data.selectors.endtime) {
						var final = {
							timestamp: $scope.stateChangeJSON.data.selectors.endtime
						};
						$scope.stateChangeJSON.data.statechangelist.push(final);
					}

					// For all but the first entry create a previous
					// entry "pointer"
					for(var i = 1;
							i < $scope.stateChangeJSON.data.statechangelist.length;
							i++) {
						$scope.stateChangeJSON.data.statechangelist[i].previous =
								$scope.stateChangeJSON.data.statechangelist[i-1];
					}

					// Determine whether the state change list is for a
					// host or service
					switch ($scope.reporttype) {
					case "services":
						$scope.states = $scope.serviceStates;
						break;
					case "hosts":
						$scope.states = $scope.hostStates;
						break;
					}

					var gridWidth = $scope.svgWidth -
							($scope.stateChangeMargin.left +
							$scope.stateChangeMargin.right);
					var gridHeight = $scope.svgHeight -
							($scope.stateChangeMargin.top +
							$scope.stateChangeMargin.bottom);

					// Set up the x-axis scale
					$scope.xScale = d3.time.scale()
						.domain([$scope.stateChangeJSON.data.selectors.starttime,
								$scope.stateChangeJSON.data.selectors.endtime])
						.rangeRound([sidePadding,
								gridWidth - sidePadding * 2])
						.clamp(true);

					// Set up x axis
					$scope.xAxis = d3.svg.axis()
						.scale($scope.xScale)
						.orient("bottom")
						.tickSize(gridHeight)
						.tickValues(function() {
							return getTickValues($scope.stateChangeJSON.data.statechangelist);
						})
						.tickFormat(function(d) {
							var ts = new Date(d);
							return $scope.dateFormat(ts);
						});

					// Create the zoom
					var maxExtent = Math.round(($scope.stateChangeJSON.data.selectors.endtime -
								$scope.stateChangeJSON.data.selectors.starttime) / 2000);
					var zoom = d3.behavior.zoom()
						.x($scope.xScale)
						.scaleExtent([1, maxExtent])
						.on("zoomstart", onZoomStart)
						.on("zoom", onZoom)
						.on("zoomend", onZoomEnd);

					// Display the grid
					displayGrid(zoom, sidePadding);
				};

				var displayStateChanges = function() {

					// Where to place the spinner
					var spinnerdiv = d3.select("div#gridspinner");
					var spinner = null;

					var parameters = {
						query: "statechangelist",
						formatoptions: "enumerate bitmask",
						objecttype: ($scope.reporttype == "hosts" ?
								"host" : "service"),
						hostname: $scope.host,
						starttime: $scope.t1,
						endtime: $scope.t2,
						statetypes: $scope.includesoftstates ?
								"hard soft" : "hard",
						backtrackedarchives: $scope.backtrack
					};
					if($scope.reporttype == "services") {
						parameters.servicedescription = $scope.service;
					}

					var getConfig = {
						params: parameters,
						withCredentials: true
					};

					// Send the query
					$http.get($scope.cgiurl + "/archivejson.cgi?",
							getConfig)
						.error(function(err) {
							// Stop the spinner
							spinner.stop();
							$scope.fetchingStateChanges = false;

							console.warn(err);
						})
						.success(function(json) {
							// Stop the spinner
							spinner.stop();
							$scope.fetchingStateChanges = false;

							onStateChangeSuccess(json);
						});

					// Clear the current grid
					d3.select("g#groupRects")
						.selectAll("rect.state")
						.data([])
						.exit()
						.remove();
		
					// Clear the current x-axis	
					if($scope.xAxis != undefined) {
						$scope.xAxis.tickValues([]);
						var x = d3.select("g#groupXAxis")
								.call($scope.xAxis);
						x.selectAll("text").remove();
						x.selectAll("line").remove();
						x.selectAll("path").remove();
					}

					// Start the spinner
					$scope.fetchingStateChanges = true;
					spinner = new Spinner(spinnerOpts)
							.spin(spinnerdiv[0][0]);
				};

				var onZoomStart = function() {
					$scope.popupDisplayedOnZoomStart = $scope.popupDisplayed;
					$scope.popupDisplayed = false;
					$scope.$apply("popupDisplayed");
				};

				var onZoom = function() {

					var gridHeight = $scope.svgHeight -
							($scope.stateChangeMargin.top +
							$scope.stateChangeMargin.bottom);
					var xTranslate = -gridHeight;
					var yTranslate = -($scope.fontSize / 2) - gridHeight;
					var domain = $scope.xScale.domain();

					// Update the x-axis
					var x = d3.select("g#groupXAxis");
					x.call($scope.xAxis);
					x.selectAll("text")
						.attr({
							class: "xaxis",
							transform: "rotate(-90) translate(" +
									xTranslate + "," + yTranslate + ")"
						})
						.style({
							"text-anchor": "end"
						})
						.text(function(d, i) {
							// This is a kludge to get the endpoints
							// to update
							if(i == 0) {
								return $scope.dateFormat(new Date(domain[0]));
							}
							else if(d > domain[1]) {
								return $scope.dateFormat(new Date(domain[1]));
							}
							else {
								return $scope.dateFormat(new Date(d));
							}
						});
					x.selectAll("line")
						.attr({
							class: "vLine",
						});
					x.select("path").remove();

					// Update the times for the header
					$scope.startTime = domain[0];
					$scope.$apply("startTime");
					$scope.endTime = domain[1];
					$scope.$apply("endTime");

					// Update the rectangles
					d3.select("g#groupRects")
						.selectAll("rect")
						.attr({
							x: function(d, i) {
								if(i == 0) { return 0; }
								var ts = d.previous.timestamp;
								return $scope.xScale(ts);
							},
							width: function(d, i) {
								if(i == 0) { return 0; }
								var ts = d.previous.timestamp;
								return $scope.xScale(d.timestamp) -
										$scope.xScale(ts);
							}
						});
				}

				var onZoomEnd = function() {

					// Get the new domain
					var domain = $scope.xScale.domain();

					// Determine the new start and end times
					var start = Math.round(domain[0].getTime() / 1000);
					var end = Math.round(domain[1].getTime() / 1000);
					if(start == end) {
						start--;
					}

					// Update the availability display
					getAvailability(start, end);

					// Re-display the popup if it was shown before zooming
					$scope.popupDisplayed = $scope.popupDisplayedOnZoomStart;
					$scope.$apply("popupDisplayed");
					$scope.popupDisplayedOnZoomStart = false;
				};

				var calculateAvailability = function(availability,
						layoutStates, totalTime) {

					var states = {};

					for(var s in layoutStates) {
						if(s == "up") {
							layoutStates[s].totalTime =
									availability.time_up +
									availability.scheduled_time_up;
						}
						else if(s == "down") {
							layoutStates[s].totalTime =
									availability.time_down +
									availability.scheduled_time_down;
						}
						else if(s == "unreachable") {
							layoutStates[s].totalTime =
									availability.time_unreachable +
									availability.scheduled_time_unreachable;
						}
						else if(s == "ok") {
							layoutStates[s].totalTime =
									availability.time_ok +
									availability.scheduled_time_ok;
						}
						else if(s == "warning") {
							layoutStates[s].totalTime =
									availability.time_warning +
									availability.scheduled_time_warning;
						}
						else if(s == "unknown") {
							layoutStates[s].totalTime =
									availability.time_unknown +
									availability.scheduled_time_unknown;
						}
						else if(s == "critical") {
							layoutStates[s].totalTime =
									availability.time_critical +
									availability.scheduled_time_critical;
						}
						else if(s == "nodata") {
							layoutStates[s].totalTime =
									availability.time_indeterminate_nodata +
									availability.time_indeterminate_notrunning;
						}
						layoutStates[s].percentageTime =
								layoutStates[s].totalTime / totalTime;
						states[s] = layoutStates[s];
					}

					return states;
				};
			}
		};
	});
