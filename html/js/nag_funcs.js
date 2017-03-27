/*****************************************************************************
* Filename:    nag_funcs.js
* Description: contains objects and functions used in the Nagios(R) Core(SM)
*              website.
* Requires:    jquery, nag_funcs.css
* Contents:    oreo, vidbox
* License:     This program is free software; you can redistribute it and/or
*              modify it under the terms of the GNU General Public License
*              version 2 as published by the Free Software Foundation.
*****************************************************************************/

/* --------------------------------------------------------------------------
* Object:      oreo
* Description: An object to get and set cookies
* Arguments:   optional parameters:
*              { domain:"mydomain.com", path:"some/path" }
* --------------------------------------------------------------------------*/
oreo = function(parms)
{
	this.parms = parms || {};
	this.cookies = {};
	this.init();
}

/* --------------------------------------------------------------------------
* Object:      oreo
* Function:    init
* Description: Gets and stores current document cookies
* Arguments:   none
* --------------------------------------------------------------------------*/
oreo.prototype.init = function()
{
	var i, cookie, name, value, list = new Array();
	list = document.cookie.split(";");

	i = list.length;
	while (i--) {
		cookie = list[i];
		cookie = cookie.split("=");
		name = decodeURIComponent(cookie[0].replace(/^ (.*)/, "$1"));
		value = decodeURIComponent(cookie[1]);
		this.cookies[name] = value;
	}
}

/* --------------------------------------------------------------------------
* Object:      oreo
* Function:    get
* Description: Gets the value of a cookie
* Arguments:   Name of the cookie
* Returns:     The value of the cookie, null
* --------------------------------------------------------------------------*/
oreo.prototype.get = function(name)
{
	if (!name || !this.cookies[name])
		return null;
	if (typeof this.cookies[name] == "undefined")
		return null;
	return this.cookies[name];
}

/* --------------------------------------------------------------------------
* Object:      oreo
* Function:    set
* Description: Sets a cookie and adds or removes it from this.cookies
* Arguments:   { name:"name", value:"value" [, expires:"exp-value",
*                domain:"domain.com", path:"some/path"] }
* Notes:       parms.name and parms.value are required. The other parms.xxx
*              entries are optional.
*
*              parms.expires must be a number as follows:
*                   -1      = Never expire
*                   0       = Expire it now
*                   1 - ??? = Number of seconds until it expires
*
*              Passing conflicting arguments for value and expires:
*                   Value         Expires    Result
*                   ------------- ---------- --------------------------
*                   "some string" 0          Expire / remove it
*                   ""            -1 or > 0  Sets an empty cookie
*                   ""            unset      Sets an empty cookie
*                   null          0          Expire / remove it
*                   null          unset      Expire / remove it
*                   null          -1 or > 0  Sets an empty cookie
*                   unset         any        Expire / remove it
*
*              parms.domain and/or parms.path will override domain and/or
*              path in the constructor.
*
* Returns:     Nothing
* --------------------------------------------------------------------------*/
oreo.prototype.set = function(parms)
{
	var cookie, name, value, exp, dom, path, dt = null;

	if (!parms || !parms.name)
		return;

	name = parms.name;
	value = parms.value;
	exp = parms.expires;
	dom = parms.domain || this.parms.domain || null;
	path = parms.path || this.parms.path || null;

	if (typeof value == "undefined") {
		value = "";
		exp = 0;
	} else if (value == null) {
		if (typeof exp != "number" || (typeof exp == "number" && exp == 0)) {
			exp = 0;
			value = "";
		}
	} else if (typeof exp == "number" && exp == 0) {
		value == "";
	}

	if (typeof exp == "number") {
		dt = new Date();
		if (exp < 0)
			dt.setTime(dt.getTime() + 315360000 * 1000);
		else if (exp > 0)
			dt.setTime(dt.getTime() + exp * 1000);
		else
			dt.setTime(0);

		dt = dt.toUTCString();
	}

	if (exp == 0)
		delete this.cookies[name];
	else
		this.cookies[name] = value;
	cookie = encodeURIComponent(name) + "=" + encodeURIComponent(value);
	if (dt)
		cookie += "; expires=" + dt;
	if (dom)
		cookie += "; domain=" + encodeURIComponent(dom);
	if (path)
		cookie += "; path=" + encodeURIComponent(path);

	document.cookie = cookie;
}


/* --------------------------------------------------------------------------
* Object:      vidbox
* Description: A video box for demo vids on the core website
* Arguments:   Takes an optional "args" object which can have one or more
*              of the following properties:
*              args.pos - vertical (u=upper,l=lower,c=center) and
*                         horizontal (l=left,r=right,c=center)
*              args.text - Text to display below the video
*              args.vidid - Video ID for determining if the video box
*                           should not be created
* --------------------------------------------------------------------------*/
vidbox = function(args)
{
	this.args = args || {};
	this.tab = null;
	this.box = null;
	this.frame = null;
	this.cancel = null;
	this.tab = null;
	this.tabClose = null;
	this.vidbox = null;
	this.txtbox = null;
	this.showing = false;
	this.pos = args.pos || "lr";
	this.vidurl = args.vidurl || "";
	this.text = args.text || "";
	this.vidid = args.vidid || null;
	this.cookie = null;
	this.init();
}

vidbox.prototype.init = function()
{
	var cls1 = "vidboxContainer vidbox_" + this.pos,
		cls2 = "vidboxTab vidboxTab_" + this.pos,
		This = this, embed, txt;

	if (this.vidid) {
		this.cookie = new oreo();
		txt = this.cookie.get(this.vidid);
		if (txt == "no")
			return;
	}

	this.box = $("<div/>", { 'class':cls1 }).appendTo($('body'));

	this.frame = $("<div/>", { 'class':"vidboxFrame" }).appendTo($(this.box));
	if (this.vidid) {
		this.cancel = $("<div class=vidboxCancel>Never show this again</div>").
						appendTo($(this.frame));
		$(this.cancel).click(function(){This.cancelBox();});
	}
	this.vidbox = $("<div/>", { 'class':"vidFrame" }).appendTo($(this.frame));
	this.txtbox = $("<div/>", { 'class':"textFrame" }).appendTo($(this.frame));

	this.tab = $("<div/>", { 'class':cls2 }).text("Page Tour").appendTo($('body'));
	$(this.tab).click(function(){This.toggleFrame();});
	this.tabClose = $("<div/>", { class:"vidboxTabClose", text:'x' } );
	this.tabClose.appendTo($(this.tab));

	embed = $("<iframe/>", { 'class':'vidboxIframe', 'width':560,'height':315,
							'src':this.vidurl } );
	this.vidbox.append(embed);
	txt = $("<p/>", { html:this.text } );
	this.txtbox.append(txt);
}

vidbox.prototype.cancelBox = function()
{
	if (this.showing)
		this.toggleFrame(true);
	this.cookie.set( { name:this.vidid, value:"no", expires:-1 } );
}

vidbox.prototype.toggleFrame = function(quit)
{
	var	w, This = this;

	if (this.showing) {
		this.showing = false;

		w = this.box.width() * -1;

		if (this.pos.substr(1,1) == 'l') {
			$(this.box).animate( { "left":w }, "slow", function(){
				if (quit == true) {
					$(This.box).remove();
					return;
				}
				$(This.frame).css("display", "none");
				$(This.tabClose).css("display","");
				$(This.tab).css("position", "").css("margin-left","");
				$(This.tab).addClass("vidboxTab_" + This.pos);
				$('body').append($(This.tab).detach());
			} );

		} else if (this.pos.substr(1,1) == 'r') {
			$(this.box).animate( { "right":w }, "slow", function(){
				if (quit == true) {
					$(This.box).remove();
					return;
				}
				$(This.frame).css("display", "none");
				$(This.tabClose).css("display","");
				$(This.tab).css("position", "").css("margin-left","");
				$(This.tab).addClass("vidboxTab_" + This.pos);
				$('body').append($(This.tab).detach());
			} );
		}

	} else {

		this.showing = true;
		$(this.tab).removeClass("vidboxTab_" + this.pos);
		$(this.tab).css("position", "static").css("margin-left", "10px");
		$(this.tabClose).css("display","inline-block");
		$(this.box).prepend($(this.tab).detach());

		$(this.frame).css("display", "block");
		w = this.box.width() * -1;

		if (this.pos.substr(1,1) == 'l') {
			$(this.box).css( { "left":w+"px" });
			$(this.box).animate( { "left":"10px" }, "slow"  );
		} else if (this.pos.substr(1,1) == 'r') {
			$(this.box).css( { "right":w+"px" });
			$(this.box).animate( { "right":"10px" }, "slow"  );
		}
	}
}
