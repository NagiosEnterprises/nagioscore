/*****************************************************************************
* Filename:    nag_funcs.js
* Description: contains objects and functions used in the Nagios(R) Core(SM)
*              website.
* Requires:    jquery, nag_funcs.css
* Contents:    vidbox
* License:     This program is free software; you can redistribute it and/or
*              modify it under the terms of the GNU General Public License
*              version 2 as published by the Free Software Foundation.
*****************************************************************************/

/* --------------------------------------------------------------------------
* Object:      vidbox
* Description: A video box for demo vids on the core website
* Arguments:   Takes an optional "args" object which can have one or more
*              of the following properties:
*              args.pos - vertical (u=upper,l=lower,c=center) and
*                         horizontal (l=left,r=right,c=center)
* --------------------------------------------------------------------------*/

vidbox = function(args)
{
	this.args = args || {};
	this.tab = null;
	this.box = null;
	this.frame = null;
	this.tab = null;
	this.tabClose = null;
	this.vidbox = null;
	this.txtbox = null;
	this.showing = false;
	this.pos = args.pos || "lr";
	this.vidurl = args.vidurl;
	this.text = args.text;
	this.init();
}

vidbox.prototype.init = function()
{
	var cls1 = "vidboxContainer vidbox_" + this.pos,
		cls2 = "vidboxTab vidboxTab_" + this.pos,
		This = this, embed, txt;

	this.box = $("<div/>", { 'class':cls1 }).appendTo($('body'));

	this.frame = $("<div/>", { 'class':"vidboxFrame" }).appendTo($(this.box));
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

vidbox.prototype.toggleFrame = function()
{
	var	w, This = this;

	if (this.showing) {
		this.showing = false;

		w = this.box.width() * -1;

		if (this.pos.substr(1,1) == 'l') {
			$(this.box).animate( { "left":w }, "slow", function(){
				$(This.frame).css("display", "none");
				$(This.tabClose).css("display","");
				$(This.tab).css("position", "").css("margin-left","");
				$(This.tab).addClass("vidboxTab_" + This.pos);
				$('body').append($(This.tab).detach());
			} );

		} else if (this.pos.substr(1,1) == 'r') {
			$(this.box).animate( { "right":w }, "slow", function(){
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
