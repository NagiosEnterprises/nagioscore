/* include file for popen.c */

pid_t *childpid;
int *childerr;

FILE *spopen(const char *);
int spclose(FILE *);
