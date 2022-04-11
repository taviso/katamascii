#include <glib.h>
#include <stdio.h>

static FILE *logfile;

static void debug_log_handler(const gchar *log_domain,
                              GLogLevelFlags log_level,
                              const gchar *message,
                              gpointer user_data)
{
    fprintf(logfile, "%s\n", message);
}

static void __attribute__((constructor)) init_debug_log()
{
    logfile = fopen("debug.log", "w");

    g_log_set_handler(NULL, G_LOG_LEVEL_DEBUG, debug_log_handler, NULL);
}

static void __attribute__((destructor)) fini_debug_log()
{
    fclose(logfile);
}
