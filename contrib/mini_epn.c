/*
	mini_epn.c

*/

#include <EXTERN.h>
#include <perl.h>
#include "epn_nagios.h"

static PerlInterpreter *my_perl = NULL;

int main(int argc, char **argv, char **env) {

/*
#ifdef aTHX
	dTHX;
#endif
*/

	char *embedding[] = { "", "p1.pl" };
	char *plugin_output ;
	char fname[64];
	char *args[] = {"","0", "", "", NULL };
	char command_line[80];
	int exitstatus;
	int pclose_result;

	if((my_perl=perl_alloc())==NULL){
		printf("%s\n","Error: Could not allocate memory for embedded Perl interpreter!"); 
		exit(1);
	}
	perl_construct(my_perl);
	exitstatus=perl_parse(my_perl,xs_init,2,embedding,NULL);
	if(!exitstatus) {

		exitstatus=perl_run(my_perl);

	        while(printf("Enter file name: ") && fgets(command_line, 80, stdin)) {
			SV *plugin_hndlr_cr;
		        STRLEN n_a;
			int count = 0 ;

			dSP;

			command_line[strlen(command_line) -1] = '\0';

                        strncpy(fname,command_line,strcspn(command_line," "));
                        fname[strcspn(command_line," ")] = '\x0';
                        args[0] = fname ;
                        args[3] = command_line + strlen(fname) + 1 ;

                        args[2] = "";

			/* call our perl interpreter to compile and optionally cache the command */

			ENTER; 
			SAVETMPS;
			PUSHMARK(SP);

			XPUSHs(sv_2mortal(newSVpv(args[0],0)));
			XPUSHs(sv_2mortal(newSVpv(args[1],0)));
			XPUSHs(sv_2mortal(newSVpv(args[2],0)));
			XPUSHs(sv_2mortal(newSVpv(args[3],0)));

			PUTBACK;

			count = call_pv("Embed::Persistent::eval_file", G_SCALAR | G_EVAL);

			SPAGAIN;

			/* check return status  */
			if(SvTRUE(ERRSV)){
				(void) POPs;

				pclose_result=-2;
				printf("embedded perl ran %s with error %s\n",fname,SvPVX(ERRSV));
				continue;
			} else {
		                plugin_hndlr_cr = newSVsv(POPs);         

	               		PUTBACK;
                		FREETMPS;
                		LEAVE;
        		}

			ENTER; 
			SAVETMPS;
			PUSHMARK(SP);

			XPUSHs(sv_2mortal(newSVpv(args[0],0)));
			XPUSHs(sv_2mortal(newSVpv(args[1],0)));
			XPUSHs(plugin_hndlr_cr);
			XPUSHs(sv_2mortal(newSVpv(args[3],0)));

			PUTBACK;

			count = perl_call_pv("Embed::Persistent::run_package", G_EVAL | G_ARRAY);

			SPAGAIN;

			plugin_output = POPpx ;
			pclose_result = POPi ;

			printf("embedded perl plugin return code and output was: %d & '%s'\n", pclose_result, plugin_output);

			PUTBACK;
			FREETMPS;
			LEAVE;

		}

	}

	
        PL_perl_destruct_level = 0;
        perl_destruct(my_perl);
        perl_free(my_perl);
        exit(exitstatus);
}
