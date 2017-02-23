angular.module("trendsApp", ["ui.bootstrap", "ui.utils",
		"nagiosDecorations", "nagiosTime"])

	// Spinner options
	.constant("spinnerOpts", {
		lines: 11, // The number of lines to draw
		length: 11, // The length of each line
		width: 6, // The line thickness
		radius: 11, // The radius of the inner circle
		corners: 1, // Corner roundness (0..1)
		rotate: 0, // The rotation offset
		direction: 1, // 1: clockwise, -1: counterclockwise
		color: '#000', // #rgb or #rrggbb or array of colors
		speed: 1, // Rounds per second
		trail: 58, // Afterglow percentage
		shadow: false, // Whether to render a shadow
		hwaccel: false, // Whether to use hardware acceleration
		className: 'spinner', // The CSS class to assign to the spinner
		zIndex: 2e9, // The z-index (defaults to 2000000000)
		top: '50%', // Top position relative to parent
		left: '50%' // Left position relative to parent
	})

	.config(function($locationProvider) {
		$locationProvider.html5Mode({
			enabled: true,
			requireBase: false
		})
	})

	.controller("trendsCtrl", function($scope, $location, $modal, $http,
			nagiosTimeService) {
		$scope.search = $location.search();

		// URL parameters
		$scope.params = {
			cgiurl: $scope.search.cgiurl ? $scope.search.cgiurl : 
					$location.absUrl().replace(/trends\.html.*$/, "cgi-bin/"),
			reporttype: $scope.search.reporttype ?
					$scope.search.reporttype : "",
			host: $scope.search.host ? $scope.search.host : "",
			service: $scope.search.service ?
					$scope.search.service : "",
			timeperiod: $scope.search.timeperiod ?
					$scope.search.timeperiod : "",
			t1: $scope.search.t1 ? $scope.search.t1 : 0,
			t2: $scope.search.t2 ? $scope.search.t2 : 0,
			assumeinitialstates: $scope.search.assumeinitialstates ?
					true : false,
			assumestateretention: $scope.search.assumestateretention ?
					true : false,
			assumestatesduringnotrunning:
					$scope.search.assumestatesduringnotrunning ?
					true : false,
			includesoftstates: $scope.search.includesoftstates ?
					$scope.search.includesoftstates : false,
			initialassumedhoststate:
					$scope.search.initialassumedhoststate ?
					$scope.search.initialassumedhoststate : "",
			initialassumedservicestate:
					$scope.search.initialassumedservicestate ?
					$scope.search.initialassumedservicestate : "",
			backtrack: $scope.search.backtrack ?
					parseInt($scope.search.backtrack) : 4,
			nopopups: $scope.search.nopopups ? true : false,
			nomap: $scope.search.nomap ? true : false
		};

		// Decoration-related variables
		$scope.lastUpdate = "none";

		var notBlank = function(value) {
			return value != undefined && value != "";
		};

		// Determine the report type if not specified
		if ($scope.params.reporttype == "") {
			if (notBlank($scope.params.host) &&
					notBlank($scope.params.service)) {
				$scope.params.reporttype = "services";
			}
			else if (notBlank($scope.params.host)) {
				$scope.params.reporttype = "hosts";
			}
		}

		// Calculate start and end times based on report timeperiod
		if (nagiosTimeService.isCannedTimeperiod($scope.params.timeperiod)) {
				var times = nagiosTimeService.calculateReportTimes(new Date(),
						$scope.params.timeperiod);
				$scope.params.t1 = times.start.getTime() / 1000;
				$scope.params.t2 = times.end.getTime() / 1000;
		}
		else if ($scope.params.t1 == 0 && $scope.params.t2 == 0) {
			// or s* and e* variables, if specified
			var now = new Date();
			if ($scope.search.syear || $scope.search.smon ||
					$scope.search.sday || $scope.search.shour ||
					$scope.search.smin || $scope.search.ssec) {
				var year = $scope.search.syear ?
						parseInt($scope.search.syear) : now.getFullYear();
				var month = $scope.search.smon ?
						parseInt($scope.search.smon) - 1 : now.getMonth();
				var day = $scope.search.sday ?
						parseInt($scope.search.sday) : now.getDate();
				var hour = $scope.search.shour ?
						parseInt($scope.search.shour) : 0;
				var min = $scope.search.smin ?
						parseInt($scope.search.smin) : 0;
				var sec = $scope.search.ssec ?
						parseInt($scope.search.ssec) : 0;
				var start = new Date(year, month, day, hour, min, sec);
				$scope.params.t1 = start.getTime() / 1000;
			}
			if ($scope.search.eyear || $scope.search.emon ||
					$scope.search.eday || $scope.search.ehour ||
					$scope.search.emin || $scope.search.esec) {
				var year = $scope.search.eyear ?
						parseInt($scope.search.eyear) : now.getFullYear();
				var month = $scope.search.emon ?
						parseInt($scope.search.emon) - 1 : now.getMonth();
				var day = $scope.search.eday ?
						parseInt($scope.search.eday) : now.getDate();
				var hour = $scope.search.ehour ?
						parseInt($scope.search.ehour) : 0;
				var min = $scope.search.emin ?
						parseInt($scope.search.emin) : 0;
				var sec = $scope.search.esec ?
						parseInt($scope.search.esec) : 0;
				var end = new Date(year, month, day, hour, min, sec);
				$scope.params.t2 = end.getTime() / 1000;
			}
		}

		// Application state variables
		$scope.reload = 0;

		// Do we have everything necessary to build a trend?
		$scope.canBuildTrend = function() {
			switch ($scope.params.reporttype) {
			case "services":
				if ((($scope.params.timeperiod != "") ||
						(($scope.params.t1 != 0) &&
						($scope.params.t2 != 0)))) {
					return true;
				}
				return false;
				break;
			case "hosts":
				if ((($scope.params.timeperiod != "") ||
						(($scope.params.t1 != 0) &&
						($scope.params.t2 != 0)))) {
					$scope.params.reporttype = "hosts";
					return true;
				}
				return false;
				break;
			default:
				return false;
				break;
			}
		};

		$scope.displayForm = function(size) {
			$scope.formDisplayed = true;
			var modalInstance = $modal.open({
				templateUrl: 'trends-form.html',
				controller: 'trendsFormCtrl',
				size: size,
				resolve: {
					params: function () {
						return $scope.params;
					}
				}
			});

			modalInstance.result.then(function(params) {
				$scope.formDisplayed = false;
				$scope.params = params;
				$scope.reload++;
			},
			function(reason) {
				$scope.formDisplayed = false;
			});
		};

		if(!$scope.canBuildTrend()) {
			$scope.displayForm();
		};

		$scope.infoBoxTitle = function() {
			switch ($scope.params.reporttype) {
			case "hosts":
				return "Host State Trends";
				break;
			case "services":
				return "Service State Trends";
				break;
			default:
				return "Host and Service State Trends";
				break;
			}
		};

	});
