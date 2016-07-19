angular.module("mapApp", ["ui.bootstrap", "ui.utils", "nagiosDecorations",
		"nagiosTime"])

	.constant("nagiosProcessName", "Nagios Process")

	// Layout options
	.constant("layouts", {
		UserSupplied: {
			index: 0,
			textAlignment: "below",
			padding: {
				top: 25,
				right: 25,
				bottom: 25,
				left: 25,
			},
			textPadding: {
				above: 4,
				right: 4,
				left: 4,
				below: 11
			},
			dimensionType: "fixed"
		},
		DepthLayers: {
			index: 1,
			topPadding: 100,
			bottomPadding: 100,
			dyText: 4
		},
		CollapsedTree: {
			index: 2,
			topPadding: 100,
			bottomPadding: 100,
			dyText: 4
		},
		BalancedTree: {
			index: 3,
			topPadding: 100,
			bottomPadding: 100,
			dyText: 4
		},
		Circular: {
			index: 4
		},
		CircularMarkup: {
			index: 5,
			radialExponent: 1.2,
			padding: 20,
			textPadding: 4
		},
		CircularBalloon: {
			index: 6,
			outsidePadding: 60,
			rotation: -90,
			textPadding: 4,
			dyText: ".31em"
		},
		BalancedTreeVertical: {
			index: 7,
			leftPadding: 100,
			rightPadding: 100,
			dxText: 4,
			dyText: 3
		},
		CollapsedTreeVertical: {
			index: 8,
			leftPadding: 100,
			rightPadding: 100,
			dxText: 4,
			dyText: 3
		},
		DepthLayersVertical: {
			index: 9,
			leftPadding: 100,
			rightPadding: 100,
			dxText: 4,
			dyText: 3
		},
		Force: {
			index: 10,
			outsidePadding: 60,
			textPadding: 4
		}
	})

	.config(function($locationProvider) {
		$locationProvider.html5Mode({
			enabled: true,
			requireBase: false
		});
	})

	.controller("mapCtrl", function($scope, $location, $modal, $http,
			nagiosTimeService, nagiosProcessName, layouts, $window) {
		$scope.search = $location.search();

		// URL parameters
		$scope.params = {
			cgiurl: $scope.search.cgiurl ? $scope.search.cgiurl :
					$location.absUrl().replace(/map\.php.*$/, "cgi-bin/"),
			layout: map_layout,
			dimensions: $scope.search.dimensions ?
					$scope.search.dimensions : "",
			ulx: $scope.search.ulx ? parseInt($scope.search.ulx) : 0,
			uly: $scope.search.uly ? parseInt($scope.search.uly) : 0,
			lrx: $scope.search.lrx ? parseInt($scope.search.lrx) : 0,
			lry: $scope.search.lry ? parseInt($scope.search.lry) : 0,
			root: $scope.search.root ? $scope.search.root :
					nagiosProcessName,
			maxzoom: $scope.search.maxzoom ?
					parseInt($scope.search.maxzoom) : 10,
			nolinks: $scope.search.nolinks ? true : false,
			notext: $scope.search.notext ? true : false,
			nopopups: $scope.search.nopopups ? true : false,
			nomenu: $scope.search.nomenu ? true : false,
			noresize: $scope.search.noresize ? true : false,
			noicons: $scope.search.noicons ? true : false,
			iconurl: $scope.search.iconurl ? $scope.search.iconurl :
					$location.absUrl().replace(/map\.php.*$/, "images/logos/"),
		};

		var rightPadding = 1;
		var bottomPadding = 4;

		$scope.svgWidth = $window.innerWidth - rightPadding;
		$scope.svgHeight = $window.innerHeight - bottomPadding;

		// Application state variables
		$scope.formDisplayed = false;
		$scope.reload = 0;

		// Decoration-related variables
		$scope.lastUpdate = "none";

		// Determine whether we believe we have sufficient information to
		// build the map. If we don't have a valid URL for the JSON CGIs
		// we won't know that until we try to fetch the list of hosts, so
		// we can't know that now.
		$scope.canBuildMap = function() {
			document.body.className = "";
			if ($scope.params.layout == layouts.UserSupplied.index) {
				switch ($scope.params.dimensions) {
				case "fixed":
				case "auto":
					document.body.className = "hasBgImage";
					return true;
					break;
				case "user":
					if ($scope.params.ulx >= $scope.params.lrx ||
							$scope.params.uly >= $scope.params.lry) {
						return false;
					}
					else {
						document.body.className = "hasBgImage";
						return true;
					}
					break;
				default:
					return false;
					break;
				}
			}
			else {
				return true;
			}
		};

		angular.element($window).bind("resize", function() {
			$scope.svgWidth = $window.innerWidth - rightPadding;
			$scope.svgHeight = $window.innerHeight - bottomPadding;
			$scope.$apply("svgWidth");
			$scope.$apply("svgHeight");
		});

		$scope.displayForm = function(size) {
			$scope.formDisplayed = true;
			var modalInstance = $modal.open({
				templateUrl: 'map-form.html',
				controller: 'mapFormCtrl',
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
		}

		// Style the menu button
		$scope.menuButtonStyle = function() {
			return {
				left: ($scope.svgWidth - 30) + "px",
				top: "5px"
			};
		};

		$scope.infoBoxTitle = function() {
			if ($scope.params.root == nagiosProcessName) {
				return "Network Map for All Hosts";
			}
			else {
				return "Network Map for Host " + $scope.params.root;
			}
		};

		if (!$scope.canBuildMap()) {
			$scope.displayForm();
		}

	})
