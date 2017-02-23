angular.module("histogramApp", ["ui.bootstrap", "ui.utils",
		"histogramEvents", "nagiosDecorations", "nagiosTime"])

	// Statistics breakdown list
	.constant("statisticsBreakdown", [
		{ value: "monthly",		label: "Month" },
		{ value: "dayofmonth",	label: "Day of the Month" },
		{ value: "dayofweek",	label: "Day of the Week" },
		{ value: "hourly",		label: "Hour of the Day" }
	])

	.config(function($locationProvider) {
		$locationProvider.html5Mode({
			enabled: true,
			requireBase: false
		})
	})

	.controller("histogramCtrl", function($scope, $location, $modal) {

		// Parameters fonud in the URL
		$scope.search = $location.search();

		// URL parameters
		$scope.params = {
			cgiurl: $scope.search.cgiurl ? $scope.search.cgiurl : 
					$location.absUrl().replace(/histogram\.html.*$/, "cgi-bin/"),
			reporttype: $scope.search.reporttype ?
					$scope.search.reporttype : "",
			host: $scope.search.host ? $scope.search.host : "",
			service: $scope.search.service ?
					$scope.search.service : "",
			timeperiod: $scope.search.timeperiod ?
					$scope.search.timeperiod : "",
			t1: $scope.search.t1 ? $scope.search.t1 : 0,
			t2: $scope.search.t2 ? $scope.search.t2 : 0,
			breakdown: $scope.search.breakdown ? $scope.search.breakdown :
					"dayofmonth",
			graphevents: $scope.search.graphevents ?
					parseInt($scope.search.graphevents) : 0,
			graphstatetypes: $scope.search.graphstatetypes ?
					parseInt($scope.search.graphstatetypes) : 3,
			assumestateretention: $scope.search.assumestateretention ?
					$scope.search.assumestateretention : false,
			initialstateslogged: $scope.search.initialstateslogged ?
					$scope.search.initialstateslogged : false,
			ignorerepeatedstates: $scope.search.ignorerepeatedstates ?
					$scope.search.ignorerepeatedstates : false
		};

		// Reload index - increment to cause nagios-histogram to reload
		$scope.reload = 0;

		// Application state variables
		$scope.formDisplayed = false;

		// Decoration-related variables
		$scope.lastUpdate = "none";

		var notBlank = function(value) {
			return value != undefined && value != "";
		};

		// Do we have everything necessary to build a histogram?
		$scope.canBuildHistogram = function() {
			if ($scope.params.reporttype == "services" ||
					(notBlank($scope.params.host) &&
					notBlank($scope.params.service))) {
				if ((($scope.params.timeperiod != "") ||
						(($scope.params.t1 != 0) &&
						($scope.params.t2 != 0)))) {
					$scope.params.reporttype = "services";
					return true;
				}
				return false;
			}
			else if ($scope.params.reporttype == "hosts" ||
					notBlank($scope.params.host)) {
				if ((($scope.params.timeperiod != "") ||
						(($scope.params.t1 != 0) &&
						($scope.params.t2 != 0)))) {
					$scope.params.reporttype = "hosts";
					return true;
				}
				return false;
			}
			return false;
		};

		$scope.displayForm = function(size) {
			$scope.formDisplayed = true;
			var modalInstance = $modal.open({
				templateUrl: 'histogram-form.html',
				controller: 'histogramFormCtrl',
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

		if(!$scope.canBuildHistogram()) {
			$scope.displayForm();
		}

		$scope.infoBoxTitle = function() {
			switch ($scope.params.reporttype) {
			case "hosts":
				return "Host Alert Histogram";
				break;
			case "services":
				return "Service Alert Histogram";
				break;
			default:
				return "Alert Histogram";
				break;
			}
		};

	});
