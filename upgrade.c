#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "upgrade.h"

#include "g_log.h"

static const char *_goblin_file;
static const char *_goblin_md5_file;
void Set_goblin_file(const char * goblin, const char *goblin_md5)
{
    _goblin_file=goblin;
    _goblin_md5_file=goblin_md5;
}

const char * goblin_path = "/root";
const char * goblin_temp_path = "/root/goblin_temp";
const char * goblin_temp_file = "/root/goblin_temp/goblin";
const char * goblin_temp_md5_file = "/root/goblin_temp/goblin.md5";
const char * md5s ="md5sum -c goblin.md5";

int Check_MD5()
{
	chdir(goblin_temp_path);
	int r =system(md5s);
	chdir(goblin_path);
	return r;
}

void Check_upgrade()
{
    log_printf("Checking upgrade files...");
    
	if(access(goblin_temp_file, F_OK) != 0)
    {
        log_printf("'%s' not exist", goblin_temp_file);
        return;
    }

    if(access(goblin_temp_md5_file, F_OK) != 0)
    {
        log_printf("'%s' not exist", goblin_temp_md5_file);
        return;
    }
	
    log_printf("New upgrade files exsit. Checking MD5...");
    if(Check_MD5()!=0)
	{
	    log_printf("MD5 not matched");
		return;
	}
    log_printf("MD5 matched");
    log_printf("Moving files...");

    char name_new[1024];

    // goblin => goblin.old
    strcpy(name_new, _goblin_file);
    strcat(name_new, ".old");
    if(rename(_goblin_file,name_new)==-1)
    {
        log_printf("Move '%s' to '%s' failed",_goblin_file,name_new);
        return;
    }
    
    // temp/goblin => goblin
    if(rename(goblin_temp_file,_goblin_file)==-1)
    {
        log_printf("Move '%s' to '%s' failed",goblin_temp_file,_goblin_file);
        return;
    }

    // goblin.md5 => goblin.md5.old
    strcpy(name_new, _goblin_md5_file);
    strcat(name_new, ".old");
    if(rename(_goblin_md5_file,name_new)==-1)
    {
        log_printf("Move '%s' to '%s' failed",_goblin_md5_file,name_new);
        return;
    }
    
    // temp/goblin.md5 => goblin.md5
    if(rename(goblin_temp_md5_file,_goblin_md5_file)==-1)
    {
        log_printf("Move '%s' to '%s' failed",goblin_temp_md5_file,_goblin_md5_file);
        return;
    }

	log_printf("Upgrade finished");
	return;
}

