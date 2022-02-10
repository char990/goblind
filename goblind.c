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

#include "g_log.h"
#include "upgrade.h"
#include "static_files.h"

#define NO_DATA_CNT_MAX 10
#define FIFO_ERROR_CNT_MAX 10

//#define ENABLE_DAEMON

static void Skeleton_daemon()
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/root");

    /* Close all open file descriptors */
    for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--)
    {
        close(x);
    }
}

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
int fifo_fd;
charFifo_t Read_fifo()
{
    char data;
    int r = FIFO_NO_CHAR;
    int rd;
    while (1)
    {
        rd = read(fifo_fd, &data, 1);
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

/************************* goblind control function ****************************/
void Kill_goblin()
{
    char pidline[1024];
    char *pid = pidline;
    FILE *fp = popen("pidof goblin", "r");
    if (fp == NULL)
    {
        log_printf("Error when run 'pidof goblin'");
        return;
    }
    int len = fread(pidline, 1, 1023, fp);
    if (len > 0)
    {
        pidline[len] = 0;
        while (1)
        {
            char *tail;
            int pid_no;
            while (isspace(*pid)) // Skip whitespace
            {
                pid++;
            }
            if (*pid == 0) //detect the end
            {
                break;
            }
            pid_no = strtol(pid, &tail, 10);
            if (pid == tail || pid_no <= 0) // no valid data
            {
                break;
            }
            kill(pid_no, SIGTERM);
            pid = tail;
        }
        log_printf("goblin killed");
    }
    else
    {
        log_printf("goblin is not running");
    }
    pclose(fp);
}

void Restart_goblin()
{
    Kill_goblin();
    Read_fifo(); // clear fifo
    Check_upgrade();
    if (access(goblin_file, F_OK) != 0)
    {
        log_printf("Can't find '%s'. Recover from '%s'", goblin_file, goblin_file_old);
        if (access(goblin_file_old, F_OK) != 0)
        {
            log_printf("Can't find '%s'. Exit.", goblin_file_old);
            exit(3);
        }
        if (rename(goblin_file_old, goblin_file) == -1)
        {
            log_printf("Move '%s' to '%s' failed", goblin_file_old, goblin_file);
            exit(3);
        }
        rename(goblin_md5_file_old, goblin_md5_file);
    }
    chmod(goblin_file, S_IRWXU);
    log_printf("Start to run '%s'", goblin_file);
    system(goblin_file);
    sleep(3);
}

void Reboot_system()
{
    sync();
    reboot(RB_AUTOBOOT);
}

// --------------------------------------------
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf(
            "Usage: goblind 0|1\n"
            "       0: daemon mode\n"
            "       1: console mode\n");
        exit(1);
    }
    if (argc == 2 && atoi(argv[1]) == 0)
    {
        Skeleton_daemon();
    }
    if (Set_log_file(goblind_log_file) != 0)
    {
        exit(1);
    }
    log_printf("====================================");
    log_printf("goblind (ver 1.0.0) starts");

#define T_D_A 256
    int tda = strlen(goblin_temp_path);
    if (tda > (T_D_A - 2))
    {
        log_printf("Path temp_dir '%s' is too long (>%d)", goblin_temp_path, T_D_A - 2);
        exit(2);
    }
    char temp_dir_access[T_D_A];
    memcpy(temp_dir_access, goblin_temp_path, tda);
    temp_dir_access[tda] = '/';
    temp_dir_access[tda + 1] = '\0';
    if (access(temp_dir_access, F_OK) != 0)
    {
        if (mkdir(goblin_temp_path, S_IRWXU) != 0)
        {
            log_printf("goblind could not mkdir '%s': errno=%d", goblin_temp_path, errno);
            exit(2);
        }
    }
    // create fifo
    if (access(fifo_file, F_OK) != 0)
    {
        if (mkfifo(fifo_file, 0600) != 0)
        {
            log_printf("goblind could not create '%s': errno=%d", fifo_file, errno);
            exit(2);
        }
    }

    fifo_fd = open(fifo_file, O_RDONLY | O_NONBLOCK);
    if (fifo_fd < 0)
    {
        return 3;
    }

    // (Re)start "goblin"
    Restart_goblin();

    // main loop
    int nodata_cnt = 0;
    charFifo_t charF;
    while (1)
    {
        sleep(1);
        charF = Read_fifo();
        switch (charF)
        {
        default: //case FIFO_NO_CHAR:
            if (nodata_cnt < NO_DATA_CNT_MAX)
            {
                nodata_cnt++;
            }
            if (nodata_cnt == NO_DATA_CNT_MAX)
            { // goblin halt or killed, restart
                log_printf("fifo NO_DATA timeout, restart '%s'", goblin_file);
                Restart_goblin();
                nodata_cnt = 0;
            }
            break;
        case FIFO_0:
            nodata_cnt = 0;
            //WriteLog("Received '0' => Do nothing");
            break;
        case FIFO_1:
            nodata_cnt = 0;
            log_printf("Received '1' => Restar '%s'", goblin_file);
            Restart_goblin();
            break;
        case FIFO_2:
            nodata_cnt = 0;
            log_printf("Received '2' => Reboot system");
            close(fifo_fd);
            Reboot_system();
            while (1)
                ;
            break;
        }
    }

    // should not be here, something wrong
    log_printf("!!! Something wrong !!! 'goblind' terminated");
    close(fifo_fd);
    return 4;
}
