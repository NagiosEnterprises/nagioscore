#include <EXTERN.h>
#include <perl.h>

EXTERN_C void xs_init (pTHXo);

EXTERN_C void boot_DynaLoader (pTHXo_ CV* cv);

EXTERN_C void
xs_init(pTHXo)
{
	char *file = __FILE__;
	dXSUB_SYS;

	/* DynaLoader is a special case */
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}
