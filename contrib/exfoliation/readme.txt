exfoliation - a skin for nagios
Copyright (c) 2009-2010 Matthew Wall, all rights reserved

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

To use this skin, replace the stock 'images' and 'stylesheets' folders with
those from this tarball.  For example:

  cd /opt/nagios/share
  tar xvfz exfoliation.tgz

On debian/unbuntu installations you might have to modify side.html to include
the common.css stylesheet by adding a line like this in the head section:

  <link rel="stylesheet" href="common.css" type="text/css" />

This skin has been tested with Nagios 3.2.0, 3.2.1, 3.2.2, 3.2.3

Revision History:

0.7 25dec10
  - minor cleanup to statusmap mouseover matrix (nagios bugs still remain)
  - added instructions for installations with side.html rather than side.php
  - added .NavBarTitle, .NavBarItem, and .NavBarSearchItem for side.html in
      ubuntu/debian installations (thanks to michael berman)
  - passiveonly is now simply a question mark
  - fix jpg versions of tacenabled and tacdisabled
  - punt the globe
  - added crude service and event handler icons

0.6 16oct10
  - change enabled/disabled images to x-mark and check-mark
  - new trends icon closer to what trends actually does
  - reduce size of left and right arrow icons

0.5 12feb10
  - changed unreachable from red to orange (to parallel unknown)
  - right-justify filter names
  - eliminate more redundant css
  - use blue to indicate 'problem' (nod to vautour)
  - use purple to indicate indeterminate state
  - make font sizing more consistent in host/service tables
  - make font sizing more consistent in data/log/queue tables
  - another round of cleanup on the icons
  - include nagios base images (in the images/logos directory)

0.4 29jan10
  - lighten the 'dark' color in even/odd listings
  - fix even/odd background colors across pages
  - put border on cells that i missed (notifications, status)
       note: the cgi emits class tags where it should not, so in a few
             places we end up with borders where there should be none.
  - include the basic image set in images/logos
  - tone down heading text color in data tables
  - underline data table headings instead of background color
  - more cleanup of tactical layout and stylesheet
  - fixed queue enabled/disabled border in extinfo listings

0.3 30dec09
  - consolidate colors
  - refactor style sheets for easier editing
  - eliminate (some) redundant stylesheet entries

0.2 27dec09
  - added images for log activity
  - added images for enabled/disabled
  - replace images on tactical page

0.1 27dec09
  - initial rework of color scheme
  - eliminate extraneous borders



TODO:
  - fixes that require changes to CGI/PHP/HTML:
     for extinfo - nested tables with 'command' class
     for controls (state versus action)
     for host summary - nested td with class
     for host listing - statusPENDING should be statusHOSTPENDING
     for service summary - nested td with class
     to eliminate 'view status' and 'view ... status' redundancy
     'host status totals' -> 'host totals'
     'service status totals' -> 'service totals'
     'current network status' -> 'status'
     histogram.cgi contains 'calss' instead of 'class'
     inconsistent table definitions - some use padding/spacing
     remove hard-coded <i> tags
     remove hard-coded center tags
  - tone down the neon green/red in the trend graphs
