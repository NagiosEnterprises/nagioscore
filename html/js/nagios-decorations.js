angular.module("nagiosDecorations", [])

	.config(function($locationProvider) {
		$locationProvider.html5Mode({
			enabled: true,
			requireBase: false
		})
	})

	.filter("uriComponentEncode", function () {
        return function (component) {
			return encodeURIComponent(component);
		}
	})
	.directive("infoBox", function() {
		return {
			templateUrl: "infobox.html",
			restrict: "AE",
			scope: {
				cgiurl: "@cgiurl",
				decorationTitle: "@decorationTitle",
				updateIntervalValue: "@updateInterval",
				lastUpdate: "=lastUpdate",
				initialState: "@initialState",
				collapsible: "@collapsible",
				includePartial: "@includePartial",
				root: "@root"
			},
			controller: function($scope, $element, $attrs, $http) {

				$scope.updateInterval = parseInt($scope.updateIntervalValue);
				$scope.isPermanentlyCollapsed = $scope.isCollapsed =
						$scope.initialState == "collapsed";
				$scope.isCollapsable = $scope.collapsible == "true";

				$scope.haveProgramStatus = false;

				// Create cgiurl and root node name
				$scope.params = { cgiurl: $scope.cgiurl, root: $scope.root };
				$scope.$watch("root", function() {
					$scope.params.root = $scope.root;
				});

				$scope.$watch("cgiurl", function() {
					getStatus();
				});

				var getStatus = function() {
					var parameters = {
						query: "programstatus"
					};

					var getConfig = {
						params: parameters,
						withCredentials: true
					};

					// Send the AJAX query
					$http.get($scope.cgiurl + "statusjson.cgi", getConfig)
						.error(function(err) {
							console.warn(err);
						})
						.success(function(json) {
							$scope.haveProgramStatus = true;
							$scope.json = json;
						});
				};

				getStatus();

				$scope.notificationsDisabled = function() {
					return $scope.haveProgramStatus &&
							!$scope.json.data.programstatus.enable_notifications;
				};

				$scope.serviceChecksDisabled = function() {
					return $scope.haveProgramStatus &&
							!$scope.json.data.programstatus.execute_service_checks;
				};

				$scope.toggleDisplay = function() {
					$scope.isPermanentlyCollapsed =
							!$scope.isPermanentlyCollapsed;
					if (!$scope.isMousedOver) {
						$scope.isCollapsed = $scope.isPermanentlyCollapsed;
					}
				};

				if ($scope.isCollapsable) {
					$element.on("mouseover", function() {
						$scope.isMousedOver = true;
						$scope.isCollapsed = false;
						$scope.$apply("isCollapsed");
					});

					$element.on("mouseout", function() {
						$scope.isMousedOver = false;
						$scope.isCollapsed = $scope.isPermanentlyCollapsed;
						$scope.$apply("isCollapsed");
					});
				}

			}
		};
	});
