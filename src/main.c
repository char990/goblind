#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/reboot.h>
#include <time.h>

#include "g_log.h"
#include "upgrade.h"
#include "static_files.h"

#define NO_DATA_CNT_MAX 30

#define VERSION "2.0"

#ifdef USE_FIFO
/************************* fifo ****************************/
// Open and Read fifo
// return:
//      -1: no char
//      >=0: last char in fifo
enum charFifo
{
    FIFO_NO_CHAR = -1,
    FIFO_0 = '0',
    FIFO_1 = '1',
    FIFO_2 = '2'
};

typedef enum charFifo charFifo_t;

static int fifo_fd;

charFifo_t Read_fifo()
{
    int r = FIFO_NO_CHAR;
    while (1)
    {
        char data;
        int rd = read(fifo_fd, &data, 1);
        if (rd == 1)
        {
            if (data > r)
            {
                r = data;
            }
        }
        else // no data
        {
            break;
        }
    }
    return r;
}
#endif

/************************* goblind control function ****************************/
void Kill_goblin()
{
    putlog("Killing goblin ...");
    while (1)
    {
        FILE *fp = popen("pidof goblin", "r");
        if (fp == NULL)
        {
            putlog("Error when run 'pidof goblin'");
            return;
        }
        int pid_no;
        int n = fscanf(fp, "%d", &pid_no);
        pclose(fp);
        if (n == 1)
        {
            kill(pid_no, SIGTERM);
        }
        else
        {
            putlog("Done");
            return;
        }
    }
}

int Restart_goblin()
{
    Kill_goblin();
#ifdef USE_FIFO
    Read_fifo(); // clear fifo
#endif
    Check_upgrade();
    if (access(goblin_file, F_OK) != 0)
    {
        putlog("Can't find '%s'. Recover from '%s'", goblin_file, goblin_file_old);
        const char *recover_file[][2] = {
            {goblin_file_old, goblin_file},
            {goblin_md5_file_old, goblin_md5_file}};
        for (int i = 0; i < 2; i++)
        {
            if (access(recover_file[i][0], F_OK) != 0)
            {
                putlog("Can't find '%s'. Exit.", recover_file[i][0]);
                exit(3);
            }
            if (rename(recover_file[i][0], recover_file[i][1]) == -1)
            {
                putlog("Move '%s' to '%s' failed", recover_file[i][0], recover_file[i][1]);
                exit(3);
            }
        }
    }
    chmod(goblin_file, S_IRWXU);
    putlog("Start to run '%s'", goblin_file);
    char buf[256];
#ifdef USE_FIFO
    sprintf(buf, "screen -dmS GC20 %s", goblin_file);
#else
    sprintf(buf, "%s", goblin_file);
#endif
    return system(buf);
}

void Reboot_system()
{
    sync();
    reboot(RB_AUTOBOOT); // Perform a hard reset
}

#ifdef DEBUG
const char *MAKE = "Debug";
#else
const char *MAKE = "Release";
#endif

#ifndef __BUILDTIME__
#define __BUILDTIME__ ("UTC:" __DATE__ " " __TIME__)
#endif

void PrintVersion()
{
    char sbuf[256];
    int len = snprintf(sbuf, 255, "* %s version: %s. Build time: %s *",
                       MAKE, VERSION, __BUILDTIME__); // __BUILDTIME__ is defined in Makefile
    char buf[256];
    memset(buf, '*', len);
    buf[len] = '\0';
    putlog("%s", buf);
    putlog("%s", sbuf);
    putlog("%s", buf);
}

// --------------------------------------------
int goblind()
{
    if (Set_log_file(goblind_log_file) != 0)
    {
        return 1;
    }
    PrintVersion();

    if (access(goblin_temp_path, F_OK) != 0)
    {
        if (mkdir(goblin_temp_path, S_IRWXU) != 0)
        {
            putlog("goblind could not mkdir '%s': errno=%d", goblin_temp_path, errno);
            return 2;
        }
    }
#ifdef USE_FIFO
    // create fifo
    if (access(fifo_file, F_OK) != 0)
    {
        if (mkfifo(fifo_file, 0600) != 0)
        {
            putlog("goblind could not create '%s': errno=%d", fifo_file, errno);
            return 3;
        }
    }

    fifo_fd = open(fifo_file, O_RDONLY | O_NONBLOCK);
    if (fifo_fd < 0)
    {
        return 4;
    }
#endif
    // main loop
    while (1)
    {
        // (Re)start "goblin"
        int x = Restart_goblin();
        if (x == 0xFB || x == 0xFB00)
        {
            putlog("Received 0xFB => Reboot system");
            while (1)
            {
                Reboot_system();
                sleep(10);
            }
        }
        else
        {
            sleep(2);
            putlog("Received 0x%04X => Restart '%s'", x, goblin_file);
        }
    }

    // should not be here, something wrong
    putlog("!!! Something wrong !!! 'goblind' terminated");
#ifdef USE_FIFO
    close(fifo_fd);
#endif
    return 5;
}

int main()
{
    int r = goblind();
    // goblind terminated with error code.
    system("sync");
    return r;
}
