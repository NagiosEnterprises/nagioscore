/*-
 * Copyright (c) 2004 Nik Clayton
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>

#include "tap.h"

int
main(int argc, char *argv[])
{
	unsigned int rc = 0;
	unsigned int side_effect = 0;

	rc = plan_tests(4);
	diag("Returned: %d", rc);

	rc = ok(1 == 1, "1 equals 1");	/* Should always work */
	diag("Returned: %d", rc);

	do {
		if(1) {
			rc = skip(1, "Testing skipping");
			continue;
		}
		
		side_effect++;

		ok(side_effect == 1, "side_effect checked out");

	} while(0);

	diag("Returned: %d", rc);

	skip_start(1 == 1, 1, "Testing skipping #2");

	side_effect++;
	rc = ok(side_effect == 1, "side_effect checked out");
	diag("Returned: %d", rc);

	skip_end;

	rc = ok(side_effect == 0, "side_effect is %d", side_effect);
	diag("Returned: %d", rc);

	return exit_status();
}
