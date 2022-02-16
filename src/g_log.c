#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "g_log.h"

static const char *_goblin_config = "./config";
static const char *_goblin_user = "UciUser";

static const char *goblind_log_file;

int Set_log_file(const char *file)
{
    goblind_log_file = file;
    FILE *fd = fopen(goblind_log_file, "a");
    if (fd == NULL)
    {
        exit(-1);
    }
    fputc('\n', fd);
    fclose(fd);
    return 0;
}

#define NUMBER_OF_TZ 9
typedef struct timezone_t
{
    const char *city;
    const char *TZ;
    int gmt_offset;
    int dst;
    int twilight[2][2][2]; // minute from standard time 0:00:00
} timezone_t;

const timezone_t tz_au[NUMBER_OF_TZ] = {
    {"Perth", "AWST-8", 8 * 60 * 60, 0, {6 * 60 + 49, 7 * 60 + 16, 17 * 60 + 20, 17 * 60 + 47, 4 * 60 + 38, 5 * 60 + 7, 19 * 60 + 21, 19 * 60 + 50}},                                    // 0 Perth
    {"Brisbane", "AEST-10", 10 * 60 * 60, 0, {6 * 60 + 12, 6 * 60 + 37, 17 * 60 + 2, 17 * 60 + 27, 4 * 60 + 23, 4 * 60 + 49, 18 * 60 + 42, 19 * 60 + 9}},                                // 1 Brisbane
    {"Darwin", "ACST-9:30", (9 * 60 + 30) * 60, 0, {6 * 60 + 43, 7 * 60 + 6, 18 * 60 + 30, 18 * 60 + 53, 5 * 60 + 55, 6 * 60 + 19, 19 * 60 + 10, 19 * 60 + 33}},                         // 2 Darwin
    {"Adelaide", "ACST-9:30ACDT,M10.1.0,M4.1.0/3", (9 * 60 + 30) * 60, 1, {6 * 60 + 55, 7 * 60 + 23, 17 * 60 + 12, 17 * 60 + 40, 4 * 60 + 29, 4 * 60 + 58, 19 * 60 + 29, 19 * 60 + 59}}, // 3 Adelaide
    {"Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3", 10 * 60 * 60, 1, {6 * 60 + 32, 7 * 60 + 0, 16 * 60 + 54, 17 * 60 + 21, 4 * 60 + 11, 4 * 60 + 40, 19 * 60 + 6, 19 * 60 + 35}},             // 4 Sydney
    {"Canberra", "AEST-10AEDT,M10.1.0,M4.1.0/3", 10 * 60 * 60, 1, {6 * 60 + 44, 7 * 60 + 12, 16 * 60 + 58, 17 * 60 + 27, 4 * 60 + 15, 4 * 60 + 45, 19 * 60 + 18, 19 * 60 + 48}},         // 5 Canberra
    {"Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3", 10 * 60 * 60, 1, {7 * 60 + 6, 7 * 60 + 35, 17 * 60 + 8, 17 * 60 + 38, 4 * 60 + 23, 4 * 60 + 55, 19 * 60 + 42, 20 * 60 + 13}},          // 6 Melbourne
    {"Hobart", "AEST-10AEDT,M10.1.0,M4.1.0/3", 10 * 60 * 60, 1, {7 * 60 + 9, 7 * 60 + 41, 16 * 60 + 42, 17 * 60 + 15, 3 * 60 + 52, 4 * 60 + 27, 19 * 60 + 48, 20 * 60 + 23}},            // 7 Hobart
    {"Eucla", "ACWST-8:45", (8 * 60 + 45) * 60, 0, {6 * 60 + 41, 7 * 60 + 8, 17 * 60 + 13, 17 * 60 + 40, 4 * 60 + 32, 5 * 60 + 0, 19 * 60 + 14, 19 * 60 + 42}}                           // 8 Eucla
};

/************************* goblind log ****************************/
void Refresh_TZ()
{
    static time_t goblin_user_time = 0;
    struct stat st_buf;
    char buf[256];
    buf[255] = 0;
    snprintf(buf, 255, "%s/%s", _goblin_config, _goblin_user);
    int r = stat(buf, &st_buf);
    if (r != 0)
    { // can't get file attribe, set as default
        setenv("TZ", tz_au[4].TZ, 1);
        return;
    }
    if (goblin_user_time == st_buf.st_mtime)
    { // if goblin_user was not changed, do nothing
        return;
    }
    // goblin_user was changed, reload TZ
    goblin_user_time = st_buf.st_mtime;
    snprintf(buf, 255, "uci -c %s get %s.user_cfg.TZ", _goblin_config, _goblin_user);
    FILE *fd = popen(buf, "r");
    int city_i = 4;
    int cnt = 0;
    char city[10];
    memset(city, 0, 10);
    if (fd != NULL)
    {
        while (1)
        {
            int c = fgetc(fd);
            if (c == EOF)
            {
                break;
            }
            if (cnt == 0 && isspace(c)) // Skip whitespace
            {
                continue;
            }
            if (isalpha(c))
            {
                city[cnt++] = c;
                if (cnt == 9)
                    break;
            }
            else
            {
                if (cnt > 0)
                    break;
            }
        }
        pclose(fd);
        for (int i = 0; i < NUMBER_OF_TZ; i++)
        {
            if (strcmp(city, tz_au[i].city) == 0)
            {
                city_i = i;
                break;
            }
        }
    }
    setenv("TZ", tz_au[city_i].TZ, 1);
    tzset();
}

char *Get_current_time()
{
    static char time_buf[32];
    static time_t st;
    time_t t = time(NULL);
    if (t != st)
    {
        Refresh_TZ();
        struct tm localt;
        localtime_r(&t, &localt);
        int len = snprintf(time_buf, 31, "[%2d/%02d/%04d %2d:%02d:%02d]",
                           localt.tm_mday, localt.tm_mon + 1, localt.tm_year + 1900,
                           localt.tm_hour, localt.tm_min, localt.tm_sec);
    }
    return time_buf;
}

int log_puts(char *str)
{
    FILE *log_fd = fopen(goblind_log_file, "a");
    if (log_fd == NULL)
    {
        return -1;
    }
    char log_buf[1024];
    int len = sprintf(log_buf, "%s %s\n", Get_current_time(), str);
    fwrite(log_buf, len, 1, log_fd);
    fclose(log_fd);
#ifdef DEBUG
    printf("%s", log_buf);
#endif
    return len;
}

int putlog(const char *fmt, ...)
{
    char log_buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log_buf, 1024 - 1, fmt, ap);
    int len = log_puts(log_buf);
    va_end(ap);
    return len;
}
