/* 

   MINI_EPN.C - Mini Embedded Perl Nagios
   Contributed by Stanley Hopcroft
   Last Modified: 05/09/2002

   This is a sample mini embedded Perl interpreter (hacked out checks.c and perlembed) for use in testing Perl plugins. 

   It can be compiled with the following command (see 'man perlembed' for more info):

   gcc -omini_epn mini_epn.c `perl -MExtUtils::Embed -e ccopts -e ldopts`

   NOTES:  The compiled binary needs to be in the same directory as the p1.pl file supplied with Nagios (or vice versa)

   HISTORY:
   05/09/02 - Command line length increase (Douglas Warner)

*/

#include <string.h>
#include <fcntl.h>
#include "mini_epn.h"

#define MAX_COMMANDLINE_LENGTH 160

static PerlInterpreter *my_perl = NULL;


int main(int argc, char **argv, char **env){
        char *embedding[] = { "", "p1.pl" };
        char plugin_output[1024];
        char buffer[512];
        char tmpfname[32];
        char fname[MAX_COMMANDLINE_LENGTH];
        char *args[] = {"", "0", "", "", NULL };
        FILE *fp;

        char command_line[MAX_COMMANDLINE_LENGTH];
        char *ap ;
        int exitstatus;
        int pclose_result;
        int i;

        if((my_perl=perl_alloc())==NULL){
                snprintf(buffer,sizeof(buffer),"Error: Could not allocate memory for embedded Perl interpreter!\n");
                buffer[sizeof(buffer)-1]='\x0';
                printf("%s\n", buffer);
                exit(1);
                }
        perl_construct(my_perl);
        exitstatus=perl_parse(my_perl,xs_init,2,embedding,NULL);
        if(!exitstatus){

                exitstatus=perl_run(my_perl);

		while(printf("Enter file name: ") && fgets(command_line,sizeof(command_line),stdin)){

#ifdef aTHX
			dTHX;
#endif
        		dSP; 


                        /* call the subroutine, passing it the filename as an argument */

                        command_line[strlen(command_line) -1] = '\0';

			strncpy(fname,command_line,strcspn(command_line," "));
                        fname[strcspn(command_line," ")] = '\x0';
                        args[0] = fname ;
                        args[3] = command_line + strlen(fname) + 1 ;

                        /* generate a temporary filename to which stdout can be redirected. */
                        sprintf(tmpfname,"/tmp/embedded%d",getpid());
                        args[2] = tmpfname;

                        /* call our perl interpreter to compile and optionally cache the command */
                        perl_call_argv("Embed::Persistent::eval_file", G_DISCARD | G_EVAL, args);


			ENTER; 
			SAVETMPS;
			PUSHMARK(SP);
			for (i = 0; args[i] != NULL; i++) {
				XPUSHs(sv_2mortal(newSVpv(args[i],0)));
			}
			PUTBACK;
			perl_call_pv("Embed::Persistent::run_package", G_EVAL);
			SPAGAIN;
			pclose_result=POPi;
			PUTBACK;
			FREETMPS;
			LEAVE;


                        /* check return status  */
                        if(SvTRUE(ERRSV)){
                                pclose_result=-2;
                                printf("embedded perl ran %s with error %s\n",fname,SvPV(ERRSV,PL_na));
                        }
                        
                        /* read back stdout from script */
                        fp=fopen(tmpfname, "r");
                        
                        /* default return string in case nothing was returned */
                        strcpy(plugin_output,"(No output!)");
                        
                        fgets(plugin_output,sizeof(plugin_output)-1,fp);
                        plugin_output[sizeof(plugin_output)-1]='\x0';
                        fclose(fp);
                        unlink(tmpfname);    
                        printf("embedded perl plugin return code: %d. plugin output: %s\n",pclose_result, plugin_output);
		}
	}
        
        PL_perl_destruct_level = 0;
        perl_destruct(my_perl);
        perl_free(my_perl);
        exit(exitstatus);
}
