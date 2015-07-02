#include "../include/config.h"
#include "../include/common.h"
#include "../include/locations.h"
#include "../include/defaults.h"

/*
 * This file holds random utility functions shared by cgi's and
 * core, as well as all global variables needed by both.
 */
int date_format;
int interval_length;
char *illegal_output_chars;
char illegal_output_char_map[] = CHAR_MAP_INIT(0);
time_t program_start = 0L;
int check_external_commands;
int log_rotation_method;

char *object_cache_file;
struct object_count num_objects;

int process_performance_data;
char *status_file;

int nagios_pid = 0;
int daemon_mode = FALSE;

time_t last_log_rotation = 0L;

int check_external_commands;

int enable_timing_point = FALSE; /* cgi's never set this to TRUE */

int enable_flap_detection;
int enable_notifications;
int execute_service_checks;
int accept_passive_service_checks;
int execute_host_checks;
int accept_passive_host_checks;
int enable_event_handlers;
int obsess_over_services;
int obsess_over_hosts;

char *config_file_dir = NULL;

void init_shared_cfg_vars(int first_time) {
	date_format = DATE_FORMAT_US;
	interval_length = DEFAULT_INTERVAL_LENGTH;
	if(first_time) {
		/* Not sure why these are not reset in reset_variables() */
		illegal_output_chars = NULL;
	}
	program_start = 0L;
	log_rotation_method = LOG_ROTATION_NONE;

	object_cache_file = strdup(DEFAULT_OBJECT_CACHE_FILE);

	process_performance_data = DEFAULT_PROCESS_PERFORMANCE_DATA;
	status_file = NULL;

	check_external_commands = DEFAULT_CHECK_EXTERNAL_COMMANDS;

	enable_flap_detection = DEFAULT_ENABLE_FLAP_DETECTION;
	enable_notifications = TRUE;
	execute_service_checks = TRUE;
	accept_passive_service_checks = TRUE;
	execute_host_checks = TRUE;
	accept_passive_host_checks = TRUE;
	enable_event_handlers = TRUE;
	obsess_over_services = FALSE;
	obsess_over_hosts = FALSE;

	return;
}


/* silly debug-ish helper used to track down hotspots in config parsing */
void timing_point(const char *fmt, ...) {
	static struct timeval last = {0, 0}, first = {0, 0};
	struct timeval now;
	va_list ap;

	if(!enable_timing_point)
		return;

	if(first.tv_sec == 0) {
		gettimeofday(&first, NULL);
		last.tv_sec = first.tv_sec;
		last.tv_usec = first.tv_usec;
		printf("[0.0000 (+0.0000)] ");
		}
	else {
		gettimeofday(&now, NULL);
		printf("[%.4f (+%.4f)] ", tv_delta_f(&first, &now), tv_delta_f(&last, &now));
		last.tv_sec = now.tv_sec;
		last.tv_usec = now.tv_usec;
		}
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	}

/* fix the problem with strtok() skipping empty options between tokens */
char *my_strtok(char *buffer, const char *tokens) {
	char *token_position = NULL;
	char *sequence_head = NULL;
	static char *my_strtok_buffer = NULL;
	static char *original_my_strtok_buffer = NULL;

	if(buffer != NULL) {
		my_free(original_my_strtok_buffer);
		if((my_strtok_buffer = (char *)strdup(buffer)) == NULL)
			return NULL;
		original_my_strtok_buffer = my_strtok_buffer;
		}

	sequence_head = my_strtok_buffer;

	if(sequence_head[0] == '\x0')
		return NULL;

	token_position = strchr(my_strtok_buffer, tokens[0]);

	if(token_position == NULL) {
		my_strtok_buffer = strchr(my_strtok_buffer, '\x0');
		return sequence_head;
		}

	token_position[0] = '\x0';
	my_strtok_buffer = token_position + 1;

	return sequence_head;
	}

/* fixes compiler problems under Solaris, since strsep() isn't included */
/* this code is taken from the glibc source */
char *my_strsep(char **stringp, const char *delim) {
	char *begin, *end;

	begin = *stringp;
	if(begin == NULL)
		return NULL;

	/* A frequent case is when the delimiter string contains only one
	 * character.  Here we don't need to call the expensive `strpbrk'
	 * function and instead work using `strchr'.  */
	if(delim[0] == '\0' || delim[1] == '\0') {
		char ch = delim[0];

		if(ch == '\0' || begin[0] == '\0')
			end = NULL;
		else {
			if(*begin == ch)
				end = begin;
			else
				end = strchr(begin + 1, ch);
			}
		}
	else {
		/* find the end of the token.  */
		end = strpbrk(begin, delim);
		}

	if(end) {
		/* terminate the token and set *STRINGP past NUL character.  */
		*end++ = '\0';
		*stringp = end;
		}
	else
		/* no more delimiters; this is the last token.  */
		*stringp = NULL;

	return begin;
	}

/* open a file read-only via mmap() */
mmapfile *mmap_fopen(const char *filename) {
	mmapfile *new_mmapfile = NULL;
	int fd = 0;
	void *mmap_buf = NULL;
	struct stat statbuf;
	int mode = O_RDONLY;
	unsigned long file_size = 0L;

	if(filename == NULL)
		return NULL;

	/* allocate memory */
	if((new_mmapfile = (mmapfile *) malloc(sizeof(mmapfile))) == NULL)
		return NULL;

	/* open the file */
	if((fd = open(filename, mode)) == -1) {
		my_free(new_mmapfile);
		return NULL;
		}

	/* get file info */
	if((fstat(fd, &statbuf)) == -1) {
		close(fd);
		my_free(new_mmapfile);
		return NULL;
		}

	/* get file size */
	file_size = (unsigned long)statbuf.st_size;

	/* only mmap() if we have a file greater than 0 bytes */
	if(file_size > 0) {

		/* mmap() the file - allocate one extra byte for processing zero-byte files */
		if((mmap_buf =
		            (void *)mmap(0, file_size, PROT_READ, MAP_PRIVATE, fd,
		                         0)) == MAP_FAILED) {
			close(fd);
			my_free(new_mmapfile);
			return NULL;
			}
		}
	else
		mmap_buf = NULL;

	/* populate struct info for later use */
	new_mmapfile->path = (char *)strdup(filename);
	new_mmapfile->fd = fd;
	new_mmapfile->file_size = (unsigned long)file_size;
	new_mmapfile->current_position = 0L;
	new_mmapfile->current_line = 0L;
	new_mmapfile->mmap_buf = mmap_buf;

	return new_mmapfile;
	}

/* close a file originally opened via mmap() */
int mmap_fclose(mmapfile * temp_mmapfile) {

	if(temp_mmapfile == NULL)
		return ERROR;

	/* un-mmap() the file */
	if(temp_mmapfile->file_size > 0L)
		munmap(temp_mmapfile->mmap_buf, temp_mmapfile->file_size);

	/* close the file */
	close(temp_mmapfile->fd);

	/* free memory */
	my_free(temp_mmapfile->path);
	my_free(temp_mmapfile);

	return OK;
	}

/* gets one line of input from an mmap()'ed file */
char *mmap_fgets(mmapfile * temp_mmapfile) {
	char *buf = NULL;
	unsigned long x = 0L;
	int len = 0;

	if(temp_mmapfile == NULL)
		return NULL;

	/* size of file is 0 bytes */
	if(temp_mmapfile->file_size == 0L)
		return NULL;

	/* we've reached the end of the file */
	if(temp_mmapfile->current_position >= temp_mmapfile->file_size)
		return NULL;

	/* find the end of the string (or buffer) */
	for(x = temp_mmapfile->current_position; x < temp_mmapfile->file_size;
	        x++) {
		if(*((char *)(temp_mmapfile->mmap_buf) + x) == '\n') {
			x++;
			break;
			}
		}

	/* calculate length of line we just read */
	len = (int)(x - temp_mmapfile->current_position);

	/* allocate memory for the new line */
	if((buf = (char *)malloc(len + 1)) == NULL)
		return NULL;

	/* copy string to newly allocated memory and terminate the string */
	memcpy(buf,
	       ((char *)(temp_mmapfile->mmap_buf) +
	        temp_mmapfile->current_position), len);
	buf[len] = '\x0';

	/* update the current position */
	temp_mmapfile->current_position = x;

	/* increment the current line */
	temp_mmapfile->current_line++;

	return buf;
	}

/* gets one line of input from an mmap()'ed file (may be contained on more than one line in the source file) */
char *mmap_fgets_multiline(mmapfile * temp_mmapfile) {
	char *buf = NULL;
	char *tempbuf = NULL;
	char *stripped = NULL;
	int len = 0;
	int len2 = 0;
	int end = 0;

	if(temp_mmapfile == NULL)
		return NULL;

	while(1) {

		my_free(tempbuf);

		if((tempbuf = mmap_fgets(temp_mmapfile)) == NULL)
			break;

		if(buf == NULL) {
			len = strlen(tempbuf);
			if((buf = (char *)malloc(len + 1)) == NULL)
				break;
			memcpy(buf, tempbuf, len);
			buf[len] = '\x0';
			}
		else {
			/* strip leading white space from continuation lines */
			stripped = tempbuf;
			while(*stripped == ' ' || *stripped == '\t')
				stripped++;
			len = strlen(stripped);
			len2 = strlen(buf);
			if((buf =
			            (char *)realloc(buf, len + len2 + 1)) == NULL)
				break;
			strcat(buf, stripped);
			len += len2;
			buf[len] = '\x0';
			}

		if(len == 0)
			break;

		/* handle Windows/DOS CR/LF */
		if(len >= 2 && buf[len - 2] == '\r')
			end = len - 3;
		/* normal Unix LF */
		else if(len >= 1 && buf[len - 1] == '\n')
			end = len - 2;
		else
			end = len - 1;

		/* two backslashes found. unescape first backslash first and break */
		if(end >= 1 && buf[end - 1] == '\\' && buf[end] == '\\') {
			buf[end] = '\n';
			buf[end + 1] = '\x0';
			break;
			}

		/* one backslash found. continue reading the next line */
		else if(end > 0 && buf[end] == '\\')
			buf[end] = '\x0';

		/* no continuation marker was found, so break */
		else
			break;
		}

	my_free(tempbuf);

	return buf;
	}

/* strip newline, carriage return, and tab characters from beginning and end of a string */
void strip(char *buffer) {
	register int x, z;
	int len;

	if(buffer == NULL || buffer[0] == '\x0')
		return;

	/* strip end of string */
	len = (int)strlen(buffer);
	for(x = len - 1; x >= 0; x--) {
		switch(buffer[x]) {
			case ' ':
			case '\n':
			case '\r':
			case '\t':
				buffer[x] = '\x0';
				continue;
			}
		break;
		}

	/* if we stripped all of it, just return */
	if(!x)
		return;

	/* save last position for later... */
	z = x;

	/* strip beginning of string (by shifting) */
	/* NOTE: this is very expensive to do, so avoid it whenever possible */
	for(x = 0;; x++) {
		switch(buffer[x]) {
			case ' ':
			case '\n':
			case '\r':
			case '\t':
				continue;
			}
		break;
		}

	if(x > 0 && z > 0) {
		/* new length of the string after we stripped the end */
		len = z + 1;

		/* shift chars towards beginning of string to remove leading whitespace */
		for(z = x; z < len; z++)
			buffer[z - x] = buffer[z];
		buffer[len - x] = '\x0';
		}
	}

/**************************************************
 *************** HASH FUNCTIONS *******************
 **************************************************/
/* dual hash function */
int hashfunc(const char *name1, const char *name2, int hashslots) {
	unsigned int i, result;

	result = 0;

	if(name1)
		for(i = 0; i < strlen(name1); i++)
			result += name1[i];

	if(name2)
		for(i = 0; i < strlen(name2); i++)
			result += name2[i];

	result = result % hashslots;

	return result;
	}

/* dual hash data comparison */
int compare_hashdata(const char *val1a, const char *val1b, const char *val2a,
                     const char *val2b) {
	int result = 0;

	/* NOTE: If hash calculation changes, update the compare_strings() function! */

	/* check first name */
	if(val1a == NULL && val2a == NULL)
		result = 0;
	else if(val1a == NULL)
		result = 1;
	else if(val2a == NULL)
		result = -1;
	else
		result = strcmp(val1a, val2a);

	/* check second name if necessary */
	if(result == 0) {
		if(val1b == NULL && val2b == NULL)
			result = 0;
		else if(val1b == NULL)
			result = 1;
		else if(val2b == NULL)
			result = -1;
		else
			result = strcmp(val1b, val2b);
		}

	return result;
	}
/*
 * given a date/time in time_t format, produce a corresponding
 * date/time string, including timezone
 */
void get_datetime_string(time_t * raw_time, char *buffer, int buffer_length,
                         int type) {
	time_t t;
	struct tm *tm_ptr, tm_s;
	int hour;
	int minute;
	int second;
	int month;
	int day;
	int year;
	const char *weekdays[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	const char *months[12] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sept",
		"Oct", "Nov", "Dec"
		};
	const char *tzone = "";

	if(raw_time == NULL)
		time(&t);
	else
		t = *raw_time;

	if(type == HTTP_DATE_TIME)
		tm_ptr = gmtime_r(&t, &tm_s);
	else
		tm_ptr = localtime_r(&t, &tm_s);

	hour = tm_ptr->tm_hour;
	minute = tm_ptr->tm_min;
	second = tm_ptr->tm_sec;
	month = tm_ptr->tm_mon + 1;
	day = tm_ptr->tm_mday;
	year = tm_ptr->tm_year + 1900;

#ifdef HAVE_TM_ZONE
	tzone = tm_ptr->tm_zone;
#else
	tzone = (tm_ptr->tm_isdst) ? tzname[1] : tzname[0];
#endif

	/* ctime() style date/time */
	if(type == LONG_DATE_TIME)
		snprintf(buffer, buffer_length, "%s %s %d %02d:%02d:%02d %s %d",
		         weekdays[tm_ptr->tm_wday], months[tm_ptr->tm_mon], day,
		         hour, minute, second, tzone, year);

	/* short date/time */
	else if(type == SHORT_DATE_TIME) {
		if(date_format == DATE_FORMAT_EURO)
			snprintf(buffer, buffer_length,
			         "%02d-%02d-%04d %02d:%02d:%02d", day, month,
			         year, hour, minute, second);
		else if(date_format == DATE_FORMAT_ISO8601
		        || date_format == DATE_FORMAT_STRICT_ISO8601)
			snprintf(buffer, buffer_length,
			         "%04d-%02d-%02d%c%02d:%02d:%02d", year, month,
			         day,
			         (date_format ==
			          DATE_FORMAT_STRICT_ISO8601) ? 'T' : ' ', hour,
			         minute, second);
		else
			snprintf(buffer, buffer_length,
			         "%02d-%02d-%04d %02d:%02d:%02d", month, day,
			         year, hour, minute, second);
		}

	/* short date */
	else if(type == SHORT_DATE) {
		if(date_format == DATE_FORMAT_EURO)
			snprintf(buffer, buffer_length, "%02d-%02d-%04d", day,
			         month, year);
		else if(date_format == DATE_FORMAT_ISO8601
		        || date_format == DATE_FORMAT_STRICT_ISO8601)
			snprintf(buffer, buffer_length, "%04d-%02d-%02d", year,
			         month, day);
		else
			snprintf(buffer, buffer_length, "%02d-%02d-%04d", month,
			         day, year);
		}

	/* expiration date/time for HTTP headers */
	else if(type == HTTP_DATE_TIME)
		snprintf(buffer, buffer_length,
		         "%s, %02d %s %d %02d:%02d:%02d GMT",
		         weekdays[tm_ptr->tm_wday], day, months[tm_ptr->tm_mon],
		         year, hour, minute, second);

	/* short time */
	else
		snprintf(buffer, buffer_length, "%02d:%02d:%02d", hour, minute,
		         second);

	buffer[buffer_length - 1] = '\x0';
	}

/* get days, hours, minutes, and seconds from a raw time_t format or total seconds */
void get_time_breakdown(unsigned long raw_time, int *days, int *hours,
                        int *minutes, int *seconds) {
	unsigned long temp_time;
	int temp_days;
	int temp_hours;
	int temp_minutes;
	int temp_seconds;

	temp_time = raw_time;

	temp_days = temp_time / 86400;
	temp_time -= (temp_days * 86400);
	temp_hours = temp_time / 3600;
	temp_time -= (temp_hours * 3600);
	temp_minutes = temp_time / 60;
	temp_time -= (temp_minutes * 60);
	temp_seconds = (int)temp_time;

	*days = temp_days;
	*hours = temp_hours;
	*minutes = temp_minutes;
	*seconds = temp_seconds;
	}
