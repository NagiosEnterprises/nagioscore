/*****************************************************************************
 *
 * test_hostgroup_contacts.c - Test hostgroup contacts/contactgroups
 *
 *****************************************************************************/

#define NSCORE 1

#include "config.h"
#include "nagios.h"
#include "objects.h"
#include "tap.h"

#include "stub_logging.c"
#include "stub_xodtemplate.c"

int debug_level = 0;
int debug_verbosity = 0;
int use_large_installation_tweaks = 0;

int main(int argc, char **argv) {
	contact cntct_direct;
	contact cntct_group;
	contact cntct_other;
	contactgroup cg;
	contactsmember cg_member;
	hostgroup hg;
	contactsmember hg_contact;
	contactgroupsmember hg_contactgroup;
	host hst;
	objectlist hgol;
	contactsmember host_contact;

	plan_tests(10);

	memset(&cntct_direct, 0, sizeof(cntct_direct));
	memset(&cntct_group, 0, sizeof(cntct_group));
	memset(&cntct_other, 0, sizeof(cntct_other));
	memset(&cg, 0, sizeof(cg));
	memset(&cg_member, 0, sizeof(cg_member));
	memset(&hg, 0, sizeof(hg));
	memset(&hg_contact, 0, sizeof(hg_contact));
	memset(&hg_contactgroup, 0, sizeof(hg_contactgroup));
	memset(&hst, 0, sizeof(hst));
	memset(&hgol, 0, sizeof(hgol));
	memset(&host_contact, 0, sizeof(host_contact));

	cntct_direct.name = "hg-direct";
	cntct_group.name = "hg-group-member";
	cntct_other.name = "unrelated";

	cg.group_name = "hg-admins";
	cg_member.contact_name = cntct_group.name;
	cg_member.contact_ptr = &cntct_group;
	cg.members = &cg_member;

	hg.group_name = "linux-servers";
	hg_contact.contact_name = cntct_direct.name;
	hg_contact.contact_ptr = &cntct_direct;
	hg.contacts = &hg_contact;
	hg_contactgroup.group_name = cg.group_name;
	hg_contactgroup.group_ptr = &cg;
	hg.contact_groups = &hg_contactgroup;

	hst.name = "host1";
	hgol.object_ptr = &hg;
	hst.hostgroups_ptr = &hgol;

	ok(is_contact_for_hostgroup(&hg, &cntct_direct) == TRUE,
	   "Direct contact is a contact for the hostgroup");
	ok(is_contact_for_hostgroup(&hg, &cntct_group) == TRUE,
	   "Contactgroup member is a contact for the hostgroup");
	ok(is_contact_for_hostgroup(&hg, &cntct_other) == FALSE,
	   "Unrelated contact is not a contact for the hostgroup");
	ok(is_contactgroup_for_hostgroup(&hg, &cg) == TRUE,
	   "Contactgroup is linked to the hostgroup");

	ok(is_contact_for_host(&hst, &cntct_direct) == TRUE,
	   "Hostgroup direct contact is an effective contact for member host");
	ok(is_contact_for_host(&hst, &cntct_group) == TRUE,
	   "Hostgroup contactgroup member is an effective contact for member host");
	ok(is_contact_for_host(&hst, &cntct_other) == FALSE,
	   "Unrelated contact is not an effective contact for member host");
	ok(is_contactgroup_for_host(&hst, &cg) == TRUE,
	   "Hostgroup contactgroup is an effective contactgroup for member host");

	/* host-local contacts still work alongside hostgroup contacts */
	host_contact.contact_name = cntct_other.name;
	host_contact.contact_ptr = &cntct_other;
	hst.contacts = &host_contact;
	ok(is_contact_for_host(&hst, &cntct_other) == TRUE,
	   "Host-local contact remains a contact for the host");
	ok(is_contact_for_host(&hst, &cntct_direct) == TRUE,
	   "Hostgroup contact still applies when host also has local contacts");

	return exit_status();
	}
