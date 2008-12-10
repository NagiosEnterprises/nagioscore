<?php
// HELPER UTILITIES

require_once(dirname(__FILE__).'/../config.inc.php');

function get_update_information(){
	global $cfg;
	
	// initialize array
	$updateinfo=array(
		"update_checks_enabled" => true,
		"last_update_check" => "",
		"update_available" => false,
		"update_version" => "",
		);
	
	// first read CGI config file to determine main file location
	$ccfc=read_cgi_config_file();
	//print_r($ccfc);
	
	// read main config file to determine status file location
	if(isset($ccf['main_config_file']))
		$mcf=$ccf['main_config_file'];
	else
		$mcf="";
	$mcfc=read_main_config_file($mcf);
	//print_r($mcfc);

	// read status file (just first few lines)
	if(isset($mcf['status_file']))
		$sf=$mcf['status_file'];
	else
		$sf="";
	$sfc=read_status_file($sf,50);
	//print_r($sfc);
	//exit();
	
	///////////////////////////
	// DETERMINE UPDATE INFO
	///////////////////////////
	
	// are update checks enabled?
	if(isset($mcfc['check_for_updates']) && $mcfc['check_for_updates']=="0")
		$updateinfo["update_checks_enabled"]=false;
		
	// last update time
	if(isset($sfc['info']['last_update_check']))
		$updateinfo["last_update_check"]=$sfc['info']['last_update_check'];
	
	// update available
	if(isset($sfc['info']['update_available'])){
		if($sfc['info']['update_available']=="1")
			$updateinfo["update_available"]=true;
		else
			$updateinfo["update_available"]=false;
		}
	
	// update version
	if(isset($sfc['info']['new_version'])){
		$updateinfo["update_version"]=$sfc['info']['new_version'];
		}
		
	//print_r($updateinfo);
	//exit();
	
	return $updateinfo;
	}
	

// reads variables from main config file
function read_main_config_file($thefile=""){
	global $cfg;
	
	$contents=array();
	
	// file name can be overridden from default
	if(isset($thefile) && $thefile!="")
		$fname=$thefile;
	else
		$fname=$cfg['main_config_file'];
		
	// open main config file for reading...
	if(($fh=@fopen($fname,'r'))!=FALSE){
		// read all lines in the config file
		while(!feof($fh)){
			$s=fgets($fh);
			
			// skip comments
			if($s[0]=='#')
				continue;
				
			// skip blank lines
			// TODO - is this necessary?
			
			// split comments out from config
			$s2=explode(";",$s);
				
			// get var/val pairs
			$v=explode("=",$s2[0]);
			
			if(isset($v[0]) && isset($v[1])){

				// trim var/val pairs
				$v[0]=ltrim(rtrim($v[0]));
				$v[1]=ltrim(rtrim($v[1]));

				// allow for multiple values for some variables...
				$arr=false;
				if(!strcmp($v[0],"cfg_file"))
					$arr=true;
				else if(!strcmp($v[0],"cfg_dir"))
					$arr=true;
					
				if($arr==true)
					$contents[$v[0]][] = $v[1];
				else
					$contents[$v[0]] = $v[1];
				}
			}
		fclose($fh);
		}

	return $contents;
	}
	
	
// reads variables from cgi config file
function read_cgi_config_file($thefile=""){
	global $cfg;
	
	$contents=array();
	
	// file name can be overridden from default
	if(isset($thefile) && $thefile!="")
		$fname=$thefile;
	else
		$fname=$cfg['cgi_config_file'];
		
	// open cgi config file for reading...
	if(($fh=@fopen($fname,'r'))!=FALSE){
		// read all lines in the config file
		while(!feof($fh)){
			$s=fgets($fh);
			
			// skip comments
			if($s[0]=='#')
				continue;
				
			// skip blank lines
			// TODO - is this necessary?
			
			// split comments out from config
			$s2=explode(";",$s);
				
			// get var/val pairs
			$v=explode("=",$s2[0]);
			
			if(isset($v[0]) && isset($v[1])){

				// trim var/val pairs
				$v[0]=ltrim(rtrim($v[0]));
				$v[1]=ltrim(rtrim($v[1]));

				// do not allow for multiple values
				$contents[$v[0]] = $v[1];
				}
			}
		fclose($fh);
		}

	return $contents;
	}
	
	

// reads status file
function read_status_file($thefile="",$maxlines=0){
	global $cfg;
	
	$contents=array(
		"info" => array(),
		"programstatus" => array(),
		"hoststatus" => array(),
		"servicestatus" => array(),
		"contactstatus" => array()
		);
		
	$statustype="unknown";
	$current_host=0;
	$current_service=0;
	$current_contact=0;
	
	// file name can be overridden from default
	if(isset($thefile) && $thefile!="")
		$fname=$thefile;
	else
		$fname=$cfg['status_file'];
		
	// open cgi config file for reading...
	$x=0;
	if(($fh=@fopen($fname,'r'))!=FALSE){
		// read all lines in the config file
		while(!feof($fh)){

			$x++;
			if($maxlines>0 && $x>$maxlines)
				break;

			$s=fgets($fh);
			
			// skip comments
			if($s[0]=='#')
				continue;
				
			// trim lines
			$s=ltrim(rtrim($s));
				
			// skip blank lines
			if($s=="")
				continue;
			
			// we are in a new type of status data or new entry
			if(!strcmp($s,"info {")){
				$statustype="info";
				continue;
				}
			else if(!strcmp($s,"programstatus {")){
				$statustype="programstatus";
				continue;
				}
			else if(!strcmp($s,"hoststatus {")){
				$statustype="hoststatus";
				$current_host++;
				continue;
				}
			else if(!strcmp($s,"servicestatus {")){
				$statustype="servicestatus";
				$current_service++;
				continue;
				}
			else if(!strcmp($s,"contactstatus {")){
				$statustype="contactstatus";
				$current_contact++;
				continue;
				}
				
			// we just ended an entry...
			else if(!strcmp($s,"}")){
				$statustype="unknown";
				continue;
				}
				
			// get/split var/val pairs
			$v=explode("=",$s);
			$var="";
			$val="";
			if(isset($v[0]) && isset($v[1])){
				// trim var/val pairs
				$var=ltrim(rtrim($v[0]));
				$val=ltrim(rtrim($v[1]));
				}
				
			// INFO AND PROGRAM STATUS 
			if($statustype=="info" || $statustype=="programstatus"){
				$contents[$statustype][$var]=$val;
				continue;
				}
			// HOST STATUS
			else if($statustype=="hoststatus"){
				$contents[$statustype][$current_host][$var]=$val;
				continue;
				}
			
			// SERVICE STATUS
			else if($statustype=="servicestatus"){
				$contents[$statustype][$current_service][$var]=$val;
				continue;
				}
			
			// CONTACT STATUS
			else if($statustype=="contactstatus"){
				$contents[$statustype][$current_contact][$var]=$val;
				continue;
				}
			}
		fclose($fh);
		}
		
	$contents["total_hosts"]=$current_host;
	$contents["total_services"]=$current_service;
	$contents["total_contacts"]=$current_contact;

	return $contents;
	}
?>