angular.module("mapApp")
	.controller("mapFormCtrl", function($scope, $modalInstance, $http,
			nagiosProcessName, params) {

		$scope.params = params;
		$scope.hostlist = [];
		$scope.nodelist = [];

		$scope.apply = function () {
			$modalInstance.close($scope.params);
		};

		$scope.cancel = function () {
			$modalInstance.dismiss('cancel');
		};

		$scope.showDimensions = function() {
			return $scope.params.layout == 0;
		};

		$scope.showCoordinates = function() {
			return $scope.params.layout == 0 &&
					$scope.params.dimensions == "user";
		};

		$scope.showTimeperiod = function() {
			switch ($scope.params.reporttype) {
			case "":
				return false;
				break;
			case "hosts":
				return $scope.params.host != "";
				break;
			case "services":
				return $scope.params.host != "" &&
						$scope.params.service != "";
				break;
			}
		};

		$scope.disableApply = function() {
			return $scope.hostlist.length == 0;
		}

		var getHostList = function() {
			var url = $scope.params.cgiurl +
					"/objectjson.cgi?query=hostlist";
			$http.get(url, { withCredentials: true})
				.success(function(results) {
					$scope.hostlist = results.data.hostlist;
					$scope.nodelist = [];
					$scope.nodelist.push(nagiosProcessName);
					for (var i = 0; i < $scope.hostlist.length; i++) {
						$scope.nodelist.push($scope.hostlist[i]);
					}
				})
				.error(function(err) {
					$scope.nodelist = [];
					console.log(err);
				});
		};

		$scope.onBlurCgiurl = function(evt) {
			getHostList();
		};

		getHostList();
	});
