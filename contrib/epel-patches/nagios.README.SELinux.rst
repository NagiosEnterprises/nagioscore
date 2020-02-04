================
Nagios and SELinux
================

While there is an Nagios policy in the default Selinux policies, it does
not meet the needs of the current Nagios software. In working with the
SELinux security group, there is now a need for non-core packages to
carry their own policy in a spec file. 

Following the steps in
https://fedoraproject.org/wiki/SELinux/IndependentPolicy we are adding
the needed subpackage and files.

This policy DOES NOT REPLACE THE CORE POLICY in the selinux-policies
package. This is only a supplement that the nrpe package needs due to
changes from the older base policy.

Please report bugs as needed and we will try to get them fixed as soon
as possible.
