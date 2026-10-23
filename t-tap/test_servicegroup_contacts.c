/*****************************************************************************
 *
 * test_servicegroup_contacts.c - Test servicegroup contacts/contactgroups
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
	servicegroup sg;
	contactsmember sg_contact;
	contactgroupsmember sg_contactgroup;
	service svc;
	objectlist sgol;
	contactsmember service_contact;

	plan_tests(10);

	memset(&cntct_direct, 0, sizeof(cntct_direct));
	memset(&cntct_group, 0, sizeof(cntct_group));
	memset(&cntct_other, 0, sizeof(cntct_other));
	memset(&cg, 0, sizeof(cg));
	memset(&cg_member, 0, sizeof(cg_member));
	memset(&sg, 0, sizeof(sg));
	memset(&sg_contact, 0, sizeof(sg_contact));
	memset(&sg_contactgroup, 0, sizeof(sg_contactgroup));
	memset(&svc, 0, sizeof(svc));
	memset(&sgol, 0, sizeof(sgol));
	memset(&service_contact, 0, sizeof(service_contact));

	cntct_direct.name = "sg-direct";
	cntct_group.name = "sg-group-member";
	cntct_other.name = "unrelated";

	cg.group_name = "sg-admins";
	cg_member.contact_name = cntct_group.name;
	cg_member.contact_ptr = &cntct_group;
	cg.members = &cg_member;

	sg.group_name = "web-services";
	sg_contact.contact_name = cntct_direct.name;
	sg_contact.contact_ptr = &cntct_direct;
	sg.contacts = &sg_contact;
	sg_contactgroup.group_name = cg.group_name;
	sg_contactgroup.group_ptr = &cg;
	sg.contact_groups = &sg_contactgroup;

	svc.host_name = "host1";
	svc.description = "svc1";
	sgol.object_ptr = &sg;
	svc.servicegroups_ptr = &sgol;

	ok(is_contact_for_servicegroup(&sg, &cntct_direct) == TRUE,
	   "Direct contact is a contact for the servicegroup");
	ok(is_contact_for_servicegroup(&sg, &cntct_group) == TRUE,
	   "Contactgroup member is a contact for the servicegroup");
	ok(is_contact_for_servicegroup(&sg, &cntct_other) == FALSE,
	   "Unrelated contact is not a contact for the servicegroup");
	ok(is_contactgroup_for_servicegroup(&sg, &cg) == TRUE,
	   "Contactgroup is linked to the servicegroup");

	ok(is_contact_for_service(&svc, &cntct_direct) == TRUE,
	   "Servicegroup direct contact is an effective contact for member service");
	ok(is_contact_for_service(&svc, &cntct_group) == TRUE,
	   "Servicegroup contactgroup member is an effective contact for member service");
	ok(is_contact_for_service(&svc, &cntct_other) == FALSE,
	   "Unrelated contact is not an effective contact for member service");
	ok(is_contactgroup_for_service(&svc, &cg) == TRUE,
	   "Servicegroup contactgroup is an effective contactgroup for member service");

	/* service-local contacts still work alongside servicegroup contacts */
	service_contact.contact_name = cntct_other.name;
	service_contact.contact_ptr = &cntct_other;
	svc.contacts = &service_contact;
	ok(is_contact_for_service(&svc, &cntct_other) == TRUE,
	   "Service-local contact remains a contact for the service");
	ok(is_contact_for_service(&svc, &cntct_direct) == TRUE,
	   "Servicegroup contact still applies when service also has local contacts");

	return exit_status();
	}
