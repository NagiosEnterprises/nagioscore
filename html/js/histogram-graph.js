angular.module("histogramApp")
	.directive("nagiosHistogram", function() {
		return {
			templateUrl: "histogram-graph.html",
			restrict: "AE",
			scope: {
				cgiurl: "@cgiurl",
				reporttype: "@reporttype",
				host: "@host",
				service: "@service",
				timeperiod: "@timeperiod",
				t1: "@t1",
				t2: "@t2",
				breakdown: "@breakdown",
				graphevents: "@graphevents",
				graphstatetypes: "@graphstatetypes",
				assumestateretention: "@assumestateretention",
				initialstateslogged: "@initialstateslogged",
				ignorerepeatedstates: "@ignorerepeatedstates",
				lastUpdate: "=lastUpdate",
				reload: "@reload",
				build: "&build"
			},
			controller: function($scope, $element, $attrs, $http,
					histogramEventsService, statisticsBreakdown) {

				// Layout variables
				$scope.fontSize = 11;
				$scope.graphMargin = {
					top: 40,
					right: 290,
					bottom: 84,
					left: 60
				};
				$scope.svgHeight = 320;
				$scope.svgWidth = 900;
				$scope.dateFormat = d3.time.format("%a %b %d %H:%M:%S %Y");

				// Application state variables
				$scope.fetchingAlerts = false;

				$scope.$watch("reload", function(newValue) {
					// Remove any previous graphs
					d3.select("g#grid").selectAll("path").remove();
					// Set the start and end times
					$scope.startTime = $scope.t1 * 1000;
					$scope.endTime = $scope.t2 * 1000;
					// Show the histogram
					showHistogram();
				});

				// Get the statistics breakdown label
				$scope.statisticsBreakdownLabel = function(value) {
					for (var i = 0; i < statisticsBreakdown.length; i++) {
						var bd = statisticsBreakdown[i];
						if (bd.value == value) {
							return bd.label;
						}
					}
				};

				// Get the x-axis scale domain
				var getXScaleDomain = function() {

					// Set up the x-axis scale domain
					var domain = [];

					switch($scope.breakdown) {
					case "monthly":
						domain = d3.range(0,13);
						break;
					case "dayofweek":
						domain = d3.range(0,8);
						break;
					case "hourly":
						domain = d3.range(0,25);
						break;
					case "dayofmonth":
					default:
						domain = d3.range(0,32);
						break;
					}

					return domain;
				};

				// Get the x-axis tick values
				var getXAxisTickValue = function(d) {

					// Set up the x-axis tick values
					var values = [];

					switch($scope.breakdown) {
					case "monthly":
						var months = ["January", "February", "March",
								"April", "May", "June", "July", "August",
								"September", "October", "November",
								"December", "January"];
						return months[d % 12];
						break;
					case "dayofweek":
						var days = ["Sunday", "Monday", "Tuesday",
								"Wednesday", "Thursday", "Friday",
								"Saturday", "Sunday"];
						return days[d % 7];
						break;
					case "hourly":
						var hourFormat = d3.format("02d");
						return hourFormat(d % 24) + ":00";
						break;
					case "dayofmonth":
					default:
						var value = ((d + 1) % 32);
						return (value == 0 ? 1 : value);
						break;
					}
				};

				// Get the number of periods for the breakdown
				$scope.getBreakdownPeriods = function() {
					switch($scope.breakdown) {
					case "monthly":
						return 12;
						break;
					case "dayofweek":
						return 7;
						break;
					case "hourly":
						return 24;
						break;
					case "dayofmonth":
					default:
						return 31;
						break;
					}
				};

				// Determine the y-axis maximum
				getYMax = function() {

					var states = ["up", "down", "unreachable", "ok",
							"warning", "unknown", "critical" ];
					var yMax = 0;

					states.forEach(function(e, i, a) {
						yMax = Math.max($scope.summary[$scope.breakdown].maxima[e], yMax);
					});

					return yMax;
				};

				// Display the graph
				var displayGraph = function() {

					// Local variables
					var graphHeight = $scope.svgHeight -
							($scope.graphMargin.bottom +
							$scope.graphMargin.top);
					var graphWidth = $scope.svgWidth -
							($scope.graphMargin.right +
							$scope.graphMargin.left);
					var gridCenter = $scope.graphMargin.left +
							graphWidth / 2;

					// Get the header group
					var gridGroup = d3.select("svg#histogram")
						.select("g#grid")
						.attr({
							transform: function() {
								return "translate(" +
										$scope.graphMargin.left + "," +
										$scope.graphMargin.top + ")";
							}
						});

					// Build the x-axis scale
					var xScale = d3.scale.ordinal()
						.domain(getXScaleDomain())
						.rangePoints([0, graphWidth]);

					// Build the x-axis
					var xAxis = d3.svg.axis()
						.scale(xScale)
						.orient("bottom")
						.tickSize(graphHeight)
						.tickFormat(function(d) {
							return getXAxisTickValue(d);
						});

					// Display the x-axis
					d3.select("g#xaxis").call(xAxis);

					// Rotate the value labels for the x-axis
					var translate = -(graphHeight + ($scope.fontSize / 2));
					d3.select("g#xaxis")
						.selectAll("text")
						.attr({
							transform: function() {
								return "rotate(-90) translate(" +
										translate + "," + translate + ")";
							}
						})
						.style({
							"text-anchor": "end"
						});

					// Build the y-axis scale
					var yScale = d3.scale.linear()
						.domain([getYMax(), 0])
						.rangeRound([0, graphHeight]);

					// Build the y-axis
					var yAxis = d3.svg.axis()
						.scale(yScale)
						.orient("left")
						.tickSize(graphWidth);

					// Display the y-axis
					d3.select("g#yaxis").call(yAxis);

					// Generate the lines
					var states = [];
					if($scope.reporttype == "hosts") {
						states = ["up", "down", "unreachable"];
					}
					else {
						states = ["ok", "warning", "unknown", "critical"];
					}
					states.forEach(function(e, i, a) {
						var line = d3.svg.line()
							.x(function(d, i) { return xScale(i); })
							.y(function(d) { return yScale(d[e]); })
							.interpolate("linear");

						gridGroup.append("path")
							.attr({
								"d": line($scope.summary[$scope.breakdown].details),
								"class": e
							});
					});
				};

				// Initialize the data for display
				var initializeData = function(obj, size) {

					var states = ["up", "down", "unreachable", "ok",
							"warning", "unknown", "critical" ];

					obj.maxima = new Object;
					obj.minima = new Object;
					obj.totals = new Object;
					states.forEach(function(e, i, a) {
						obj.maxima[e] = 0;
						obj.minima[e] = -1;
						obj.totals[e] = 0;
					});

					obj.details = new Array;
					for(var i = 0; i < size; i++) {
						obj.details.push({
							"up": 0,
							"down": 0,
							"unreachable": 0,
							"ok": 0,
							"warning": 0,
							"unknown": 0,
							"critical": 0
						});
					}
				};

				// Update the summary data
				var updateData = function(obj, state, index) {

					obj.details[index][state]++;
					obj.maxima[state] = Math.max(obj.maxima[state],
							obj.details[index][state]);
					obj.totals[state]++;
					if(index == 0) {
						// If this is the first entry in the breakdown,
						// also update the last because the graph
						// "wraps around"
						obj.details[obj.details.length - 1][state]++;
					}
				};

				// Calculate the minima
				var calculateMinima = function(obj) {
					for(var key in obj.minima) {
						obj.minima[key] = obj.details[0][key];
						for(var j = 1; j < obj.details.length; j++) {
							obj.minima[key] = Math.min(obj.minima[key],
									obj.details[j][key]);
						}
					}
				};

				// Summarize the data for display
				var summarizeData = function() {

					// Initialize the data
					$scope.summary = {
						monthly: new Object,
						dayofmonth: new Object,
						dayofweek: new Object,
						hourly: new Object,
					};
					initializeData($scope.summary.monthly, 13);
					initializeData($scope.summary.dayofmonth, 32);
					initializeData($scope.summary.dayofweek, 8);
					initializeData($scope.summary.hourly, 25);

					$scope.json.data.alertlist.forEach(function(e, i, a) {
						// Create a Javascript date object
						var ts = new Date(e.timestamp);

						// Update the monthly data
						updateData($scope.summary.monthly, e.state,
								ts.getMonth());

						// Update the day of month data
						updateData($scope.summary.dayofmonth, e.state,
								ts.getDate() - 1);
		
						// Update the day of week data
						updateData($scope.summary.dayofweek, e.state,
								ts.getDay());
		
						// Update the hourly data
						updateData($scope.summary.hourly, e.state,
								ts.getHours());

					});

					// Calculate the minima
					calculateMinima($scope.summary.monthly);
					calculateMinima($scope.summary.dayofmonth);
					calculateMinima($scope.summary.dayofweek);
					calculateMinima($scope.summary.hourly);
				};

				// Show the graph
				var showHistogram = function() {

					if(!$scope.build()) {
						return;
					}

					// Build the list of parameters for the JSON query
					var parameters = {
						query: "alertlist",
						formatoptions: "enumerate bitmask",
						objecttypes: ($scope.reporttype == "hosts" ?
								"host" : "service"),
						hostname: $scope.host,
						starttime: $scope.t1,
						endtime: $scope.t2,
						statetypes: function() {
							switch($scope.graphstatetypes) {
							case 1:
								return "soft";
								break;
							case 2:
								return "hard";
								break;
							case 3:
							default:
								return "hard soft";
								break;
							}
						}(),
						backtrackedarchives: $scope.backtracks
					};

					switch($scope.reporttype) {
					case "hosts":
						var events = histogramEventsService.hostEvents.filter(function(e) {
							return e.value == $scope.graphevents;
						});
						if(events.length > 0) {
							parameters.hoststates = events[0].states;
						}
						else {
							parameters.hoststates = "up down unreachable";
						}
						break;
					case "services":
						var events = histogramEventsService.serviceEvents.filter(function(e) {
							return e.value == $scope.graphevents;
						});
						parameters.servicedescription = $scope.service;
						if(events.length > 0) {
							parameters.servicestates = events[0].states;
						}
						else {
							parameters.servicestates =
									"ok warning unknown critical";
						}
						break;
					}

					var getConfig = {
						params: parameters,
						withCredentials: true
					};

					// Where to place the spinner
					var spinnerdiv = d3.select("div#spinner");
					var spinner = null;

					// Send the JSON query
					$http.get($scope.cgiurl + "archivejson.cgi", getConfig)
						.error(function(err) {
							console.warn(err);
							// Stop the spinner
							$scope.fetchingAlerts = false;
							spinner.stop();
						})
						.success(function(json) {
							// Stop the spinner
							$scope.fetchingAlerts = false;
							spinner.stop();
		
							// Save the json results
							$scope.json = json;
	
							// Record the query time
							$scope.lastUpdate = new Date(json.result.query_time);
	
							// Summarize the results
							summarizeData();

							// Display the graph
							displayGraph();

							// Display the summary
						});

					// Start the spinner
					$scope.fetchingAlerts = true;
					spinner = new Spinner($scope.spinnerOpts).spin(spinnerdiv[0][0]);
				};

			}
		};
	});
