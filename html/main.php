<?php
include_once(dirname(__FILE__).'/includes/utils.inc.php');

?>


<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">

<HTML>

<HEAD>
<META NAME="ROBOTS" CONTENT="NOINDEX, NOFOLLOW">
<TITLE>Nagios</TITLE>
<LINK REL='stylesheet' TYPE='text/css' HREF='stylesheets/common.css'>
</HEAD>

<BODY id="splashpage">


<div id="mainbrandsplash">
<div id="mainlogo">
<a href="http://www.nagios.org/" target="_blank"><img src="images/logofullsize.png" border="0" alt="Nagios" title="Nagios"></a>
</div>
</div>

<div id="currentversioninfo">
<div class="product">Nagios<sup><span style="font-size: small;">&reg;</span></sup> Core<sup><span style="font-size: small;">&trade;</span></sup></div>
<div class="version">Version 3.2.0</div>
<div class="releasedate">August 12, 2009</div>
<div class="checkforupdates"><a href="http://www.nagios.org/checkforupdates/?version=3.2.0&product=nagioscore" target="_blank">Check for updates</a></div>
<div class="whatsnew"><a href="docs/whatsnew.html">Read what's new in Nagios Core 3</a></div>
</div>


<div id="updateversioninfo">
<?php
	$updateinfo=get_update_information();
	//print_r($updateinfo);
	//$updateinfo['update_checks_enabled']=false;
	//$updateinfo['update_available']=true;
	if($updateinfo['update_checks_enabled']==false){
?>
		<div class="updatechecksdisabled">
		<div class="warningmessage">Warning: Automatic Update Checks are Disabled!</div>
		<div class="submessage">Disabling update checks presents a possible security risk.  Visit <a href="http://www.nagios.org/" target="_blank">nagios.org</a> to check for updates manually or enable update checks in your Nagios config file.</a></div>
		</div>
<?php
		}
	else if($updateinfo['update_available']==true){
?>
		<div class="updateavailable">
		<div class="updatemessage">A new version of Nagios is available!</div>
		<div class="submessage">Visit <a href="http://www.nagios.org/" target="_blank">nagios.org</a> to download Nagios <?php echo $updateinfo['update_version'];?>.</div>
		</div>
<?php
		}
?>
</div>


<div id="mainfooter">
<div id="maincopy">Copyright &copy; 2009 Nagios Core Development Team and Community Contributors.<br>Copyright &copy; 1999-2009 Ethan Galstad.<br>See the THANKS file for more information on contributors.</div>
<div CLASS="disclaimer">
Nagios Core is licensed under the GNU General Public License and is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.  Nagios, Nagios Core and the Nagios logo are trademarks, servicemarks, registered trademarks or registered servicemarks owned by Nagios Enterprises, LLC.  Usage of the Nagios marks are governed by our <A href="http://www.nagios.org/legal/trademarkpolicy/">trademark policy</a>.
</div>
<div class="logos">
<a href="http://www.nagios.com/" target="_blank"><img src="images/NagiosEnterprises-whitebg-112x46.png" width="112" height="46" border="0" style="padding: 0 20px 0 0;" title="Nagios Enterprises"></a>  

<a href="http://www.nagios.org/" target="_blank"><img src="images/weblogo1.png" width="102" height="47" border="0" style="padding: 0 40px 0 40px;" title="Nagios.org"></a>

<a href="http://sourceforge.net/projects/nagios"><img src="images/sflogo.png" width="88" height="31" border="0" alt="SourceForge.net Logo" /></a>
</div>
</div> 


</BODY>
</HTML>

