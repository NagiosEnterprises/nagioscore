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


#include <EXTERN.h>
#include <perl.h>
#include <fcntl.h>
#include <string.h>

#define MAX_COMMANDLINE_LENGTH 160

/* include PERL xs_init code for module and C library support */

#if defined(__cplusplus)
#define is_cplusplus
#endif

#ifdef is_cplusplus
extern "C" {
#endif

#define NO_XSLOCKS
#include <XSUB.h>

#ifdef is_cplusplus
}
#  ifndef EXTERN_C
#    define EXTERN_C extern "C"
#  endif
#else
#  ifndef EXTERN_C
#    define EXTERN_C extern
#  endif
#endif
 
EXTERN_C void xs_init _((void));

EXTERN_C void boot_DynaLoader _((CV* cv));

EXTERN_C void
xs_init(void)
{
        char *file = __FILE__;
        dXSUB_SYS;

        /* DynaLoader is a special case */
        newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

static PerlInterpreter *perl = NULL;


int main(int argc, char **argv, char **env){
        char *embedding[] = { "", "p1.pl" };
        char plugin_output[1024];
        char buffer[512];
        char tmpfname[32];
        char fname[MAX_COMMANDLINE_LENGTH];
        char *args[] = {"","0", "", "", NULL };
        FILE *fp;

        char command_line[MAX_COMMANDLINE_LENGTH];
        char *ap ;
        int exitstatus;
        int pclose_result;
#ifdef THREADEDPERL
	dTHX;
#endif
        dSP; 

        if((perl=perl_alloc())==NULL){
                snprintf(buffer,sizeof(buffer),"Error: Could not allocate memory for embedded Perl interpreter!\n");
                buffer[sizeof(buffer)-1]='\x0';
                printf("%s\n", buffer);
                exit(1);
                }
        perl_construct(perl);
        exitstatus=perl_parse(perl,xs_init,2,embedding,NULL);
        if(!exitstatus){

                exitstatus=perl_run(perl);

                while(printf("Enter file name: ") && fgets(command_line,sizeof(command_line),stdin)){

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

                        perl_call_argv("Embed::Persistent::run_package", G_DISCARD | G_EVAL, args);
                        
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
                        printf("embedded perl plugin output was %d,%s\n",pclose_result, plugin_output);
                        }
                }
        
        PL_perl_destruct_level = 0;
        perl_destruct(perl);
        perl_free(perl);
        exit(exitstatus);
        }
