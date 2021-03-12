#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "static_files.h"

#include "upgrade.h"

#include "g_log.h"

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

    if(rename(goblin_file,goblin_file_old)==-1)
    {
        log_printf("Move '%s' to '%s' failed",goblin_file,goblin_file_old);
        return;
    }
    
    // temp/goblin => goblin
    if(rename(goblin_temp_file,goblin_file)==-1)
    {
        log_printf("Move '%s' to '%s' failed",goblin_temp_file,goblin_file);
        return;
    }

    // goblin.md5 => goblin.md5.old
    if(rename(goblin_md5_file,goblin_md5_file_old)==-1)
    {
        log_printf("Move '%s' to '%s' failed",goblin_md5_file,goblin_md5_file_old);
        return;
    }
    
    // temp/goblin.md5 => goblin.md5
    if(rename(goblin_temp_md5_file,goblin_md5_file)==-1)
    {
        log_printf("Move '%s' to '%s' failed",goblin_temp_md5_file,goblin_md5_file);
        return;
    }

	log_printf("Upgrade finished");
	return;
}

