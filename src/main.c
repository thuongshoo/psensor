/*
 * Copyright (C) 2010-2016 jeanfi@gmail.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#define _GNU_SOURCE
#include <bool.h>

#include <locale.h>

#include <sched.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <pmutex.h>

#include <gtk/gtk.h>



#include <amd.h>
#include <cfg.h>
#include <graph.h>
#include <hdd.h>
#include <lmsensor.h>
#include <notify_cmd.h>
#include <nvidia.h>
#include <pgtop2.h>
#include <psensor.h>
#include <pudisks2.h>
#include <rsensor.h>
#include <slog.h>
#include <ui.h>
#include <ui_appindicator.h>
#include <ui_color.h>
#include <ui_graph.h>
#include <ui_notify.h>
#include <ui_pref.h>
#include <ui_sensorlist.h>
#include <ui_status.h>
#include <ui_unity.h>
#include "copyright.h"

#define ONE_SECOND 1000U
static const char *program_name;

static void print_version(void)
{
    printf("%s %s\n", PACKAGE_NAME, VERSION);
    printf(_(PSENSOR_COPYRIGHT_CLI),
           PSENSOR_ORIGINAL_YEARS, PSENSOR_ORIGINAL_EMAIL,
           PSENSOR_CURRENT_YEARS, PSENSOR_CURRENT_EMAIL);
}

static void print_help(void)
{
    printf(_("Usage: %s [OPTION]...\n"), program_name);

    puts(_("Psensor is a GTK+ application for monitoring hardware sensors, "
           "including temperatures and fan speeds."));

    puts("");
    puts(_("Options:"));
    puts(_("  -h, --help          display this help and exit\n"
           "  -v, --version       display version information and exit"));

    puts("");

    puts(_(
        "  -u, --url=URL       the URL of the psensor-server,\n"
        "                      example: http://hostname:3131"));
    puts(_(
        "  -n, --new-instance  force the creation of a new Psensor application"));
    puts("");

    puts(_("  -d, --debug=LEVEL   "
           "set the debug level, integer between 0 and 3"));

    puts("");

    printf(_("Report bugs to: %s\n"), PACKAGE_BUGREPORT);
    puts("");
    printf(_("%s home page: <%s>\n"), PACKAGE_NAME, PACKAGE_URL);
}

static int update_measures_lock(pthread_mutex_t *m)
{
    return pmutex_lock(m);
}
static int update_measures_unlock(pthread_mutex_t *m)
{
    return pmutex_unlock(m);
}

/*
 * Updates the size of the sensor values if different than the
 * configuration.
 */
static void
update_psensor_values_size(Psensor **sensors, const struct config *cfg)
{
    for (Psensor **cur = sensors; *cur; cur++)
    {
        Psensor *s = *cur;
        // User can modify graph_monitoring_duration and sensor_update_interval in UI
        // This triggers recalculation of sensor_values_max_length in configuration
        if (s->measures_size != cfg->sensor_values_max_length)
            psensor_values_resize(s,
                                  cfg->sensor_values_max_length);
        else
        {
            //Currently all sensors share the same buffer size so no need to check all
            return;
        }
    }
}

static void *update_measures(void *data)
{
    struct ui_psensor *ui = (struct ui_psensor *)data;
    Pconfig *cfg = ui->config;

    pthread_setname_np(pthread_self(), "update_measures");
    while (1)
    {
        update_measures_lock(&ui->sensors_mutex);

        Psensor **sensors = ui->sensors;
        if (!sensors)
            pthread_exit(nullptr);

        update_psensor_values_size(sensors, cfg);

        size_t count = 0;
        count += lmsensor_psensor_list_update(sensors);

        remote_psensor_list_update(sensors);
        nvidia_psensor_list_update(sensors);
        amd_psensor_list_update(sensors);
        udisks2_psensor_list_update(sensors);
        gtop2_psensor_list_update(sensors);
        atasmart_psensor_list_update(sensors);
        hddtemp_psensor_list_update(sensors);

        psensor_log_measures(sensors);

        if (count > 0)
        {
            cfg->is_new_data = true;
        }
        unsigned int period = (unsigned int)cfg->sensor_update_interval;

        update_measures_unlock(&ui->sensors_mutex);

        sleep(period);
    }
}

static void indicators_update(struct ui_psensor *ui)
{
    bool attention = false;
    Psensor **ss = ui->sensors;
    while (*ss)
    {
        const Psensor *s = *ss;

        if (s->alarm_raised && config_get_sensor_alarm_enabled(s->id))
        {
            attention = true;
            break;
        }

        ss++;
    }

    if (is_appindicator_supported())
        ui_appindicator_update(ui, attention);

    if (is_status_supported())
        ui_status_update(ui, attention);
}

static gboolean ui_refresh_thread(gpointer data);
static void queue_another_execution(struct ui_psensor *ui)
{
    g_timeout_add(ONE_SECOND * ui->graph_update_interval,
                  ui_refresh_thread, ui);
}

// This idle function ensures the call is made from the main thread
static gboolean queue_redraw_idle(GtkWidget *widget)
{
    gtk_widget_queue_draw(widget);
    return G_SOURCE_REMOVE; // Run only once
}
static int ui_refresh_thread_lock(pthread_mutex_t *m)
{
    return pmutex_lock(m);
}
static int ui_refresh_thread_unlock(pthread_mutex_t *m)
{
    return pmutex_unlock(m);
}
static gboolean ui_refresh_thread(gpointer data)
{
    struct ui_psensor *ui = (struct ui_psensor *)data;

    pthread_setname_np(pthread_self(), "ui_refresh_thread");

    gboolean ret = TRUE;
    const Pconfig *config = ui->config;

    if (!config->is_new_data)
    {
        queue_another_execution(ui);
        return FALSE;
    }
    ////
    ui_refresh_thread_lock(&ui->sensors_mutex);

    if (is_appindicator_supported() || is_status_supported())
        indicators_update(ui);

    ui_unity_launcher_entry_update(ui->sensors);

    if (ui->graph_update_interval != config->graph_update_interval)
    {
        ui->graph_update_interval = config->graph_update_interval;
        ret = FALSE;
    }
    
    ui_refresh_thread_unlock(&ui->sensors_mutex);
    ////
    if (!should_update_ui(ui)) {
        // App không active, tăng counter
        struct config *cfg = ui->config;
        MyWidgetData *widget_data = &cfg->widget_data;
        widget_data->skipped_redraws++;
        
        queue_another_execution(ui);
        return G_SOURCE_REMOVE;
    }

    ui_sensorlist_update(ui, false);

    // ===== KHI QUAY LẠI, NẾU CÓ SKIPPED_REDRAWS > 0 =====
    struct config *cfg = ui->config;
    MyWidgetData *widget_data = &cfg->widget_data;
    
    if (widget_data->skipped_redraws > 0) {
        // Bỏ qua tất cả các lần dịch đã bỏ qua, vẽ lại toàn bộ
        g_print("Recovering from %lu skipped redraws\n", widget_data->skipped_redraws);
        
        // Force full redraw
        widget_data->cache_valid = FALSE;
        widget_data->background_valid = FALSE;
        widget_data->shift_count = 0;
        widget_data->skipped_redraws = 0;
        
        // Reset last_values buffer
        if (widget_data->last_values && widget_data->last_sensors_count > 0) {
            for (int i = 0; i < widget_data->last_sensors_count; i++) {
                widget_data->last_values[i] = UNKNOWN_DOUBLE_VALUE;
            }
        }
    }
    // Instead of direct drawing, request a widget redraw
    g_idle_add((GSourceFunc)queue_redraw_idle,
               ui_get_graph_widget());

    if (ret == FALSE)
        queue_another_execution(ui);

    return ret;
}

static void cb_alarm_raised(Psensor *sensor, void *data)
{
    if (config_get_sensor_alarm_enabled(sensor->id))
    {
        ui_notify(sensor, (struct ui_psensor *)data);
        notify_cmd(sensor);
    }
}

static void
associate_cb_alarm_raised(Psensor **sensors, struct ui_psensor *ui)
{
    bool ret;
    Psensor *s;
    double high_temp;

    high_temp = config_get_default_high_threshold_temperature();

    while (sensors && *sensors)
    {
        s = *sensors;

        s->cb_alarm_raised = cb_alarm_raised;
        s->cb_alarm_raised_data = ui;

        ret = config_get_sensor_alarm_high_threshold(s->id, &s->alarm_high_threshold);

        if (!ret)
        {
            if (s->max == UNKNOWN_DOUBLE_VALUE)
            {
                if (s->type & SENSOR_TYPE_TEMP)
                    s->alarm_high_threshold = high_temp;
            }
            else
            {
                s->alarm_high_threshold = s->max;
            }
        }

        ret = config_get_sensor_alarm_low_threshold(s->id, &s->alarm_low_threshold);

        if (!ret && s->min != UNKNOWN_DOUBLE_VALUE)
            s->alarm_low_threshold = s->min;

        sensors++;
    }
}

/**
 * @brief Associates user-defined names with sensor objects
 * 
 * This function iterates through an array of sensor pointers and replaces
 * the default sensor names (from lmsensor) with user-defined names
 * retrieved from the configuration system. If a user has configured a
 * custom name for a sensor ID, that name will replace the original.
 * 
 * @param[in,out] sensors Double pointer to an array of psensor pointers.
 *                        The array must be nullptr-terminated. The function
 *                        modifies the 'name' field of each sensor struct
 *                        if a user-defined name exists in configuration.
 * 
 * @note The function takes ownership of the allocated name string from
 *       config_get_sensor_name() and frees the original name. The sensor
 *       array itself is not modified, only the name fields of individual
 *       sensor structures.
 * 
 * @warning The original sensor names are freed when replaced. Ensure
 *          the names were dynamically allocated.
 */
static void associate_preferences(Psensor **sensors)
{
    Psensor **sensor_cur = sensors;  /* Current position in sensor array */

    /* Iterate through nullptr-terminated array of sensor pointers */
    while (*sensor_cur)
    {
        Psensor *s = *sensor_cur;  /* Current sensor being processed */

        /* Retrieve user-defined name from configuration using sensor ID */
        char *n = config_get_sensor_name(s->id);

        /* If user has configured a custom name for this sensor */
        if (n)
        {
            /* Free the original lmsensor-provided name */
            free(s->name);
            /* Replace with user-defined name */
            s->name = n;
        }

        sensor_cur++;  /* Move to next sensor in the array */
    }
}

static void log_init(void)
{
    const char *dir = get_psensor_user_dir();

    if (!dir)
        return;

    char *path;
    if (-1 == asprintf(&path, "%s/%s", dir, "log"))
        return;

    log_open(path);

    free(path);
}

static struct option long_options[] = {
    {"version", no_argument, nullptr, 'v'},
    {"help", no_argument, nullptr, 'h'},
    {"url", required_argument, nullptr, 'u'},
    {"debug", required_argument, nullptr, 'd'},
    {"new-instance", no_argument, nullptr, 'n'},
    {nullptr, 0, nullptr, 0}};

static gboolean initial_window_show(gpointer data)
{
    struct ui_psensor *ui;

    log_debug("initial_window_show()");
    ui = (struct ui_psensor *)data;

    log_debug("is_status_supported: %d", (int)is_status_supported());
    log_debug("is_appindicator_supported: %d", (int)is_appindicator_supported());
    log_debug("hide_on_startup: %d", (int)ui->config->hide_on_startup);

    if (!ui->config->hide_on_startup || (!is_appindicator_supported() && !is_status_supported()))
        ui_window_show(ui);

    ui_window_update(ui);

    return FALSE;
}

static void log_glib_info(void)
{
    log_debug("Compiled with GLib %d.%d.%d",
              GLIB_MAJOR_VERSION,
              GLIB_MINOR_VERSION,
              GLIB_MICRO_VERSION);

    log_debug("Running with GLib %d.%d.%d",
              glib_major_version,
              glib_minor_version,
              glib_micro_version);
}

static void cb_activate(GApplication *application,
                        gpointer data)
{
    ui_window_show((struct ui_psensor *)data);
}

static int cleanup_lock(pthread_mutex_t *m)
{
    return pmutex_lock(m);
}
static int cleanup_unlock(pthread_mutex_t *m)
{
    return pmutex_unlock(m);
}
/*
 * Release memory for Valgrind.
 */
static void cleanup(struct ui_psensor *ui)
{
    cleanup_lock(&ui->sensors_mutex);

    log_debug("Cleanup...");

    nvidia_cleanup();
    amd_cleanup();
    rsensor_cleanup();
    if (config_is_lmsensor_enabled())
        lmsensor_cleanup();

    psensor_list_free(ui->sensors);
    ui->sensors = nullptr;

    ui_appindicator_cleanup();

    ui_status_cleanup();
    ui_graph_cleanup(ui);

    cleanup_unlock(&ui->sensors_mutex);

    config_cleanup();

    free(ui->config->graph_fgcolor);
    free(ui->config->graph_bgcolor);
    free(ui->config);

    log_debug("Cleanup done, closing log");
}

/*
 * Creates the list of sensors.
 *
 * 'url': remote psensor server url, null for local monitoring.
 */
static Psensor **create_sensors_list(const char *url, const struct config *config)
{
    Psensor **sensors;

    if (url)
    {
        if (rsensor_is_supported())
        {
            rsensor_init();
            const uint32_t measures_len = 600;
            sensors = get_remote_sensors(url, measures_len);
        }
        else
        {
            log_err(_("Psensor has not been compiled with remote "
                      "sensor support."));
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        sensors = (Psensor **)calloc(1, sizeof(Psensor *));
        if (sensors == nullptr)
            return nullptr;

        const unsigned int measures_len = config->sensor_values_max_length;
        if (config_is_lmsensor_enabled())
            lmsensor_psensor_list_append(&sensors, measures_len);

        if (config_is_hddtemp_enabled())
            hddtemp_psensor_list_append(&sensors, measures_len);

        if (config_is_libatasmart_enabled())
            atasmart_psensor_list_append(&sensors, measures_len);

        if (config_is_nvctrl_enabled())
            nvidia_psensor_list_append(&sensors, measures_len);

        if (config_is_atiadlsdk_enabled())
            amd_psensor_list_append(&sensors, measures_len);

        if (config_is_gtop2_enabled())
            gtop2_psensor_list_append(&sensors, measures_len);

        if (config_is_udisks2_enabled())
            udisks2_psensor_list_append(&sensors, measures_len);
    }

    associate_preferences(sensors);

    return sensors;
}

int main(int argc, char **argv)
{
    int optc, cmdok, opti, ret;
    char *url = nullptr;

    // cpu_set_t mask;
    // CPU_ZERO(&mask);
    // CPU_SET(0, &mask); // Only use CPU 0
    // sched_setaffinity(0, sizeof(mask), &mask);

    program_name = argv[0];

    #pragma message("Compiling at: " __DATE__ " " __TIME__)
    printf("2 DATE TIME: %s %s \n", __TIME__, __DATE__);
    // char *current_locale = setlocale(LC_ALL, "");
    // printf("Current locale: %s\n", current_locale ? current_locale : "nullptr");
    // printf("LANGUAGE=%s\n", getenv("LANGUAGE") ? getenv("LANGUAGE") : "nullptr");
    // printf("LANG=%s\n", getenv("LANG") ? getenv("LANG") : "nullptr");
    // printf("LC_ALL=%s\n", getenv("LC_ALL") ? getenv("LC_ALL") : "nullptr");

#if ENABLE_NLS
    // printf("PACKAGE: %s, LOCALEDIR: %s\n", PACKAGE, LOCALEDIR);
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    // char *bound_dir = bindtextdomain(PACKAGE, nullptr);
    // printf("Bound directory: %s\n", bound_dir ? bound_dir : "nullptr");
    // printf("Current textdomain: %s\n", textdomain(nullptr));
#else
    printf("NLS not enabled\n");
#endif

    int new_instance = 0;

    cmdok = 1;
    while ((optc = getopt_long(argc, argv, "vhd:u:n", long_options, &opti)) != -1)
    {
        switch (optc)
        {
        case 'u':
            if (optarg)
            {
                if (url)
                    free(url);
                url = strdup(optarg);
            }
            break;
        case 'h':
            print_help();
            if (url)
                free(url);
            exit(EXIT_SUCCESS);
        case 'v':
            print_version();
            if (url)
                free(url);
            exit(EXIT_SUCCESS);
        case 'd':
            log_level = atoi(optarg);
            log_info(_("Enables debug mode."));
            break;
        case 'n':
            new_instance = 1;
            break;
        default:
            cmdok = 0;
            break;
        }
    }

    if (!cmdok || optind != argc)
    {
        fprintf(stderr, _("Try `%s --help' for more information.\n"),
                program_name);
        if (url)
            free(url);
        exit(EXIT_FAILURE);
    }

    log_init();

    GApplication *app = g_application_new(PACKAGE_GSETTING, 0);

    g_application_register(app, nullptr, nullptr);

    if (!new_instance && g_application_get_is_remote(app))
    {
        g_application_activate(app);
        log_warn(_("A Psensor instance already exists."));
        if (url)
            free(url);
        exit(EXIT_SUCCESS);
    }

    struct ui_psensor ui;
    memset((void *)&ui, 0, sizeof(struct ui_psensor));

    g_signal_connect(app, "activate", G_CALLBACK(cb_activate), &ui);

    log_glib_info();
#if !(GLIB_CHECK_VERSION(2, 31, 0))
    /*
     * Since GLib 2.31 g_thread_init call is deprecated and not
     * needed.
     */
    log_debug("Calling g_thread_init(nullptr)");
    g_thread_init(nullptr);
#endif

    gtk_init(nullptr, nullptr);

    pmutex_init(&ui.sensors_mutex);

    ui.config = config_load();

    ui.sensors = create_sensors_list(url, ui.config);
    associate_cb_alarm_raised(ui.sensors, &ui);

    if (ui.config->slog_enabled)
        slog_activate(nullptr,
                      (const Psensor*const *) ui.sensors,
                      &ui.sensors_mutex,
                      config_get_slog_interval());

    // obsolete ui_status_init(&ui);
    // ui_status_set_visible(1);

    /* main window */
    ui_window_create(&ui);

    ui_enable_alpha_channel(&ui);
    
    pthread_t thread;
    ret = pthread_create(&thread, nullptr, update_measures, &ui);
    if (ret)
        log_err(_("Failed to create thread for monitoring sensors"));

    ui.graph_update_interval = ui.config->graph_update_interval;

    g_timeout_add(ONE_SECOND * ui.graph_update_interval, ui_refresh_thread, &ui);

    ui_appindicator_init(&ui);
    ui_unity_init();

    gdk_notify_startup_complete();

    // /*
    //  * hack, did not find a cleaner solution.
    //  * wait 30s to ensure that the status icon is attempted to be
    //  * drawn before determining whether the main window must be
    //  * show.
    //  */
    if (ui.config->hide_on_startup)
    {
        g_timeout_add(ONE_SECOND, (GSourceFunc)initial_window_show, &ui);
    }
    else
    {
        initial_window_show(&ui);
    }

    /* main loop */
    gtk_main();

    cleanup(&ui);

    pthread_join(thread, nullptr);

    log_debug("Quitting...");
    log_close();

    g_object_unref(app);

    if (url)
        free(url);

    return 0;
}
