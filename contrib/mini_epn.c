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
	/* char plugin_output[1024]; */
	char buffer[512];
	char tmpfname[32];
	char fname[32];
	char *args[] = {"","0", "", "", NULL };
	FILE *fp;

	char command_line[80];
	int exitstatus;
	int pclose_result;

	if((my_perl=perl_alloc())==NULL){
		snprintf(buffer,sizeof(buffer),"Error: Could not allocate memory for embedded Perl interpreter!\n");
		buffer[sizeof(buffer)-1]='\x0';
		printf("%s\n", buffer);
		exit(1);
	}
	perl_construct(my_perl);
	exitstatus=perl_parse(my_perl,xs_init,2,embedding,NULL);
	if(!exitstatus) {

		exitstatus=perl_run(my_perl);

	        while(printf("Enter file name: ") && fgets(command_line, 80, stdin)) {

			SV *plugin_out_sv ;
			int count = 0 ;

			dSP;


	                /* call the subroutine, passing it the filename as an argument */

			command_line[strlen(command_line) -1] = '\0';

                        strncpy(fname,command_line,strcspn(command_line," "));
                        fname[strcspn(command_line," ")] = '\x0';
                        args[0] = fname ;
                        args[3] = command_line + strlen(fname) + 1 ;

                        args[2] = "";

			/* call our perl interpreter to compile and optionally cache the command */
			count = perl_call_argv("Embed::Persistent::eval_file", G_DISCARD | G_EVAL, args);

			ENTER; 
			SAVETMPS;
			PUSHMARK(SP);
			XPUSHs(sv_2mortal(newSVpv(args[0],0)));
			XPUSHs(sv_2mortal(newSVpv(args[1],0)));
			XPUSHs(sv_2mortal(newSVpv(args[2],0)));
			XPUSHs(sv_2mortal(newSVpv(args[3],0)));
			PUTBACK;
			count = perl_call_pv("Embed::Persistent::run_package", G_EVAL | G_ARRAY);
			SPAGAIN;
			plugin_out_sv = POPs;
			plugin_output = SvPOK(plugin_out_sv) && (SvCUR(plugin_out_sv) > 0) ? savepv(SvPVX(plugin_out_sv)) : savepv("(No output!)\n") ;
			pclose_result = POPi ;
			/* FIXME - no digits returned */
			PUTBACK;
			FREETMPS;
			LEAVE;

			/* check return status  */
			if(SvTRUE(ERRSV)){
				pclose_result=-2;
				printf("embedded perl ran %s with error %s\n",fname,SvPV(ERRSV,PL_na));
				continue;
			}
			
			printf("embedded perl plugin return code and output was: %d & '%s'\n",pclose_result, plugin_output);

		}

	}

	
        PL_perl_destruct_level = 0;
        perl_destruct(my_perl);
        perl_free(my_perl);
        exit(exitstatus);
}
