#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main()
{
    // struct tm timed;
    // "%04d-%02d-%02d%*[ T]%02d:%02d:%02d"
    time_t entry_time = 0L;
    char cmd[20];
    char *host_name = NULL;
    int persistent = 0;
    int expires = 0;
    time_t expires_time = 0L;
    char *author = NULL;
    char *comment = NULL;
    char letter = '\0';

    sscanf("[1580276192] ADD_HOST_COMMENT;localhost;1;1;1580276132;NagiosAdmin;asdfasfasdfasdfsdafsdafasdfl\n", "[%i]%c%s;%s;%i;%i;%i;%s;%s\n" , &entry_time, letter, cmd, host_name, &persistent, &expires, &expires_time, author, comment); 

    fprintf(stdout, "I am the TIME:  %lu %s\n\n", entry_time, cmd);
    return 0;
}
