/*****************************************************************************
 *
 * NEBMODS.C - Event Broker Module Functions
 *
 * Copyright (c) 2002-2004 Ethan Galstad (nagios@nagios.org)
 * Last Modified:   10-24-2004
 *
 * License:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/nebmods.h"
#include "../include/neberrors.h"
#include "../include/nagios.h"


nebmodule *neb_module_list=NULL;
nebcallback **neb_callback_list=NULL;


/*#define DEBUG*/


/****************************************************************************/
/****************************************************************************/
/* INITIALIZATION/CLEANUP FUNCTIONS                                         */
/****************************************************************************/
/****************************************************************************/

/* initialize module routines */
int neb_init_modules(void){
	int result;

	/* initialize library */
#ifdef USE_LTDL
	result=lt_dlinit();
	if(result)
		return ERROR;
#endif

	return OK;
        }


/* deinitialize module routines */
int neb_deinit_modules(void){
	int result;

	/* deinitialize library */
#ifdef USE_LTDL
	result=lt_dlexit();
	if(result)
		return ERROR;
#endif

	return OK;
        }



/* add a new module to module list */
int neb_add_module(char *filename,char *args,int should_be_loaded){
	nebmodule *new_module;
	int x;

	if(filename==NULL)
		return ERROR;

	/* allocate memory */
	new_module=(nebmodule *)malloc(sizeof(nebmodule));
	if(new_module==NULL)
		return ERROR;

	/* initialize vars */
	new_module->filename=(char *)strdup(filename);
	new_module->args=(args==NULL)?NULL:(char *)strdup(args);
	new_module->should_be_loaded=should_be_loaded;
	new_module->is_currently_loaded=FALSE;
	for(x=0;x<NEBMODULE_MODINFO_NUMITEMS;x++)
		new_module->info[x]=NULL;
	new_module->module_handle=NULL;
	new_module->init_func=NULL;
	new_module->deinit_func=NULL;
	new_module->thread_id=-1;

	/* add module to head of list */
	new_module->next=neb_module_list;
	neb_module_list=new_module;

#ifdef DEBUG
	printf("Added module: name='%s', args='%s', should_be_loaded='%d'\n",filename,args,should_be_loaded);
#endif

	return OK;
        }


/* free memory allocated to module list */
int neb_free_module_list(void){
	nebmodule *temp_module;
	nebmodule *next_module;
	int x;

	for(temp_module=neb_module_list;temp_module;){
		next_module=temp_module->next;
		free(temp_module->filename);
		free(temp_module->args);
		for(x=0;x<NEBMODULE_MODINFO_NUMITEMS;x++)
			free(temp_module->info[x]);
		free(temp_module);
		temp_module=next_module;
	        }

	neb_module_list=NULL;

	return OK;
        }



/****************************************************************************/
/****************************************************************************/
/* LOAD/UNLOAD FUNCTIONS                                                    */
/****************************************************************************/
/****************************************************************************/


/* load all modules */
int neb_load_all_modules(void){
	nebmodule *temp_module;
	int result;

	for(temp_module=neb_module_list;temp_module;temp_module=temp_module->next){
		result=neb_load_module(temp_module);
	        }

	return OK;
        }


/* load a particular module */
int neb_load_module(nebmodule *mod){
	char temp_buffer[MAX_INPUT_BUFFER];
	int (*initfunc)(int,char *,void *);
	int *module_version_ptr;
	int result;

	if(mod==NULL || mod->filename==NULL)
		return ERROR;

	/* don't reopen the module */
	if(mod->is_currently_loaded==TRUE)
		return OK;

	/* don't load modules unless they should be loaded */
	if(mod->should_be_loaded==FALSE)
		return ERROR;

	/* load the module */
#ifdef USE_LTDL
	mod->module_handle=lt_dlopen(mod->filename);
#else
	mod->module_handle=dlopen(mod->filename,RTLD_NOW|RTLD_GLOBAL);
#endif
	if(mod->module_handle==NULL){

#ifdef USE_LTDL
		snprintf(temp_buffer,sizeof(temp_buffer),"Error: Could not load module '%s' -> %s\n",mod->filename,lt_dlerror());
#else
		snprintf(temp_buffer,sizeof(temp_buffer),"Error: Could not load module '%s' -> %s\n",mod->filename,dlerror());
#endif
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_all_logs(temp_buffer,NSLOG_RUNTIME_ERROR);

		return ERROR;
	        }

	/* mark the module as being loaded */
	mod->is_currently_loaded=TRUE;

	/* find module API version */
#ifdef USE_LTDL
	module_version_ptr=(int *)lt_dlsym(mod->module_handle,"__neb_api_version");
#else
	module_version_ptr=(int *)dlsym(mod->module_handle,"__neb_api_version");
#endif
	
	/* check the module API version */
	if(module_version_ptr==NULL || ((*module_version_ptr)!=CURRENT_NEB_API_VERSION)){

		snprintf(temp_buffer,sizeof(temp_buffer),"Error: Module '%s' is using an old or unspecified version of the event broker API.  Module will be unloaded.\n",mod->filename);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_all_logs(temp_buffer,NSLOG_RUNTIME_ERROR);

		neb_unload_module(mod,NEBMODULE_FORCE_UNLOAD,NEBMODULE_ERROR_API_VERSION);

		return ERROR;
	        }

	/* locate the initialization function */
#ifdef USE_LTDL
	mod->init_func=lt_dlsym(mod->module_handle,"nebmodule_init");
#else
	mod->init_func=dlsym(mod->module_handle,"nebmodule_init");
#endif

	/* if the init function could not be located, unload the module */
	if(mod->init_func==NULL){

		snprintf(temp_buffer,sizeof(temp_buffer),"Error: Could not locate nebmodule_init() in module '%s'.  Module will be unloaded.\n",mod->filename);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_all_logs(temp_buffer,NSLOG_RUNTIME_ERROR);

		neb_unload_module(mod,NEBMODULE_FORCE_UNLOAD,NEBMODULE_ERROR_NO_INIT);

		return ERROR;
	        }

	/* run the module's init function */
	initfunc=mod->init_func;
	result=(*initfunc)(NEBMODULE_NORMAL_LOAD,mod->args,mod);

	/* if the init function returned an error, unload the module */
	if(result!=OK){

		snprintf(temp_buffer,sizeof(temp_buffer),"Error: Function nebmodule_init() in module '%s' returned an error.  Module will be unloaded.\n",mod->filename);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		write_to_all_logs(temp_buffer,NSLOG_RUNTIME_ERROR);

		neb_unload_module(mod,NEBMODULE_FORCE_UNLOAD,NEBMODULE_ERROR_BAD_INIT);

		return ERROR;
	        }

	snprintf(temp_buffer,sizeof(temp_buffer),"Event broker module '%s' initialized successfully.\n",mod->filename);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_all_logs(temp_buffer,NSLOG_INFO_MESSAGE);

	/* locate the de-initialization function (may or may not be present) */
#ifdef USE_LTDL
	mod->deinit_func=lt_dlsym(mod->module_handle,"nebmodule_deinit");
#else
	mod->deinit_func=dlsym(mod->module_handle,"nebmodule_deinit");
#endif

#ifdef DEBUG
	printf("Module '%s' loaded with return code of '%d'\n",mod->filename,result);
	if(mod->deinit_func!=NULL)
		printf("\tnebmodule_deinit() found\n");
#endif

	return OK;
        }


/* close (unload) all modules that are currently loaded */
int neb_unload_all_modules(int flags, int reason){
	nebmodule *temp_module;

	for(temp_module=neb_module_list;temp_module;temp_module=temp_module->next){

		/* skip modules that are not loaded */
		if(temp_module->is_currently_loaded==FALSE)
			continue;

		/* skip modules that do not have a valid handle */
		if(temp_module->module_handle==NULL)
			continue;

		/* close/unload the module */
		neb_unload_module(temp_module,flags,reason);
	        }

	return OK;
        }



/* close (unload) a particular module */
int neb_unload_module(nebmodule *mod, int flags, int reason){
	char temp_buffer[MAX_INPUT_BUFFER];
	int (*deinitfunc)(int,int);
	int result;

	if(mod==NULL)
		return ERROR;

#ifdef DEBUG
	printf("Attempting to unload module '%s': flags=%d, reason=%d\n",mod->filename,flags,reason);
#endif

	/* call the de-initialization function if available (and the module was initialized) */
	if(mod->deinit_func && reason!=NEBMODULE_ERROR_BAD_INIT){

		deinitfunc=mod->deinit_func;

		/* module can opt to not be unloaded */
		result=(*deinitfunc)(flags,reason);

		/* if module doesn't want to be unloaded, exit with error (unless its being forced) */
		if(result!=OK && !(flags & NEBMODULE_FORCE_UNLOAD))
			return ERROR;
	        }

	/* unload the module */
#ifdef USE_LTDL
	result=lt_dlclose(mod->module_handle);
#else
	result=dlclose(mod->module_handle);
#endif

	/* mark the module as being unloaded */
	mod->is_currently_loaded=FALSE;

#ifdef DEBUG
	printf("Module '%s' unloaded successfully.\n",mod->filename);
#endif

	snprintf(temp_buffer,sizeof(temp_buffer),"Event broker module '%s' deinitialized successfully.\n",mod->filename);
	temp_buffer[sizeof(temp_buffer)-1]='\x0';
	write_to_all_logs(temp_buffer,NSLOG_INFO_MESSAGE);

	return OK;
        }




/****************************************************************************/
/****************************************************************************/
/* INFO FUNCTIONS                                                           */
/****************************************************************************/
/****************************************************************************/

/* sets module information */
int neb_set_module_info(void *handle, int type, char *data){
	nebmodule *mod;

	if(handle==NULL)
		return NEBERROR_NOMODULE;

	/* check type */
	if(type<0 || type>=NEBMODULE_MODINFO_NUMITEMS)
		return NEBERROR_MODINFOBOUNDS;

	/* get the module */
	mod=(nebmodule *)handle;

	/* free any previously allocated memory */
	free(mod->info[type]);

	/* allocate memory for the new data */
	if(data==NULL)
		mod->info[type]=NULL;
	else{
		mod->info[type]=strdup(data);
		if(mod->info[type]==NULL)
			return NEBERROR_NOMEM;
	        }

	return OK;
        }



/****************************************************************************/
/****************************************************************************/
/* CALLBACK FUNCTIONS                                                       */
/****************************************************************************/
/****************************************************************************/

/* allows a module to register a callback function */
int neb_register_callback(int callback_type, int priority, int (*callback_func)(int,void *)){
	nebcallback *new_callback;
	nebcallback *temp_callback;
	nebcallback *last_callback;

	if(callback_func==NULL)
		return NEBERROR_NOCALLBACKFUNC;

	if(neb_callback_list==NULL)
		return NEBERROR_NOCALLBACKLIST;

	/* make sure the callback type is within bounds */
	if(callback_type<0 || callback_type>=NEBCALLBACK_NUMITEMS)
		return NEBERROR_CALLBACKBOUNDS;

	/* allocate memory */
	new_callback=(nebcallback *)malloc(sizeof(nebcallback));
	if(new_callback==NULL)
		return NEBERROR_NOMEM;
	
	new_callback->priority=priority;
	new_callback->callback_func=(void *)callback_func;

	/* add new function to callback list, sorted by priority (first come, first served for same priority) */
	new_callback->next=NULL;
	if(neb_callback_list[callback_type]==NULL)
		neb_callback_list[callback_type]=new_callback;
	else{
		last_callback=NULL;
		for(temp_callback=neb_callback_list[callback_type];temp_callback!=NULL;temp_callback=temp_callback->next){
			if(temp_callback->priority>new_callback->priority)
				break;
			last_callback=temp_callback;
	                }
		if(last_callback==NULL)
			neb_callback_list[callback_type]=new_callback;
		else{
			if(temp_callback==NULL)
				last_callback->next=new_callback;
			else{
				new_callback->next=temp_callback;
				last_callback->next=new_callback;
			        }
		        }
	        }

	return OK;
        }


/* allows a module to deregister a callback function */
int neb_deregister_callback(int callback_type, int (*callback_func)(int,void *)){
	nebcallback *temp_callback;
	nebcallback *last_callback;
	nebcallback *next_callback=NULL;

	if(callback_func==NULL)
		return NEBERROR_NOCALLBACKFUNC;

	if(neb_callback_list==NULL)
		return NEBERROR_NOCALLBACKLIST;

	/* make sure the callback type is within bounds */
	if(callback_type<0 || callback_type>=NEBCALLBACK_NUMITEMS)
		return NEBERROR_CALLBACKBOUNDS;

	/* find the callback to remove */
	for(temp_callback=last_callback=neb_callback_list[callback_type];temp_callback!=NULL;temp_callback=next_callback){
		next_callback=temp_callback->next;

		/* we found it */
		if(temp_callback->callback_func==callback_func)
			break;

		last_callback=temp_callback;
	        }

	/* we couldn't find the callback */
	if(temp_callback==NULL)
		return NEBERROR_CALLBACKNOTFOUND;

	else{
		last_callback->next=next_callback;
		free(temp_callback);
	        }
	
	return OK;
        }



/* make callbacks to modules */
int neb_make_callbacks(int callback_type, void *data){
	nebcallback *temp_callback;
	int (*callbackfunc)(int,void *);
	int cbresult;

	/* make sure callback list is initialized */
	if(neb_callback_list==NULL)
		return ERROR;

	/* make sure the callback type is within bounds */
	if(callback_type<0 || callback_type>=NEBCALLBACK_NUMITEMS)
		return ERROR;

	/* make the callbacks... */
	for(temp_callback=neb_callback_list[callback_type];temp_callback!=NULL;temp_callback=temp_callback->next){
		callbackfunc=temp_callback->callback_func;
		cbresult=callbackfunc(callback_type,data);
#ifdef DEBUG
		printf("Callback type %d resulted in return code of %d\n",callback_type,cbresult);
#endif
	        }

	return OK;
        }



/* initialize callback list */
int neb_init_callback_list(void){
	int x;

	/* allocate memory for the callback list */
	neb_callback_list=(nebcallback **)malloc(NEBCALLBACK_NUMITEMS*sizeof(nebcallback *));
	if(neb_callback_list==NULL)
		return ERROR;

	/* initialize list pointers */
	for(x=0;x<NEBCALLBACK_NUMITEMS;x++)
		neb_callback_list[x]=NULL;

	return OK;
        }


/* free memory allocated to callback list */
int neb_free_callback_list(void){
	nebcallback *temp_callback;
	nebcallback *next_callback;
	int x;

	for(x=0;x<NEBCALLBACK_NUMITEMS;x++){

		for(temp_callback=neb_callback_list[x];temp_callback!=NULL;temp_callback=next_callback){
			next_callback=temp_callback->next;
			free(temp_callback);
			temp_callback=next_callback;
	                }

		neb_callback_list[x]=NULL;
	        }

	free(neb_callback_list);
	neb_callback_list=NULL;

	return OK;
        }

