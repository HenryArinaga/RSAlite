#include <gtk/gtk.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <glib.h>
#include <string.h>

#include "log.h"
#include "factor.h"
#include "prime.h"
#include "optimization.h"

typedef struct BenchmarkJob BenchmarkJob;

typedef struct
{
    GtkWidget *window;
    GtkWidget *entry;
    GtkWidget *result_view;
    GtkWidget *logs_textview;
    
    GtkWidget *factor_button;
    GtkWidget *clear_button;
    GtkWidget *cancel_button;
    GtkWidget *export_csv_button;
    GtkWidget *clear_logs_button;
    
    GtkWidget *methods_listbox;
    GtkWidget *spinner;
    
    GtkWidget *sieve_switch;
    GtkWidget *simd_switch;
    GtkWidget *benchmark_switch;
    GtkWidget *dark_mode_switch;
    
    BenchmarkJob *current_job;
    
    FactorMethod method;
    struct OptimizationContext opt;
} AppWidgets;

struct BenchmarkJob
{
    uint64_t n;
    FactorMethod method;
    struct OptimizationContext opt;
    
    uint64_t factors[64];
    int count;
    double elapsed;
    
    volatile bool cancel_requested;
    
    AppWidgets *app;
};


static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void update_logs_display(AppWidgets *w)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w->logs_textview));
    GString *logs_text = g_string_new("");
    
    int count = log_count();
    if (count == 0)
    {
        g_string_append(logs_text, "No operations logged yet.\n");
    }
    else
    {
        for (int i = count - 1; i >= 0; i--)
        {
            const LogEntry *entry = log_get(i);
            if (entry)
            {
                const char *method_name =
                    (entry->method == FACTOR_METHOD_TRIAL) ? "Trial" :
                    (entry->method == FACTOR_METHOD_SQRT) ? "Sqrt" :
                    (entry->method == FACTOR_METHOD_WHEEL) ? "Wheel" :
                    (entry->method == FACTOR_METHOD_FERMAT) ? "Fermat" :
                    (entry->method == FACTOR_METHOD_POLLARD) ? "Pollard" : "Unknown";
                
                g_string_append_printf(logs_text,
                    "[%s] %llu → %s (%.4fs)\n",
                    method_name,
                    (unsigned long long)entry->input,
                    entry->result,
                    entry->elapsed);
            }
        }
    }
    
    gtk_text_buffer_set_text(buffer, logs_text->str, -1);
    g_string_free(logs_text, TRUE);
}

static void on_export_csv(GtkButton *button, gpointer user_data)
{
    (void)button;
    AppWidgets *w = user_data;
    
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Export CSV",
        GTK_WINDOW(w->window),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL);
    
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "rsalite_log.csv");
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        
        if (log_export_csv(path) == 0)
            g_print("Exported CSV to %s\n", path);
        else
            g_printerr("Failed to export CSV\n");
        
        g_free(path);
    }
    
    gtk_widget_destroy(dialog);
}

static void on_clear_logs(GtkButton *button, gpointer user_data)
{
    (void)button;
    AppWidgets *w = user_data;
    
    log_clear();
    update_logs_display(w);
}

static gboolean benchmark_finish_cb(gpointer data)
{
    BenchmarkJob *job = data;
    AppWidgets *w = job->app;
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w->result_view));
    
    if (job->cancel_requested)
    {
        gtk_text_buffer_set_text(buffer, "Operation cancelled.\n", -1);
        goto cleanup;
    }
    
    GString *out = g_string_new(NULL);
    
    const char *method_name =
        (job->method == FACTOR_METHOD_TRIAL) ? "Trial Division" :
        (job->method == FACTOR_METHOD_SQRT) ? "Square Root" :
        (job->method == FACTOR_METHOD_WHEEL) ? "Wheel Factorization" :
        (job->method == FACTOR_METHOD_FERMAT) ? "Fermat" :
        (job->method == FACTOR_METHOD_POLLARD) ? "Pollard" : "Other";
    
    if (job->opt.USE_BENCHMARKING)
    {
        g_string_append_printf(out,
            "Method: %s\nTime: %.6f seconds\n\n%llu = ",
            method_name,
            job->elapsed,
            (unsigned long long)job->n);
    }
    else
    {
        g_string_append_printf(out,
            "Method: %s\n\n%llu = ",
            method_name,
            (unsigned long long)job->n);
    }
    
    for (int i = 0; i < job->count; i++)
    {
        g_string_append_printf(out, "%llu", (unsigned long long)job->factors[i]);
        if (i < job->count - 1)
            g_string_append(out, " × ");
    }
    
    g_string_append(out, "\n");
    gtk_text_buffer_set_text(buffer, out->str, -1);
    g_string_free(out, TRUE);
    
    log_add(job->n, job->method, &job->opt, job->elapsed, job->factors, job->count);
    update_logs_display(w);

cleanup:
    gtk_widget_set_sensitive(w->factor_button, TRUE);
    gtk_widget_set_sensitive(w->clear_button, TRUE);
    gtk_widget_set_sensitive(w->entry, TRUE);
    gtk_widget_set_sensitive(w->methods_listbox, TRUE);
    gtk_widget_set_sensitive(w->sieve_switch, TRUE);
    gtk_widget_set_sensitive(w->benchmark_switch, TRUE);
    gtk_widget_set_sensitive(w->simd_switch, TRUE);
    
    gtk_spinner_stop(GTK_SPINNER(w->spinner));
    gtk_widget_set_visible(w->spinner, FALSE);
    gtk_widget_set_visible(w->cancel_button, FALSE);
    gtk_widget_set_sensitive(w->cancel_button, TRUE);
    
    w->current_job = NULL;
    g_free(job);
    return G_SOURCE_REMOVE;
}

static gpointer benchmark_worker(gpointer data)
{
    BenchmarkJob *job = data;
    
    double start = now_seconds();
    job->count = factor_number(job->n, job->method, job->factors, 64, &job->opt);
    double end = now_seconds();
    job->elapsed = end - start;
    
    g_idle_add(benchmark_finish_cb, job);
    return NULL;
}

static void on_entry_insert_text(GtkEditable *editable,
                                 const gchar *text,
                                 gint length,
                                 gint *position,
                                 gpointer user_data)
{
    (void)position;
    (void)user_data;
    
    const int MAX_DIGITS = 20;
    const gchar *current = gtk_entry_get_text(GTK_ENTRY(editable));
    
    if ((int)strlen(current) + length > MAX_DIGITS)
    {
        g_signal_stop_emission_by_name(editable, "insert-text");
        return;
    }
    
    for (int i = 0; i < length; i++)
    {
        if (!g_ascii_isdigit(text[i]))
        {
            g_signal_stop_emission_by_name(editable, "insert-text");
            return;
        }
    }
}

static void on_method_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
    (void)box;
    AppWidgets *w = user_data;
    
    int index = gtk_list_box_row_get_index(row);
    
    switch (index)
    {
        case 0: w->method = FACTOR_METHOD_TRIAL; break;
        case 1: w->method = FACTOR_METHOD_SQRT; break;
        case 2: w->method = FACTOR_METHOD_WHEEL; break;
        case 3: w->method = FACTOR_METHOD_FERMAT; break;
        case 4: w->method = FACTOR_METHOD_POLLARD; break;
        default: w->method = FACTOR_METHOD_TRIAL; break;
    }
    
    // Sieve only works with Trial Division
    bool sieve_available = (w->method == FACTOR_METHOD_TRIAL);
    gtk_widget_set_sensitive(w->sieve_switch, sieve_available);
    if (!sieve_available) {
        gtk_switch_set_active(GTK_SWITCH(w->sieve_switch), FALSE);
    }
    
    // SIMD works with Trial Division and SQRT
    bool simd_available = (w->method == FACTOR_METHOD_TRIAL || w->method == FACTOR_METHOD_SQRT);
    gtk_widget_set_sensitive(w->simd_switch, simd_available);
    if (!simd_available) {
        gtk_switch_set_active(GTK_SWITCH(w->simd_switch), FALSE);
    }
}
static void on_sieve_switch_toggled(GtkSwitch *sw, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    AppWidgets *w = user_data;
    w->opt.USE_SIEVE = gtk_switch_get_active(sw);
}

static void on_simd_switch_toggled(GtkSwitch *sw, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    AppWidgets *w = user_data;
    w->opt.USE_SIMD = gtk_switch_get_active(sw);
}

static void on_benchmark_switch_toggled(GtkSwitch *sw, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    AppWidgets *w = user_data;
    w->opt.USE_BENCHMARKING = gtk_switch_get_active(sw);
}

static void on_dark_mode_switch_toggled(GtkSwitch *sw, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    (void)user_data;
    
    gboolean dark_mode = gtk_switch_get_active(sw);
    GtkSettings *settings = gtk_settings_get_default();
    g_object_set(settings, "gtk-application-prefer-dark-theme", dark_mode, NULL);
    
    // Save preference to config file
    const gchar *config_dir = g_get_user_config_dir();
    gchar *config_path = g_build_filename(config_dir, "rsalite", NULL);
    g_mkdir_with_parents(config_path, 0755);
    
    gchar *config_file = g_build_filename(config_path, "settings.ini", NULL);
    GKeyFile *keyfile = g_key_file_new();
    
    g_key_file_set_boolean(keyfile, "Appearance", "dark-mode", dark_mode);
    g_key_file_save_to_file(keyfile, config_file, NULL);
    
    g_key_file_free(keyfile);
    g_free(config_file);
    g_free(config_path);
}

static void on_cancel_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    AppWidgets *w = user_data;
    
    if (w->current_job)
    {
        w->current_job->cancel_requested = true;
        gtk_widget_set_sensitive(w->cancel_button, FALSE);
    }
}

static void on_clear_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    AppWidgets *w = user_data;
    
    gtk_entry_set_text(GTK_ENTRY(w->entry), "");
    gtk_text_buffer_set_text(
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(w->result_view)),
        "", -1);
}


static void on_factor_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    AppWidgets *w = user_data;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w->result_view));
    
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(w->entry));
    if (!text || *text == '\0')
    {
        gtk_text_buffer_set_text(buffer, "Please enter a number.\n", -1);
        return;
    }
    
    errno = 0;
    char *endptr = NULL;
    unsigned long long n64 = g_ascii_strtoull(text, &endptr, 10);
    
    if (errno == ERANGE || endptr == text || *endptr != '\0' || n64 <= 1ULL)
    {
        gtk_text_buffer_set_text(buffer, "Invalid or unsupported number.\n", -1);
        return;
    }
    
    BenchmarkJob *job = g_new0(BenchmarkJob, 1);
    job->n = (uint64_t)n64;
    job->method = w->method;
    job->opt = w->opt;
    job->app = w;
    job->cancel_requested = false;
    job->opt.cancel_flag = &job->cancel_requested;
    
    w->current_job = job;
    
    gtk_text_buffer_set_text(buffer,
        w->opt.USE_BENCHMARKING ? "Running benchmark...\n" : "Factoring...\n",
        -1);
    
    gtk_widget_set_sensitive(w->factor_button, FALSE);
    gtk_widget_set_sensitive(w->clear_button, FALSE);
    gtk_widget_set_sensitive(w->entry, FALSE);
    gtk_widget_set_sensitive(w->methods_listbox, FALSE);
    gtk_widget_set_sensitive(w->sieve_switch, FALSE);
    gtk_widget_set_sensitive(w->benchmark_switch, FALSE);
    gtk_widget_set_sensitive(w->simd_switch, FALSE);
    
    gtk_widget_set_visible(w->cancel_button, TRUE);
    gtk_widget_set_sensitive(w->cancel_button, TRUE);
    gtk_spinner_start(GTK_SPINNER(w->spinner));
    gtk_widget_set_visible(w->spinner, TRUE);
    
    g_thread_new("rsalite-worker", benchmark_worker, job);
}


static void on_activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;
    
    GtkBuilder *builder = gtk_builder_new_from_file("ui/interface_modern.glade");
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    
    AppWidgets *w = g_new0(AppWidgets, 1);
    
    w->window = window;
    w->entry = GTK_WIDGET(gtk_builder_get_object(builder, "entry_display"));
    w->result_view = GTK_WIDGET(gtk_builder_get_object(builder, "result_output"));
    w->logs_textview = GTK_WIDGET(gtk_builder_get_object(builder, "logs_textview"));
    
    w->factor_button = GTK_WIDGET(gtk_builder_get_object(builder, "factor_button"));
    w->clear_button = GTK_WIDGET(gtk_builder_get_object(builder, "clear_button"));
    w->cancel_button = GTK_WIDGET(gtk_builder_get_object(builder, "cancel_button"));
    w->export_csv_button = GTK_WIDGET(gtk_builder_get_object(builder, "export_csv_button"));
    w->clear_logs_button = GTK_WIDGET(gtk_builder_get_object(builder, "clear_logs_button"));
    
    w->methods_listbox = GTK_WIDGET(gtk_builder_get_object(builder, "methods_listbox"));
    w->spinner = GTK_WIDGET(gtk_builder_get_object(builder, "progress_spinner"));
    
    w->sieve_switch = GTK_WIDGET(gtk_builder_get_object(builder, "sieve_switch"));
    w->simd_switch = GTK_WIDGET(gtk_builder_get_object(builder, "simd_switch"));
    w->benchmark_switch = GTK_WIDGET(gtk_builder_get_object(builder, "benchmark_switch"));
    w->dark_mode_switch = GTK_WIDGET(gtk_builder_get_object(builder, "dark_mode_switch"));
    
    w->method = FACTOR_METHOD_TRIAL;
    w->opt.USE_SIEVE = false;
    w->opt.USE_SIMD = false;
    w->opt.USE_BENCHMARKING = false;
    w->opt.USE_MULTITHREADING = false;
    w->opt.USE_GPU = false;
    w->opt.cancel_flag = NULL;
    
    gtk_list_box_select_row(GTK_LIST_BOX(w->methods_listbox),
        gtk_list_box_get_row_at_index(GTK_LIST_BOX(w->methods_listbox), 0));
    
    g_signal_connect(w->entry, "insert-text",
                     G_CALLBACK(on_entry_insert_text), NULL);
    g_signal_connect(w->factor_button, "clicked",
                     G_CALLBACK(on_factor_clicked), w);
    g_signal_connect(w->clear_button, "clicked",
                     G_CALLBACK(on_clear_clicked), w);
    g_signal_connect(w->cancel_button, "clicked",
                     G_CALLBACK(on_cancel_clicked), w);
    g_signal_connect(w->export_csv_button, "clicked",
                     G_CALLBACK(on_export_csv), w);
    g_signal_connect(w->clear_logs_button, "clicked",
                     G_CALLBACK(on_clear_logs), w);
    
    g_signal_connect(w->methods_listbox, "row-activated",
                     G_CALLBACK(on_method_row_activated), w);
    
    g_signal_connect(w->sieve_switch, "notify::active",
                     G_CALLBACK(on_sieve_switch_toggled), w);
    g_signal_connect(w->simd_switch, "notify::active",
                     G_CALLBACK(on_simd_switch_toggled), w);
    g_signal_connect(w->benchmark_switch, "notify::active",
                     G_CALLBACK(on_benchmark_switch_toggled), w);
    g_signal_connect(w->dark_mode_switch, "notify::active",
                     G_CALLBACK(on_dark_mode_switch_toggled), w);
    
    // Load and apply saved dark mode preference
    const gchar *config_dir = g_get_user_config_dir();
    gchar *config_file = g_build_filename(config_dir, "rsalite", "settings.ini", NULL);
    
    GKeyFile *keyfile = g_key_file_new();
    gboolean dark_mode = FALSE;
    
    if (g_key_file_load_from_file(keyfile, config_file, G_KEY_FILE_NONE, NULL))
    {
        dark_mode = g_key_file_get_boolean(keyfile, "Appearance", "dark-mode", NULL);
    }
    
    g_key_file_free(keyfile);
    g_free(config_file);
    
    gtk_switch_set_active(GTK_SWITCH(w->dark_mode_switch), dark_mode);
    GtkSettings *gtk_settings = gtk_settings_get_default();
    g_object_set(gtk_settings, "gtk-application-prefer-dark-theme", dark_mode, NULL);
    
    gtk_window_set_application(GTK_WINDOW(window), app);
    gtk_widget_show_all(window);
    gtk_widget_set_visible(w->spinner, FALSE);
    gtk_widget_set_visible(w->cancel_button, FALSE);
    
    update_logs_display(w);
    
    g_object_unref(builder);
}

int main(int argc, char **argv)
{
    log_init();
    
    GtkApplication *app = gtk_application_new(
        "com.henry.RSAlite",
        G_APPLICATION_DEFAULT_FLAGS);
    
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    
    g_object_unref(app);
    log_shutdown();
    return status;
}
