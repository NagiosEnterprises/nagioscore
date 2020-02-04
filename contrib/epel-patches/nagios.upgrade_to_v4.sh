#!/bin/sh
# this script checks the nagios.cfg file and comments out several lines in accordance with the README.
# You will want to migrate to the Nagios v4 nagios.cfg file as soon as possible to take advantage of new features.


# this is the nagios.cfg file we will modify
nagios_cfg=/etc/nagios/nagios.cfg

tmp1=`mktemp /tmp/nagios.cfg.XXXXXXXX`
cat $nagios_cfg > $tmp1

# search for and replace the check_result_buffer_slots attribute into a temporary file
sed -i --regexp-extended -e "s/^(\s*check_result_buffer_slots\s*=\s*)/# Line Commented out for Nagios v4 Compatibility\n#\1/g" \
	-e 's/^(\s*use_embedded_perl_implicitly\s*=\s*)/# Line Commented out for Nagios v4 Compatibility\n#\1/g' \
	-e 's/^(\s*sleep_time\s*=\s*)/# Line Commented out for Nagios v4 Compatibility\n#\1/g' \
	-e 's/^(\s*p1_file\s*=\s*)/# Line Commented out for Nagios v4 Compatibility\n#\1/g' \
	-e 's/^(\s*external_command_buffer_slots\s*=\s*)/# Line Commented out for Nagios v4 Compatibility\n#\1/g' \
	-e 's/^(\s*enable_embedded_perl\s*=\s*)/# Line Commented out for Nagios v4 Compatibility\n#\1/g' \
	-e 's/^(\s*command_check_interval\s*=\s*)/# Line Commented out for Nagios v4 Compatibility\n#\1/g' \
	-e 's|^(\s*#?query_socket\s*=\s*/var/log/nagios/rw/nagios.qh)|query_socket=/var/spool/nagios/cmd/nagios.qh|' $tmp1

# add query_socket if none exist
grep -q -F 'query_socket' $tmp1 || echo 'query_socket=/var/spool/nagios/cmd/nagios.qh' >> $tmp1

# check the diff
diff_output=`diff -u $nagios_cfg $tmp1`
diff_exit=$?

# Decide whether or not to replace the file
if [ "$diff_exit" = "0" ]; then
   echo "No changes were made to the Nagios Config file: $nagios_cfg"
elif [ "$diff_exit" = "1" ]; then
   echo "The following changes were made to the Nagios Config file: $nagios_cfg"
   echo "previous config has been saved to $nagios_cfg.oldrpm"
   echo "$diff_output"
   
   # since changes were made, move the temp file into place
   cp $nagios_cfg $nagios_cfg.oldrpm
   cat $tmp1 > $nagios_cfg
else
   echo "ERROR: Unexpected exit code from diff. No changes made to file: $nagios_cfg"
fi

rm -f $tmp1

