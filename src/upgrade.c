#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "static_files.h"

#include "upgrade.h"

#include "g_log.h"

int Check_MD5()
{
    chdir(goblin_temp_path);
    char cmd[256];
    sprintf(cmd, "md5sum -c %s", goblin_md5_file);
    int r = system(cmd);
    chdir("..");
    return r;
}

void Check_upgrade()
{
    putlog("Checking upgrade files...");

    if (access(goblin_temp_file, F_OK) != 0)
    {
        putlog("'%s' not exist", goblin_temp_file);
        return;
    }

    if (access(goblin_temp_md5_file, F_OK) != 0)
    {
        putlog("'%s' not exist", goblin_temp_md5_file);
        return;
    }

    putlog("New upgrade files exsit. Checking MD5...");
    if (Check_MD5() != 0)
    {
        putlog("MD5 not matched");
        return;
    }
    putlog("MD5 matched");
    putlog("Moving files...");

    const char *rename_files[][2] = {
        {goblin_file, goblin_file_old},
        {goblin_md5_file, goblin_md5_file_old},
        {goblin_temp_file, goblin_file},
        {goblin_temp_md5_file, goblin_md5_file}};

    for (int i = 0; i < 4; i++)
    {
        if (rename(rename_files[i][0], rename_files[i][1]) == -1)
        {
            putlog("Move '%s' to '%s' failed", rename_files[i][0], rename_files[i][1]);
            return;
        }
    }
    putlog("Upgrade finished");
    return;
}
