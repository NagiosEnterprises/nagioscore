/*
	new_mini_epn.c

*/

#include <EXTERN.h>
#include <perl.h>
#include "epn_nagios.h"

/*
 * DO_CLEAN must be a pointer to a char string
 */

#define DO_CLEAN "0"

static PerlInterpreter *my_perl = NULL;

/*
 *                                                      <=== Align Right
 *                                                      56 spaces from left margin
 *    Do  N  O  T  use  T  A  B  S  in  #define  code.
 *
 *    Indent level == 4 spaces
 */

#define DO_READLINE                                     \
    "$_ = defined($term) "                              \
    "        ? $term->readline($prompt) "               \
    "        : do    { "                                \
    "                    print $prompt; "               \
    "                    chomp($_ = <>); "              \
    "                    $_; "                          \
    "                }; "                               \
    "die qq(That's all folks.\\n) "                     \
    "    unless $_ && ! /^\\s*$/ ; "                    \
    "$_; "

#define INIT_TERM_READLINE                              \
    "use vars qw($term $prompt $OUT); "                 \
                                                        \
    "eval { require Term::ReadLine; }; "                \
    "unless ($@) { "                                    \
    "    $term = new Term::ReadLine 'new_mini_epn'; "   \
    "} else { "                                         \
    "    warn qq(Install Term::ReadLine for arrow key access to history, filename completion etc.); " \
    "} "                                                \
                                                        \
    "$OUT = $term->OUT "                                \
    "    if defined($term); "                           \
    "$prompt = 'plugin command line: '; "

void run_plugin(char *command_line) {

#ifdef aTHX
	dTHX;
#endif

	SV *plugin_hndlr_cr ;
	STRLEN n_a ;
	int count = 0 ;
	int pclose_result;
	char *plugin_output;
	char fname[128];
	char *args[] = {"", "", "", "", NULL };

	dSP;

	strncpy(fname, command_line, strcspn(command_line, " "));
	fname[strcspn(command_line, " ")] = '\0';

	/*
	 * Arguments passsed to Perl sub Embed::Persistent::run_package
	 */

	/*
	 * filename containing plugin
	 */
	args[0] = fname ;
	/*
	 * Do _not_ cache the compiled plugin
	 */
	args[1] = DO_CLEAN ;
	/*
	 * pointer to plugin arguments
	 */

	args[3] = command_line + strlen(fname) + 1 ;

	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	XPUSHs(sv_2mortal(newSVpv(args[0], 0)));
	XPUSHs(sv_2mortal(newSVpv(args[1], 0)));
	XPUSHs(sv_2mortal(newSVpv(args[2], 0)));
	XPUSHs(sv_2mortal(newSVpv(args[3], 0)));

	PUTBACK;

	call_pv("Embed::Persistent::eval_file", G_SCALAR | G_EVAL);

	SPAGAIN ;

	if(SvTRUE(ERRSV)) {
		(void) POPs;

		printf("embedded perl compiled plugin %s with error: %s - skipping plugin\n", fname, SvPVX(ERRSV));
		return;
		}
	else {
		plugin_hndlr_cr = newSVsv(POPs);

		PUTBACK ;
		FREETMPS ;
		LEAVE ;
		}
	/*
	 * Push the arguments to Embed::Persistent::run_package onto
	 * the Perl stack.
	 */
	ENTER;
	SAVETMPS;
	PUSHMARK(SP);

	XPUSHs(sv_2mortal(newSVpv(args[0], 0)));
	XPUSHs(sv_2mortal(newSVpv(args[1], 0)));
	XPUSHs(plugin_hndlr_cr);
	XPUSHs(sv_2mortal(newSVpv(args[3], 0)));

	PUTBACK;

	count = call_pv("Embed::Persistent::run_package", G_ARRAY);

	SPAGAIN;

	plugin_output = POPpx ;
	pclose_result = POPi ;

	printf("embedded perl plugin return code and output was: %d & %s\n", pclose_result, plugin_output) ;

	PUTBACK;
	FREETMPS;
	LEAVE;

	return ;

	}

SV * my_eval_pv(char *pv) {

	SV* result;

	/*
	 * eval_pv(..., TRUE) means die if Perl traps an error
	 */
	result = eval_pv(pv, TRUE) ;
	return result ;
	}

char * get_command_line(void) {

	/* debug
	 * printf("%s\n", INIT_TERM_READLINE) ;
	 */
	SV *cmd_line ;
	char *command_line ;

	cmd_line	= my_eval_pv(DO_READLINE) ;
	command_line	= SvPVX(cmd_line) ;
	/* command_line	= SvPV(cmd_line, n_a) ; */
	return command_line ;
	}

void init_term_readline(void) {
	(void) my_eval_pv(INIT_TERM_READLINE) ;
	}

void init_embedded_perl(void) {
	char *embedding[] = { "", "p1.pl" };
	/* embedding takes the place of argv[] ($argv[0] is the program name.
	 * - which is not given to Perl).
	 * Note that the number of args (ie the number of elements in embedding
	 * [argc] is the third argument of perl_parse().
	 */
	int exitstatus;
	char buffer[132];

	if((my_perl = perl_alloc()) == NULL) {
		snprintf(buffer, sizeof(buffer), "Error: Could not allocate memory for embedded Perl interpreter!\n");
		buffer[sizeof(buffer) - 1] = '\x0';
		printf("%s\n", buffer);
		exit(1);
		}

	perl_construct(my_perl);
	exitstatus = perl_parse(my_perl, xs_init, 2, embedding, NULL);
	PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
	/* Why is perl_run() necessary ?
	 * It is needed if the code parsed by perl_parse() has
	 * any runtime semantics (eg code that gets eval'd,
	 * behaviour that depends on constants etc).
	 */
	exitstatus = perl_run(my_perl);

	if(exitstatus) {
		printf("%s\n", "perl_run() failed.");
		exit(1);
		}
	}

void deinit_embedded_perl(void) {

	PL_perl_destruct_level = 0;
	perl_destruct(my_perl);
	perl_free(my_perl);
	}


int main(int argc, char **argv, char **env) {

	init_embedded_perl();
	/* Calls Perl to load and construct a new
	 * Term::ReadLine object.
	 */

	init_term_readline();

	while(1) {
		/*
		 * get_command_line calls Perl to get a scalar from stdin
		 */

		/* Perl Term::ReadLine::readline() method chomps the "\n"
		 * from the end of the input.
		 */
		char *cmd, *end;
		/* Allow any length command line */
		cmd = (get_command_line()) ;

		// Trim leading whitespace
		while(isspace(*cmd)) cmd++;

		// Trim trailing whitespace
		end = cmd + strlen(cmd) - 1;
		while(end > cmd && isspace(*end)) end--;

		// Write new null terminator
		*(end + 1) = 0;

		run_plugin(cmd) ;
		}

	deinit_embedded_perl();
	}

