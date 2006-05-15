/******************************************
 *
 * GETCGI.C -  Nagios CGI Input Routines
 *
 * Last Modified: 05-15-2006
 *
 *****************************************/

#include "../include/config.h"
#include "../include/getcgi.h"
#include <stdio.h>
#include <stdlib.h>


#undef PARANOID_CGI_INPUT


/* Remove potentially harmful characters from CGI input that we don't need or want */
void sanitize_cgi_input(char **cgivars){
	char *strptr;
	int x,y,i;
	int keep;

	/* don't strip for now... */
	return;

	for(strptr=cgivars[i=0];strptr!=NULL;strptr=cgivars[++i]){

		for(x=0,y=0;strptr[x]!='\x0';x++){

			keep=1;

			/* remove potentially nasty characters */
			if(strptr[x]==';' || strptr[x]=='|' || strptr[x]=='&' || strptr[x]=='<' || strptr[x]=='>')
				keep=0;
#ifdef PARANOID_CGI_INPUT
			else if(strptr[x]=='/' || strptr[x]=='\\')
				keep=0;
#endif
			if(keep==1)
				strptr[y++]=strptr[x];
		        }

		strptr[y]='\x0';
	        }

	return;
        }


/* convert encoded hex string (2 characters representing an 8-bit number) to its ASCII char equivalent */
unsigned char hex_to_char(char *input){
	unsigned char outchar='\x0';
	unsigned int outint;
	char tempbuf[3];

	/* NULL or empty string */
	if(input==NULL)
		return '\x0';
	if(input[0]=='\x0')
		return '\x0';

	tempbuf[0]=input[0];
	tempbuf[1]=input[1];
	tempbuf[2]='\x0';

	sscanf(tempbuf,"%X",&outint);

	/* only convert "normal" ASCII characters - we don't want the rest.  Normally you would 
	   convert all characters (i.e. for allowing users to post binary files), but since we
	   aren't doing this, stay on the cautious side of things and reject outsiders... */
#ifdef PARANOID_CGI_INPUT
	if(outint<32 || outint>126)
		outint=0;
#endif

	outchar=(unsigned char)outint;

	return outchar;
        }



/* unescape hex characters in CGI input */
void unescape_cgi_input(char *input){
	int x,y;
	int len;

	if(input==NULL)
		return;

	len=strlen(input);
	for(x=0,y=0;x<len;x++,y++){

		if(input[x]=='\x0')
			break;
		else if(input[x]=='%'){
			input[y]=hex_to_char(&input[x+1]);
			x+=2;
		        }
		else
			input[y]=input[x];
	        }
	input[y]='\x0';

	return;
        }



/* read the CGI input and place all name/val pairs into list. returns list containing name1, value1, name2, value2, ... , NULL */
/* this is a hacked version of a routine I found a long time ago somewhere - can't remember where anymore */
char **getcgivars(void){
	register int i;
	char *request_method;
	char *content_type;
	char *content_length_string;
	int content_length;
	char *cgiinput;
	char **cgivars;
	char **pairlist;
	int paircount;
	char *nvpair;
	char *eqpos;

	/* initialize char variable(s) */
	cgiinput="";

	/* depending on the request method, read all CGI input into cgiinput */

	request_method=getenv("REQUEST_METHOD");
	if(request_method==NULL)
		request_method="";

	if(!strcmp(request_method,"GET") || !strcmp(request_method,"HEAD")){

		/* check for NULL query string environment variable - 04/28/00 (Ludo Bosmans) */
		if(getenv("QUERY_STRING")==NULL){
			cgiinput=(char *)malloc(1);
			if(cgiinput==NULL){
				printf("getcgivars(): Could not allocate memory for CGI input.\n");
				exit(1);
			        }
			cgiinput[0]='\x0';
		        }
		else
			cgiinput=strdup(getenv("QUERY_STRING"));
	        }

	else if(!strcmp(request_method,"POST") || !strcmp(request_method,"PUT")){

		/* if CONTENT_TYPE variable is not specified, RFC-2068 says we should assume it is "application/octet-string" */
		/* mobile (WAP) stations generate CONTENT_TYPE with charset, we we should only check first 33 chars */

		content_type=getenv("CONTENT_TYPE");
		if(content_type==NULL)
			content_type="";

		if(strlen(content_type) && strncasecmp(content_type,"application/x-www-form-urlencoded",33)){
			printf("getcgivars(): Unsupported Content-Type.\n");
			exit(1);
		        }

		content_length_string=getenv("CONTENT_LENGTH");
		if(content_length_string==NULL)
			content_length_string="0";

		if(!(content_length=atoi(content_length_string))){
			printf("getcgivars(): No Content-Length was sent with the POST request.\n") ;
			exit(1);
		        }
		/* suspicious content length */
		if((content_length<0) || (content_length>=INT_MAX-1)){
			printf("getcgivars(): Suspicious Content-Length was sent with the POST request.\n");
			exit(1);
			}

		if(!(cgiinput=(char *)malloc(content_length+1))){
			printf("getcgivars(): Could not allocate memory for CGI input.\n");
			exit(1);
		        }
		if(!fread(cgiinput,content_length,1,stdin)){
			printf("getcgivars(): Could not read input from STDIN.\n");
			exit(1);
		        }
		cgiinput[content_length]='\0';
	        }
	else{

		printf("getcgivars(): Unsupported REQUEST_METHOD -> '%s'\n",request_method);
		printf("\n");
		printf("I'm guessing you're trying to execute the CGI from a command line.\n");
		printf("In order to do that, you need to set the REQUEST_METHOD environment\n");
		printf("variable to either \"GET\", \"HEAD\", or \"POST\".  When using the\n");
		printf("GET and HEAD methods, arguments can be passed to the CGI\n");
		printf("by setting the \"QUERY_STRING\" environment variable.  If you're\n");
		printf("using the POST method, data is read from standard input.  Also of\n");
		printf("note: if you've enabled authentication in the CGIs, you must set the\n");
		printf("\"REMOTE_USER\" environment variable to be the name of the user you're\n");
		printf("\"authenticated\" as.\n");
		printf("\n");

		exit(1);
	        }

	/* change all plus signs back to spaces */
	for(i=0;cgiinput[i];i++){
		if(cgiinput[i]=='+')
			cgiinput[i]=' ';
	        }

	/* first, split on ampersands (&) to extract the name-value pairs into pairlist */
	/* allocate memory for 256 name-value pairs at a time, increasing by same
	   amount as necessary... */
	pairlist=(char **)malloc(256*sizeof(char **));
	if(pairlist==NULL){
		printf("getcgivars(): Could not allocate memory for name-value pairlist.\n");
		exit(1);
	        }
	paircount=0;
	nvpair=strtok(cgiinput,"&");
	while(nvpair){
		pairlist[paircount++]=strdup(nvpair);
		if(!(paircount%256)){
			pairlist=(char **)realloc(pairlist,(paircount+256)*sizeof(char **));
			if(pairlist==NULL){
				printf("getcgivars(): Could not re-allocate memory for name-value pairlist.\n");
				exit(1);
			        }
		        }
		nvpair=strtok(NULL,"&");
	        }

	/* terminate the list */
	pairlist[paircount]='\x0';    

	/* extract the names and values from the pairlist */
	cgivars=(char **)malloc((paircount*2+1)*sizeof(char **));
	if(cgivars==NULL){
		printf("getcgivars(): Could not allocate memory for name-value list.\n");
		exit(1);
	        }
	for(i=0;i<paircount;i++){

		/* get the variable name preceding the equal (=) sign */
		if((eqpos=strchr(pairlist[i],'='))!=NULL){
			*eqpos='\0';
			unescape_cgi_input(cgivars[i*2+1]=strdup(eqpos+1));
		        } 
		else
			unescape_cgi_input(cgivars[i*2+1]=strdup(""));

		/* get the variable value (or name/value of there was no real "pair" in the first place) */
		unescape_cgi_input(cgivars[i*2]=strdup(pairlist[i]));
	        }

	/* terminate the name-value list */
	cgivars[paircount*2]='\x0';
    
	/* free allocated memory */
	free(cgiinput);
	for(i=0;pairlist[i]!=NULL;i++)
		free(pairlist[i]);
	free(pairlist);

	/* sanitize the name-value strings */
	sanitize_cgi_input(cgivars);

	/* return the list of name-value strings */
	return cgivars;
        }



/* free() memory allocated to storing the CGI variables */
void free_cgivars(char **cgivars){
	register int x;

	for(x=0;cgivars[x]!='\x0';x++)
		free(cgivars[x]);

	return;
        }
