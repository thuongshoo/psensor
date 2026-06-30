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
#include <sys/syscall.h>

#include <gtk/gtk.h>

#include <pmutex.h>
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
#include <glib.h>
#include "copyright.h"

#define ONE_SECOND 1000U

static void print_version(void)
{
    printf("%s %s\n", PACKAGE_NAME, VERSION);
    printf(_(PSENSOR_COPYRIGHT_CLI),
           PSENSOR_ORIGINAL_YEARS, PSENSOR_ORIGINAL_EMAIL,
           PSENSOR_CURRENT_YEARS, PSENSOR_CURRENT_EMAIL);
}

static void print_help(const char *program_name)
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
            // Currently all sensors share the same buffer size so no need to check all
            return;
        }
    }
}

static void *sensor_data_collector(void *data)
{
    struct ui_psensor *ui = (struct ui_psensor *)data;
    Pconfig *cfg = ui->config;

    pthread_setname_np(pthread_self(), "collector");
    while (!ui->should_exit)
    {
        update_measures_lock(&ui->sensors_mutex);

        Psensor **sensors = ui->sensors;
        if (!sensors)
        {
            update_measures_unlock(&ui->sensors_mutex);
            pthread_exit(nullptr);
        }

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
            cfg->is_new_data = true;

        unsigned int period = (unsigned int)cfg->sensor_update_interval;

        increase_skipped_draw(&cfg->graph_ctx);

        update_measures_unlock(&ui->sensors_mutex);

        sleep(period);
    }

    return nullptr;
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

static void update_sensor_list_thread(gpointer data, gpointer user_data)
{
    pthread_setname_np(pthread_self(), "list-update");

    struct ui_psensor *ui = (struct ui_psensor *)data;

    ui_refresh_thread_lock(&ui->sensors_mutex);
    ui_sensorlist_update(ui, false);
    ui_refresh_thread_unlock(&ui->sensors_mutex);

    // Xong việc với danh sách text, giờ yêu cầu vẽ đồ thị
    // Phải dùng g_idle_add vì GTK không cho gọi queue_draw từ thread khác
    g_idle_add((GSourceFunc)queue_redraw_idle, ui_get_graph_widget());
}

// ===== THREAD POOL CHO XỬ LÝ CẢM BIẾN =====
static GThreadPool *sensor_update_pool = nullptr;

static void init_sensor_update_pool(void)
{
    if (sensor_update_pool)
        return;

    sensor_update_pool = g_thread_pool_new(
        (GFunc)update_sensor_list_thread,
        nullptr,
        1, // 1 thread, tránh chồng lấn dữ liệu
        FALSE,
        nullptr);
}

static gboolean ui_refresh_thread(gpointer data)
{
    pthread_setname_np(pthread_self(), "ui-timer");

    struct ui_psensor *ui = (struct ui_psensor *)data;
    gboolean ret = TRUE;
    const Pconfig *config = ui->config;

    if (!config->is_new_data)
    {
        queue_another_execution(ui);
        return FALSE;
    }

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

    if (!should_update_ui(ui))
    {
        queue_another_execution(ui);
        return G_SOURCE_REMOVE;
    }

    //  Đẩy việc cập nhật danh sách cảm biến sang thread riêng
    init_sensor_update_pool();
    g_thread_pool_push(sensor_update_pool, ui, nullptr);

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
    Psensor **sensor_cur = sensors; /* Current position in sensor array */

    /* Iterate through nullptr-terminated array of sensor pointers */
    while (*sensor_cur)
    {
        Psensor *s = *sensor_cur; /* Current sensor being processed */

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

        sensor_cur++; /* Move to next sensor in the array */
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

/* ============ 1. AppOptions struct ============ */
typedef struct
{
    char *url;
    int debug_level;
    int new_instance;
    int show_help;
    int show_version;
} AppOptions;
/* Định nghĩa ở đầu file hoặc trong header */
typedef struct
{
    AppOptions options;      // Options từ command line
    UI_psensor ui;           // UI state
    GApplication *app;       // GTK application
    pthread_t update_thread; // Background thread
    guint timer_id;          // Timer ID
} AppContext;

/* Helper: parse integer an toàn */
static int parse_int(const char *str, int min, int max, int *out)
{
    char *endptr;
    long val;

    if (!str || !out)
        return 0;

    errno = 0;
    val = strtol(str, &endptr, 10);

    /* Kiểm tra lỗi */
    if (errno == ERANGE || val < min || val > max)
    {
        return 0;
    }

    if (*endptr != '\0' || endptr == str)
    {
        return 0;
    }

    *out = (int)val;
    return 1;
}

/* ============ 2. Parse arguments ============ */
static int parse_arguments(int argc, char **argv, AppOptions *opts)
{
    int optc, opti;

    const char *program_name = argv[0];

    memset(opts, 0, sizeof(AppOptions));

    // Chuyển các static option vào đây
    while ((optc = getopt_long(argc, argv, "vhd:u:n", long_options, &opti)) != -1)
    {
        switch (optc)
        {
        case 'u':
            if (optarg)
            {
                if (opts->url)
                    free(opts->url);
                opts->url = strdup(optarg);
            }
            break;
        case 'h':
            opts->show_help = 1;
            break;
        case 'v':
            opts->show_version = 1;
            break;
        case 'd':
            if (!parse_int(optarg, 0, 3, &opts->debug_level))
            {
                fprintf(stderr, _("Debug level must be integer between 0 and 3.\n"));
                return 0;
            }
            log_level = opts->debug_level;
            log_info(_("Enables debug mode."));
            break;
        case 'n':
            opts->new_instance = 1;
            break;
        default:
            return 0;
        }
    }

    if (opts->show_help)
    {
        print_help(program_name);
        return 0;
    }

    if (opts->show_version)
    {
        print_version();
        return 0;
    }

    if (optind != argc)
    {
        fprintf(stderr, _("Try `%s --help' for more information.\n"), program_name);
        return 0;
    }

    return 1;
}

static void free_url(AppContext *ctx)
{
    // Free options
    if (ctx->options.url)
    {
        free(ctx->options.url);
        ctx->options.url = nullptr;
    }
}
/* ============ 3. Initialize application ============ */
static int initialize_application(AppContext *ctx)
{
    // Setup NLS
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    // Setup logging
    log_init();
    log_glib_info();

    // Setup GTK
    gtk_init(nullptr, nullptr);

    // Setup GLib
#if !(GLIB_CHECK_VERSION(2, 31, 0))
    g_thread_init(nullptr);
#endif

    // Create GApplication
    ctx->app = g_application_new(PACKAGE_GSETTING, 0);
    g_application_register(ctx->app, nullptr, nullptr);

    // Check remote instance
    if (!ctx->options.new_instance && g_application_get_is_remote(ctx->app))
    {
        g_application_activate(ctx->app);
        log_warn(_("A Psensor instance already exists."));
        return 0;
    }

    // Init UI struct
    memset(&ctx->ui, 0, sizeof(UI_psensor));

    // Init mutexes
    pmutex_init(&ctx->ui.sensors_mutex);
    pmutex_init(&ctx->ui.graph_mutex);

    // Load config
    ctx->ui.config = config_load();
    if (!ctx->ui.config)
    {
        log_err("Failed to load config");
        return 0;
    }

    // Create sensors list
    ctx->ui.sensors = create_sensors_list(ctx->options.url, ctx->ui.config);

    // Associate callbacks
    associate_cb_alarm_raised(ctx->ui.sensors, &ctx->ui);

    // Init status (obsolete but keep for compatibility)
    // ui_status_init(&ctx->ui);

    // Connect signals
    g_signal_connect(ctx->app, "activate", G_CALLBACK(cb_activate), &ctx->ui);

    return 1;
}

/* ============ 4. Create main window ============ */
static int create_main_window(AppContext *ctx)
{
    UI_psensor *ui = &ctx->ui;

    // Enable alpha channel
    ui_enable_alpha_channel(ui);

    // Create main window
    ui_window_create(ui);

    // Init appindicator
    ui_appindicator_init(ui);
    ui_unity_init();

    // Setup startup behavior
    if (ui->config->hide_on_startup)
    {
        g_timeout_add(ONE_SECOND, (GSourceFunc)initial_window_show, ui);
    }
    else
    {
        initial_window_show(ui);
    }

    // Enable slog if configured
    if (ui->config->slog_enabled)
    {
        slog_activate(nullptr,
                      (const Psensor *const *)ui->sensors,
                      &ui->sensors_mutex,
                      config_get_slog_interval());
    }

    gdk_notify_startup_complete();

    return 1;
}

/* ============ 5. Start background threads ============ */
static int start_background_threads(AppContext *ctx)
{
    UI_psensor *ui = &ctx->ui;
    int ret;

    // Start update thread
    ret = pthread_create(&ctx->update_thread, nullptr, sensor_data_collector, ui);
    if (ret)
    {
        log_err(_("Failed to create thread for monitoring sensors: %s"),
                strerror(ret));
        // Continue anyway, UI can still work
    }

    // Setup graph update timer
    ui->graph_update_interval = ui->config->graph_update_interval;
    ctx->timer_id = g_timeout_add(ONE_SECOND * ui->graph_update_interval,
                                  ui_refresh_thread, ui);

    return 1;
}

/* ============ 6. Run main loop ============ */
static void run_main_loop(AppContext *ctx)
{
    gtk_main();
}

/* ============ 7. Cleanup ============ */
static void cleanup_application(AppContext *ctx)
{
    UI_psensor *ui = &ctx->ui;

    // Signal exit
    ui->should_exit = TRUE;

    // Cleanup threads
    if (ctx->update_thread)
    {
        pthread_join(ctx->update_thread, nullptr);
    }

    // Remove timer
    if (ctx->timer_id)
    {
        g_source_remove(ctx->timer_id);
    }

    // Cleanup UI
    cleanup(ui);

    // Unref app
    if (ctx->app)
    {
        g_object_unref(ctx->app);
        ctx->app = nullptr;
    }

    // Close log
    log_debug("Quitting...");
    log_close();
}

int main(int argc, char **argv)
{
    AppContext ctx = {0}; // Gom tất cả biến vào struct
    int ret = EXIT_SUCCESS;

    // 1. Parse arguments
    if (!parse_arguments(argc, argv, &ctx.options))
    {
        free_url(&ctx);
        return EXIT_FAILURE;
    }

    // 2. Initialize
    if (!initialize_application(&ctx))
    {
        free_url(&ctx);
        return EXIT_FAILURE;
    }

    // 3. Create UI
    if (!create_main_window(&ctx))
        return EXIT_FAILURE;

    // 4. Start background threads
    if (!start_background_threads(&ctx))
        return EXIT_FAILURE;

    // 5. Run main loop
    run_main_loop(&ctx);

    // 6. Cleanup
    cleanup_application(&ctx);

    return ret;
}
