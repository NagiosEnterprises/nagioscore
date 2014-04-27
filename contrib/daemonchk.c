#include "config.h"
#include "common.h"
#include "locations.h"
#include "cgiutils.h"
#include "getcgi.h"
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <stdarg.h>

#define CHARLEN 256
#define max(a,b) ((a)>(b))?(a):(b)

static void document_header(void);
//static void document_footer(void);
static int process_cgivars(void);

static char *strscpy(char *dest, const char *src);
static char *ssprintf(char *str, const char *fmt, ...);
static void terminate(int result, const char *fmt, ...);
static void get_expire_time_string(time_t *raw_time, char *buffer, int buffer_length);

int main(int argc, char **argv) {
	FILE *fp;
	char *status_file = NULL;
	char *lock_file = NULL;
	char *proc_file = NULL;
	char input_buffer[CHARLEN];
	int c, age, pid, testpid, found;
	int wt = -1;
	int ct = -1;
	struct stat statbuf;
	time_t current_time;

#ifdef DEFAULT_STATUS_FILE
	status_file = strscpy(status_file, DEFAULT_STATUS_FILE);
#else
	status_file = strscpy(status_file, "/var/log/nagios/status.log");
#endif

#ifdef DEFAULT_LOCK_FILE
	lock_file = strscpy(lock_file, DEFAULT_LOCK_FILE);
#else
	lock_file = strscpy(lock_file, "/tmp/nagios.lock");
#endif

	if(getenv("REQUEST_METHOD")) {
		process_cgivars();
		document_header();
		}
	else {   /* get arguments */
		while((c = getopt(argc, argv, "+c:w:s:l:")) != EOF) {
			switch(c) {
				case 'c':
					ct = atoi(optarg);
					break;
				case 'w':
					wt = atoi(optarg);
					break;
				case 's':
					status_file = optarg;
					break;
				case 'l':
					lock_file = optarg;
					break;
				}
			}
		}

	/* find status file, get lastmod time */
	if(stat(status_file, &statbuf) == -1) {
		printf("NAGIOS CRITICAL - could not find status log: %s\n", status_file);
		exit(STATE_CRITICAL);
		}
	time(&current_time);
	age = (int)(current_time - statbuf.st_mtime);

	/* find lock file.  get pid if it exists */
	if(stat(lock_file, &statbuf) == -1) {
		printf("NAGIOS CRITICAL - could not find lock file: %s\n", lock_file);
		exit(STATE_CRITICAL);
		}
	fp = fopen(lock_file, "r");
	fscanf(fp, "%d", &pid);
	fclose(fp);
	proc_file = ssprintf(proc_file, "/proc/%d", pid);

	if(stat("/proc", &statbuf) == 0) {
		if(stat(proc_file, &statbuf) == -1) {
			printf("NAGIOS CRITICAL - could not find proc file: %s\n", proc_file);
			exit(STATE_CRITICAL);
			}
		}
	else if(snprintf(proc_file, CHARLEN - 1, "/bin/ps -o pid -p %d", pid) &&
	        (fp = popen(proc_file, "r")) != NULL) {
		fgets(input_buffer, CHARLEN - 1, fp);
		fgets(input_buffer, CHARLEN - 1, fp);
		if(sscanf(input_buffer, "%d", &testpid) == 1) {
			if(testpid != pid) {
				printf("NAGIOS CRITICAL - could not find process(1): %d\n", pid);
				exit(STATE_CRITICAL);
				}
			}
		}
	else if(snprintf(proc_file, CHARLEN - 1, "/bin/ps -eo pid") &&
	        (fp = popen(proc_file, "r")) != NULL) {
		found = FALSE;
		fgets(input_buffer, CHARLEN - 1, fp);
		while(fgets(input_buffer, CHARLEN - 1, fp)) {
			if(sscanf(input_buffer, "%d", &testpid) == 1)
				if(testpid == pid) found = TRUE;
			}
		if(!found) {
			printf("NAGIOS CRITICAL - could not find process(2): %d\n", pid);
			exit(STATE_CRITICAL);
			}
		}
	else if(snprintf(proc_file, CHARLEN - 1, "/bin/ps -Ao pid") &&
	        (fp = popen(proc_file, "r")) != NULL) {
		found = FALSE;
		fgets(input_buffer, CHARLEN - 1, fp);
		while(fgets(input_buffer, CHARLEN - 1, fp)) {
			if(sscanf(input_buffer, "%d", &testpid) == 1)
				if(testpid == pid) found = TRUE;
			}
		if(!found) {
			printf("NAGIOS CRITICAL - could not find process(2): %d\n", pid);
			exit(STATE_CRITICAL);
			}
		}

	if(ct > 0 && ct < age) {
		printf("NAGIOS CRITICAL - status written %d seconds ago\n", age);
		exit(STATE_CRITICAL);
		}
	else if(wt > 0 && wt < age) {
		printf("NAGIOS WARNING - status written %d seconds ago\n", age);
		exit(STATE_WARNING);
		}
	else {
		printf("NAGIOS ok - status written %d seconds ago\n", age);
		exit(STATE_OK);
		}

	}

static void document_header(void) {
	char date_time[48];
	time_t current_time;

	printf("Cache-Control: no-store\nPragma: no-cache\n");

	time(&current_time);
	get_expire_time_string(&current_time, date_time, (int)sizeof(date_time));
	printf("Last-Modified: %s\n", date_time);
	printf("Expires: %s\n", date_time);

	printf("Content-type: text/html\n\n");

	printf("<html>\n<head>\n<title>Nagios Daemon Status</title>\n</head>\n");
	printf("<body onunload=\"if (window.wnd && wnd.close) wnd.close(); return true;\">\n");

	return;
	}

static int process_cgivars(void) {
	char **variables;
	int error = FALSE;
	int x;

	variables = getcgivars();

	for(x = 0; variables[x] != NULL; x++) {

		/* do some basic length checking on the variable identifier to prevent buffer overflows */
		if(strlen(variables[x]) >= MAX_INPUT_BUFFER - 1) {
			continue;
			}
		}
	return error;
	}

/* get date/time string used in META tags for page expiration */
static void get_expire_time_string(time_t *raw_time, char *buffer, int buffer_length) {
	time_t t;
	struct tm *tm_ptr;
	int day;
	int hour;
	int minute;
	int second;
	int year;
	char *weekdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sept", "Oct", "Nov", "Dec"};

	if(raw_time == NULL)
		time(&t);
	else
		t = *raw_time;

	tm_ptr = gmtime(&t);

	hour = tm_ptr->tm_hour;
	minute = tm_ptr->tm_min;
	second = tm_ptr->tm_sec;
	day = tm_ptr->tm_mday;
	year = tm_ptr->tm_year + 1900;

	snprintf(buffer, buffer_length, "%s, %d %s %d %02d:%02d:%02d GMT", weekdays[tm_ptr->tm_wday], day, months[tm_ptr->tm_mon], year, hour, minute, second);
	buffer[buffer_length - 1] = '\x0';

	return;
	}

static char *strscpy(char *dest, const char *src) {
	int len;

	if(src != NULL)
		len = strlen(src) + 1;
	else
		return dest;

	if(dest == NULL || strlen(dest) < len)
		dest = realloc(dest, len);
	if(dest == NULL)
		terminate(STATE_UNKNOWN, "failed realloc in strscpy\n");

	strncpy(dest, src, len);

	return dest;
	}

static char *ssprintf(char *str, const char *fmt, ...) {
	va_list ap;
	int nchars;
	int size;

	if(str == NULL)
		str = malloc(CHARLEN);
	if(str == NULL)
		terminate(STATE_UNKNOWN, "malloc failed in ssprintf");

	size = max(strlen(str), CHARLEN);

	va_start(ap, fmt);

	while(1) {

		nchars = vsnprintf(str, size, fmt, ap);

		if(nchars > -1)
			if(nchars < size) {
				va_end(ap);
				return str;
				}
			else {
				size = nchars + 1;
				}

		else
			size *= 2;

		str = realloc(str, nchars + 1);

		if(str == NULL)
			terminate(STATE_UNKNOWN, "realloc failed in ssprintf");
		}

	}

static void terminate(int result, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	exit(result);
	}
