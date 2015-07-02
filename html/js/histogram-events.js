angular.module("histogramEvents", [])

	.service("histogramEventsService", function() {
		// Raw events values
		this.hostUp = 1;
		this.hostDown = 2;
		this.hostUnreachable = 4;
		this.serviceOk = 8;
		this.serviceWarning = 16;
		this.serviceUnknown = 32;
		this.serviceCritical = 64;

		// Calculated events values
		this.hostProblems = this.hostDown + this.hostUnreachable;
		this.hostAll = this.hostUp + this.hostProblems;
		this.serviceProblems = this.serviceWarning +
				this.serviceUnknown + this.serviceCritical;
		this.serviceAll = this.serviceOK + this.serviceProblems;

		return {
			// Host events list
			hostEvents: [
				{
					value: this.hostAll,
					label: "All host events",
					states: "up down unreachable"
				},
				{
					value: this.hostProblems,
					label: "Host problem events",
					states: "down unreachable"
				},
				{
					value: this.hostUp,
					label: "Host up events",
					states: "up"
				},
				{
					value: this.hostDown,
					label: "Host down events",
					states: "down"
				},
				{
					value: this.hostUnreachable,
					label: "Host unreachable events",
					states: "unreachable"
				}
			],

			// Service events list
			serviceEvents: [
				{
					value: this.serviceAll,
					label: "All service events",
					states: "ok warning unknown critical"
				},
				{
					value: this.serviceProblems,
					label: "Service problem events",
					states: "warning unknown critical"
				},
				{
					value: this.serviceOk,
					label: "Service ok events",
					states: "ok"
				},
				{
					value: this.serviceWarning,
					label: "Service warning events",
					states: "warning"
				},
				{
					value: this.serviceUnknown,
					label: "Service unknown events",
					states: "unknown"
				},
				{
					value: this.serviceCritical,
					label: "Service critical events",
					states: "critical"
				},
			]
		};
	});

