$( document).ready( function() {
	var parts = $(location).attr( 'href').split( '/');
	parts.pop()
	var baseurl = parts.join( '/');
    
    $('#query button').attr('disabled','disabled');

	var disableWhenDependsOnValueBlank = false;

	var help = Object();

	var true_false = {
		"valid_values": [
			"True",
			"False",
		]
	};

	function buildSelect( label, name, option) {
		var optlist = Array();
		optlist.push( $( '<option></option')
			.attr({
				 value: ''
			})
			.text( 'Select a value')
		);
		if( option.valid_values instanceof Array) {
			for( var x = 0; x < option.valid_values.length; x++) {
				optlist.push( $( '<option></option>')
					.attr({
						 value: option.valid_values[ x]
					})
					.text( option.valid_values[ x])
				);
			}
		}
		else if( option.valid_values instanceof Object) {
			for( var key in option.valid_values) {
				var valid_value = option.valid_values[ key];
				optlist.push( $( '<option></option>')
					.attr({
						value: key,
						title: valid_value.description
					})
					.text( key)
				);
			}
		}
		var html = $( '<tr></tr>')
			.addClass( 'generated')
			.attr( 'id', name + 'row').append(
				$( '<td></td>')
					.addClass( 'label')
					.text( label),
				$( '<td></td>').append(
					$( '<select></select>')
						.attr({
							id: name,
							name: name
						})
						.append(
							$( optlist).map( function() { 
								return this.toArray()
							})
						)
				)
			);

		return html;
	}

	function buildCheckbox( label, name, option) {
		var inputs = Array();
		if( option.valid_values instanceof Array) {
			for( var x = 0; x < option.valid_values.length; x++) {
				inputs.push( $('<input/>')
					.attr({ 
						type: 'checkbox', 
						name: name, 
						value: option.valid_values[ x]
					})
				);
				inputs.push( $( '<span></span>').text( option.valid_values[ x]));
				inputs.push( $( '<br/>'));
			}
		}
		else if( option.valid_values instanceof Object) {
			for( var key in option.valid_values) {
				var valid_value = option.valid_values[ key];
				inputs.push( $('<input/>')
					.attr({
						type: 'checkbox',
						name: name,
						value: key,
						title: valid_value.description
					})
				);
				inputs.push( $( '<span></span>').text( key));
				inputs.push( $( '<br/>'));
			}
		}
		var html = $('<tr></tr>')
			.addClass('generated')
			.attr('id', name + 'row')
			.append( 
				$('<td></td>').addClass( 'label').text( label),
				$('<td></td>').append( 
					$( inputs).map( function() { 
						return this.toArray()
					})
				)
			);

		return html;
	}

	function buildRadioButtons( label, name, option) {
		var inputs = Array();
		if( option.valid_values instanceof Array) {
			for( var x = 0; x < option.valid_values.length; x++) {
				inputs.push( $('<input/>')
					.attr({ 
						type: 'radio', 
						name: name, 
						value: option.valid_values[ x]
					})
				);
				inputs.push( $( '<span></span>').text( option.valid_values[ x]));
				inputs.push( $( '<br/>'));
			}
		}
		else if( option.valid_values instanceof Object) {
			for( var key in option.valid_values) {
				inputs.push( $('<input/>')
					.attr({
						type: 'radio',
						name: name,
						value: key
					})
				);
				inputs.push( $( '<span></span>').text( key));
				inputs.push( $( '<br/>'));
			}
		}
		var html = $('<tr></tr>')
			.addClass('generated')
			.attr('id', name + 'row')
			.append( 
				$('<td></td>').addClass( 'label').text( label),
				$('<td></td>').append( 
					$( inputs).map( function() { 
						return this.toArray()
					})
				)
			);
		return html;
	}

	function buildTextField( label, name, option) {
		var html = $('<tr></tr>')
				.addClass('generated')
				.attr('id', name + 'row')
				.append(
			$('<td></td>').addClass('label').text( label),
			$('<td></td>').append( 
				$('<input/>').attr({ 
					type: "text", 
					name: name, 
					title: option.description
				})
			)
		);
		return html;
	}

	function isRequired( option, query) {
		var required = false;
		if( option.hasOwnProperty( 'required') && 
				( option.required.length > 0)) {
			if( option.required[ 0] == "all") {
				required = true;
			}
			else {
				for( var x = 0; x < option.required.length; x++) {
					if( option.required[ x] == query) {
						required = true;
					}
				}
			}
		}
		return required;
	}

	function isOptional( option, query) {
		var optional = false;
		if( option.hasOwnProperty( 'optional') && 
				( option.optional.length > 0)) {
			if( option.optional[ 0] == "all") {
				optional = true;
			}
			else {
				for( var x = 0; x < option.optional.length; x++) {
					if( option.optional[ x] == query) {
						optional = true;
					}
				}
			}
		}
		return optional;
	}

	function getNagiosObject( key, option) {
		var queryspec = option.type.substr( "nagios:".length);
		var slash = queryspec.search( "/");
		var cgi = queryspec.substr( 0, slash);
		var query = queryspec.substr( slash+1);
		var success = function(data, results) {
			var nagobjs = {};
			nagobjs[ 'valid_values'] = new Object();
			for( var vvkey in option.valid_values) {
				nagobjs.valid_values[ vvkey] = option.valid_values[ vvkey];
			}
			if( query == "servicelist") {
				// Special case because services are nested under hosts
				var services = new Array();
				for( var hostname in data.data[ query]) {
					var host = data.data[ query][ hostname];
					for( var x = 0; x < host.length; x++) {
						if( services.indexOf( host[ x]) == -1) {
							services.push( host[ x]);
						}
					}
				}
				services.sort();
				for( var x = 0; x < services.length; x++) {
					nagobjs.valid_values[ services[ x]] = {};
				}
			}
			else {
				for( var x = 0; x < data.data[ query].length; x++) {
					nagobjs.valid_values[ data.data[ query][ x]] = new Object();
					nagobjs.valid_values[ data.data[ query][ x]][ 'description'] = data.data[ query][ x];
				}
			}
			var selector = '#cgiform #' + key + 'row';
			$(selector).replaceWith( 
				buildSelect( option.label, key, nagobjs)
			);
		};
		var parameters = Object();
		parameters.query = query;
		if( option.depends_on != "") {
			var dependsOnValue = getValue( option.depends_on,
					help.data.options[ option.depends_on]);
			if( dependsOnValue != "") {
				parameters[ option.depends_on] = dependsOnValue;
			}
		}
		$.ajax({
			url: baseurl + '/cgi-bin/' + cgi + '.cgi',
			data: parameters,
			success: success,
			async: false
		});
	}

	function updateOption(query, key, option) {
		var required = isRequired( option, query);
		var optional = isOptional( option, query);
		var row = '#' + key + 'row'; // row selector
		// If either optional or required, show the parameter
		if( required || optional) {
			// If the parameter is Nagios object, fetch it
			if( /^nagios:/.test( option.type)) {
				if( option.depends_on == "") {
					getNagiosObject( key, option);
				}
				else {
					var dependsOnValue = getValue( option.depends_on, 
							help.data.options[ option.depends_on]);
					if(( dependsOnValue == "") && 
							disableWhenDependsOnValueBlank) {
						$(row + ' select').attr( "disabled", "disabled");
					}
					else {
						getNagiosObject( key, option);
						$(row + ' select').removeAttr( "disabled");
					}
					$('select#' + option.depends_on).change(function() {
						var query = getValue( 'query', help.data.options.query);
						for( var key in help.data.options) {
							var option = help.data.options[ key];
							if( option.depends_on === $(this).attr( "id")) {
								updateOption(query, key, option);
							}
						}
					});
				}
			}
			$( row).show();
		}
		else {
			$( row).hide();
		}
		// If required, highlight the label
		if( required) {
			$( row + ' td.label').addClass( 'required');
		}
		else {
			$( row + ' td.label').removeClass( 'required');
		}
	}

	function updateForm(query) {
		$('#query button').attr('disabled','disabled');
        for( var key in help.data.options ) {
			updateOption(query, key, help.data.options[ key]);
		}
	}

	function getValue( name, option) {
		switch( option.type) {
			case "enumeration":
				return $( '#' + name + 'row select option:selected').val();
				break;
			case "list":
				output = Array();
				if( option.valid_values instanceof Array) {
					for( var x = 0; x < option.valid_values.length; x++) {
						if( $( '#' + name + 'row input[value=' + 
								valid_values[ x] + ']').attr( 'checked') == 
								'checked') {
							output.push( valid_values[ x]);
						}
					}
				}
				else if( option.valid_values instanceof Object) {
					for( var key in option.valid_values) {
						if( $( '#' + name + 'row input[value=' + key + 
								']').attr( 'checked') == 'checked') {
							output.push( key);
						};
					}
				}
				return output;
				break;
			case "integer":
			case "string":
				return $( '#' + name + 'row input').val();
				break;
			case "boolean":
				if( $( '#' + name + 'row input:radio[name=' + name + 
						']:checked').val() == "True") {
					return true;
				}
				else {
					return false;
				}
				break;
			default:
				if( /^nagios:/.test( option.type)) {
					return $( '#' + name + 'row select option:selected').val();
				}
				break;
		}
	}

	function buildForm() {
		$('#cgiform tr.generated').remove();
		$('#cgiform').append( buildSelect( 'Query', 'query', 
				help.data.options.query));
		$('#queryrow td.label').addClass( 'required');
		for( var key in help.data.options ) {
			if( key != "query") {
				var option = help.data.options[ key]
				switch( option.type) {
				case "enumeration":
					$('#cgiform').append( buildSelect( option.label, key, 
							option));
					break;
				case "list":
					$('#cgiform').append( buildCheckbox( option.label, key, 
							option));
					break;
				case "integer":
				case "string":
					$('#cgiform').append( buildTextField( option.label, key, 
							option));
					break;
				case "boolean":
					$('#cgiform').append( buildRadioButtons( option.label, key, 
							true_false));
					break;
				default:
					if( /^nagios:/.test( option.type)) {
						var nagobjs = {
							"valid_values": [
							]
						};
						$('#cgiform').append( buildSelect( option.label, 
								key, nagobjs));
					}
					else {
						$('#cgiform').append( '<tr><td colspan="2">' + option.type + ': ' + key + '</td></tr>');
					}
					break;
				}
				if( !(( option.hasOwnProperty( 'required') && 
						( option.required.length > 0) && 
						( option.required[ 0] === "all")) ||
						( option.hasOwnProperty( 'optional') && 
						( option.optional.length > 0) &&
						( option.optional[ 0] === "all")))) {
					$('#' + key + 'row').hide();
				}
			}
		}

		$('select#query').change(function() {
			var query = $('select#query').val();
			updateForm(query);
            $('#query button').removeAttr('disabled');
		});
        $('#query button').removeAttr('disabled');
	}

	$('select#cginame').change(function() {
		var cginame = $('select#cginame').val();
		if( cginame == 'none') {
			$('#cgiform tr.generated').remove();
            $('#query button').attr('disabled','disabled');
		}
		else {
			$.get( baseurl + '/cgi-bin/' + cginame, 
					{ query: "help" }, function(data, results) {
				help = data;
				buildForm(data);
			});
		}
	});

	function indentf(padding, text) {
		var result = "";
		for( var padvar = 0; padvar < padding; padvar++) { 
			result = result.concat( "&nbsp;&nbsp;&nbsp;&nbsp;");
		}
		result = result.concat( text);
		return result;
	}

	function arrayToString( padding, arr) {
		var result = "";
		var members = Array();

		result = result.concat( "[<br/>");
		padding++;
		for( var x = 0; x < arr.length; x++) {
			members.push( memberToString( padding, null, arr[ x]));
		}
		result = result.concat( members.join( ",<br/>") + "<br/>");
		padding--;
		result = result.concat( indentf(padding, "]"));
		return result;
	}

	function isString(o) {
		return typeof o == "string" || 
				(typeof o == "object" && o.constructor === String);
	}

	function memberToString( padding, key, value) {
		var result = "";

		if( key == null) {
			result = result.concat( indentf( padding, ''));
		}
		else {
			result = result.concat( indentf( padding, '"' + key + '": '));
		}

		if( value instanceof Array) {
			result = result.concat( arrayToString( padding, value));
		}
		else if( value instanceof Object) {
			result = result.concat( objectToString( padding, value));
		}
		else if(isString(value)) {
			result = result.concat( '"' + value + '"')
		}
		else {
			result = result.concat( value);
		}

		return result;
	}

	function objectToString( padding, obj) {
		var result = "";
		var members = Array();

		result = result.concat( "{<br/>");
		padding++;
		for( var key in obj) {
			members.push( memberToString( padding, key, obj[ key]));
			}
		result = result.concat( members.join( ",<br/>") + "<br/>");
		padding--;
		result = result.concat( indentf(padding, "}"));
		return result;
	}

	$('#query').submit( function() {
		$('#results').html( "");
		var cginame = $('select#cginame').val();
		if( cginame != 'none') {
			var query = $('select#query').val();
			var parameters = Object();
			for( var key in help.data.options) {
				var option = help.data.options[ key];
				var required = isRequired( option, query);
				var optional = isOptional( option, query);
				if( required || optional) {
					var value = getValue( key, option);
					if(( value == null) || ( value == "")) {
						if( required) {
							$('#results').append( 
								$( '<p></p>').
									addClass( 'error').
									append( option.label + " missing")
							);
						}
					}
					else {
						if( value instanceof Array) {
							parameters[ key] = value.join( ' ');
						}
						else {
							parameters[ key] = value;
						}
					}
				}
			}
			var p = jQuery.param( parameters);
			var url = baseurl + '/cgi-bin/' + cginame + '?' + p
			$('#results').append( 
				$('<p></p>').text( 'URL: ').append(
					$('<a></a>').attr({
						href: url,
						target: "_blank"
					}).text( url)
				)
			);
			$.get( baseurl + '/cgi-bin/' + cginame, 
				parameters, 
				function(data, results) {
					$('#results').append( 
						$('<p></p>').html( objectToString( 0, data))
					);
				}
			);
		}
		return false;
	});
	
	$('select#cginame').val( 'none');
});
