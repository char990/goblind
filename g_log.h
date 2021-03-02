#ifndef __G_LOG_H__

#define  __G_LOG_H__

int Set_log_file(const char * log_f);

int log_puts(char * str);

int log_printf(const char * fmt, ...);

#endif // !__G_LOG_H__
