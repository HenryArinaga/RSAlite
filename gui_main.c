#include <gtk/gtk.h>
#include <stdint.h>
#include <limits.h>
#include "log.h"
#include <errno.h>
#include <time.h>
#include <glib.h>
#include <string.h>

#include "factor.h"
#include "prime.h"
#include "optimization.h"

typedef struct BenchmarkJob BenchmarkJob;

typedef struct
{
    GtkWidget *window;
    GtkWidget *entry;
    GtkWidget *result_view;

    GtkWidget *factor_button;
    GtkWidget *clear_button;
    GtkWidget *cancel_button;

    GtkWidget *trial_button;
    GtkWidget *sqrt_button;
    GtkWidget *wheel_button;
    GtkWidget *fermat_button;
    GtkWidget *pollard_button;

    GtkWidget *sieve_button;
    GtkWidget *benchmark_button;
    GtkWidget *quit_button;

    GtkWidget *spinner;

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

static void on_export_csv(GSimpleAction *action,
                          GVariant *parameter,
                          gpointer app_ptr)
{
    (void)action;
    (void)parameter;

    GtkApplication *app = GTK_APPLICATION(app_ptr);
    GtkWindow *win = gtk_application_get_active_window(app);

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Export CSV",
        win,
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL);

    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
                                      "rsalite_log.csv");

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

static void on_app_quit(GSimpleAction *action,
                        GVariant *parameter,
                        gpointer app_ptr)
{
    (void)action;
    (void)parameter;
    g_application_quit(G_APPLICATION(app_ptr));
}

static void on_quit_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    AppWidgets *w = user_data;
    gtk_window_close(GTK_WINDOW(w->window));
}

static const GActionEntry app_actions[] = {
    {.name = "quit", .activate = on_app_quit},
    {.name = "export_csv", .activate = on_export_csv}};

static void setup_app_menu(GtkApplication *app)
{
    GMenu *menu_bar = g_menu_new();
    GMenu *file_menu = g_menu_new();

    g_menu_append(file_menu, "Export CSV…", "app.export_csv");
    g_menu_append(file_menu, "Quit", "app.quit");

    g_menu_append_submenu(menu_bar, "File", G_MENU_MODEL(file_menu));
    gtk_application_set_menubar(app, G_MENU_MODEL(menu_bar));

    g_object_unref(file_menu);
    g_object_unref(menu_bar);
}

static gboolean benchmark_finish_cb(gpointer data)
{
    BenchmarkJob *job = data;
    AppWidgets *w = job->app;

    GtkTextBuffer *buffer =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(w->result_view));

    if (job->cancel_requested)
    {
        gtk_text_buffer_set_text(buffer, "Operation cancelled.\n", -1);
        goto cleanup;
    }

    GString *out = g_string_new(NULL);

    const char *method_name =
        (job->method == FACTOR_METHOD_TRIAL)     ? "Trial Division"
        : (job->method == FACTOR_METHOD_SQRT)    ? "Square Root"
        : (job->method == FACTOR_METHOD_WHEEL)   ? "Wheel Factorization"
        : (job->method == FACTOR_METHOD_FERMAT)  ? "Fermat"
        : (job->method == FACTOR_METHOD_POLLARD) ? "Pollard"
                                                 : "Other";

    if (job->opt.USE_BENCHMARKING)
    {
        g_string_append_printf(out,
                               "Benchmark Mode: ON\nMethod: %s\nTime: %.6f seconds\n\n%llu = ",
                               method_name,
                               job->elapsed,
                               (unsigned long long)job->n);
    }
    else
    {
        g_string_append_printf(out,
                               "Method: %s\n%llu = ",
                               method_name,
                               (unsigned long long)job->n);
    }

    for (int i = 0; i < job->count; i++)
    {
        g_string_append_printf(out, "%llu",
                               (unsigned long long)job->factors[i]);
        if (i < job->count - 1)
            g_string_append(out, " × ");
    }

    g_string_append(out, "\n");
    gtk_text_buffer_set_text(buffer, out->str, -1);
    g_string_free(out, TRUE);

    log_add(job->n,
            job->method,
            &job->opt,
            job->elapsed,
            job->factors,
            job->count);

cleanup:
    gtk_widget_set_sensitive(w->factor_button, TRUE);
    gtk_widget_set_sensitive(w->clear_button, TRUE);
    gtk_widget_set_sensitive(w->entry, TRUE);

    gtk_widget_set_sensitive(w->trial_button, TRUE);
    gtk_widget_set_sensitive(w->sqrt_button, TRUE);
    gtk_widget_set_sensitive(w->wheel_button, TRUE);
    gtk_widget_set_sensitive(w->fermat_button, TRUE);
    gtk_widget_set_sensitive(w->pollard_button, TRUE);

    gtk_widget_set_sensitive(w->sieve_button, TRUE);
    gtk_widget_set_sensitive(w->benchmark_button, TRUE);

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

    job->count = factor_number(
        job->n,
        job->method,
        job->factors,
        64,
        &job->opt);

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

static void reset_method_labels(AppWidgets *w)
{
    gtk_button_set_label(GTK_BUTTON(w->trial_button), "Trial Division");
    gtk_button_set_label(GTK_BUTTON(w->sqrt_button), "Square Root");
    gtk_button_set_label(GTK_BUTTON(w->wheel_button), "Wheel Factorization");
    gtk_button_set_label(GTK_BUTTON(w->fermat_button), "Fermat's Primality Test");
    gtk_button_set_label(GTK_BUTTON(w->pollard_button), "Pollard's Rho");
}

static void on_trial_division_clicked(GtkButton *b, gpointer u)
{
    AppWidgets *w = u;
    w->method = FACTOR_METHOD_TRIAL;
    reset_method_labels(w);
    gtk_button_set_label(b, "Trial Division (Selected)");
}

static void on_square_root_clicked(GtkButton *b, gpointer u)
{
    AppWidgets *w = u;
    w->method = FACTOR_METHOD_SQRT;
    reset_method_labels(w);
    gtk_button_set_label(b, "Square Root (Selected)");
}

static void on_wheel_clicked(GtkButton *b, gpointer u)
{
    AppWidgets *w = u;
    w->method = FACTOR_METHOD_WHEEL;
    reset_method_labels(w);
    gtk_button_set_label(b, "Wheel Factorization (Selected)");
}

static void on_fermat_clicked(GtkButton *b, gpointer u)
{
    AppWidgets *w = u;
    w->method = FACTOR_METHOD_FERMAT;
    reset_method_labels(w);
    gtk_button_set_label(b, "Fermat's Primality Test (Selected)");
}

static void on_pollard_clicked(GtkButton *b, gpointer u)
{
    AppWidgets *w = u;
    w->method = FACTOR_METHOD_POLLARD;
    reset_method_labels(w);
    gtk_button_set_label(b, "Pollard's Rho (Selected)");
}

static void on_sieve_toggled(GtkToggleButton *button, gpointer user_data)
{
    AppWidgets *w = user_data;
    w->opt.USE_SIEVE = gtk_toggle_button_get_active(button);
    gtk_button_set_label(GTK_BUTTON(button),
                         w->opt.USE_SIEVE ? "Sieve ON" : "Sieve OFF");
}

static void on_benchmark_toggled(GtkToggleButton *button, gpointer user_data)
{
    AppWidgets *w = user_data;
    w->opt.USE_BENCHMARKING = gtk_toggle_button_get_active(button);
    gtk_button_set_label(GTK_BUTTON(button),
                         w->opt.USE_BENCHMARKING ? "Benchmarking ON" : "Benchmarking OFF");
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
    GtkTextBuffer *buffer =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(w->result_view));

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

    gtk_text_buffer_set_text(
        buffer,
        w->opt.USE_BENCHMARKING ? "Running benchmark...\n" : "Factoring...\n",
        -1);

    gtk_widget_set_sensitive(w->factor_button, FALSE);
    gtk_widget_set_sensitive(w->clear_button, FALSE);
    gtk_widget_set_sensitive(w->entry, FALSE);

    gtk_widget_set_sensitive(w->trial_button, FALSE);
    gtk_widget_set_sensitive(w->sqrt_button, FALSE);
    gtk_widget_set_sensitive(w->wheel_button, FALSE);
    gtk_widget_set_sensitive(w->fermat_button, FALSE);
    gtk_widget_set_sensitive(w->pollard_button, FALSE);

    gtk_widget_set_sensitive(w->sieve_button, FALSE);
    gtk_widget_set_sensitive(w->benchmark_button, FALSE);

    gtk_widget_set_visible(w->cancel_button, TRUE);
    gtk_widget_set_sensitive(w->cancel_button, TRUE);

    gtk_spinner_start(GTK_SPINNER(w->spinner));
    gtk_widget_set_visible(w->spinner, TRUE);

    g_thread_new("rsalite-worker", benchmark_worker, job);
}

static void on_activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;

    const char *quit_accels[] = {"<Primary>q", NULL};
    gtk_application_set_accels_for_action(app, "app.quit", quit_accels);

    GtkBuilder *builder = gtk_builder_new_from_file("ui/interface.glade");
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "entry_window"));

    AppWidgets *w = g_new0(AppWidgets, 1);

    w->window = window;
    w->entry = GTK_WIDGET(gtk_builder_get_object(builder, "entry_display"));
    w->result_view = GTK_WIDGET(gtk_builder_get_object(builder, "result_output"));

    w->factor_button = GTK_WIDGET(gtk_builder_get_object(builder, "factor_button"));
    w->clear_button = GTK_WIDGET(gtk_builder_get_object(builder, "clear_button"));
    w->cancel_button = GTK_WIDGET(gtk_builder_get_object(builder, "cancel_button"));

    w->trial_button = GTK_WIDGET(gtk_builder_get_object(builder, "trial_division_button"));
    w->sqrt_button = GTK_WIDGET(gtk_builder_get_object(builder, "square_root_button"));
    w->wheel_button = GTK_WIDGET(gtk_builder_get_object(builder, "wheel_factorization_button"));
    w->fermat_button = GTK_WIDGET(gtk_builder_get_object(builder, "fermats_primality_test_button"));
    w->pollard_button = GTK_WIDGET(gtk_builder_get_object(builder, "pollards_rho_button"));

    w->sieve_button = GTK_WIDGET(gtk_builder_get_object(builder, "sieve_button"));
    w->benchmark_button = GTK_WIDGET(gtk_builder_get_object(builder, "benchmark_button"));

    w->spinner = GTK_WIDGET(gtk_builder_get_object(builder, "progress_spinner"));

    w->method = FACTOR_METHOD_TRIAL;

    w->opt.USE_SIEVE = false;
    w->opt.USE_BENCHMARKING = false;
    w->opt.cancel_flag = NULL;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->sieve_button), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w->benchmark_button), FALSE);
    gtk_button_set_label(GTK_BUTTON(w->sieve_button), "Sieve OFF");
    gtk_button_set_label(GTK_BUTTON(w->benchmark_button), "Benchmarking OFF");
    GtkWidget *quit_button = GTK_WIDGET(gtk_builder_get_object(builder, "quit_button"));

    g_signal_connect(w->entry, "insert-text",
                     G_CALLBACK(on_entry_insert_text), NULL);

    g_signal_connect(w->factor_button, "clicked",
                     G_CALLBACK(on_factor_clicked), w);
    g_signal_connect(w->clear_button, "clicked",
                     G_CALLBACK(on_clear_clicked), w);
    g_signal_connect(w->cancel_button, "clicked",
                     G_CALLBACK(on_cancel_clicked), w);
    g_signal_connect(quit_button, "clicked",
                     G_CALLBACK(on_quit_clicked), w);

    g_signal_connect(w->trial_button, "clicked",
                     G_CALLBACK(on_trial_division_clicked), w);
    g_signal_connect(w->sqrt_button, "clicked",
                     G_CALLBACK(on_square_root_clicked), w);
    g_signal_connect(w->wheel_button, "clicked",
                     G_CALLBACK(on_wheel_clicked), w);
    g_signal_connect(w->fermat_button, "clicked",
                     G_CALLBACK(on_fermat_clicked), w);
    g_signal_connect(w->pollard_button, "clicked",
                     G_CALLBACK(on_pollard_clicked), w);

    g_signal_connect(w->sieve_button, "toggled",
                     G_CALLBACK(on_sieve_toggled), w);
    g_signal_connect(w->benchmark_button, "toggled",
                     G_CALLBACK(on_benchmark_toggled), w);

    gtk_window_set_application(GTK_WINDOW(window), app);

    setup_app_menu(app);

    gtk_widget_show_all(window);
    gtk_widget_set_visible(w->spinner, FALSE);
    gtk_widget_set_visible(w->cancel_button, FALSE);

    g_object_unref(builder);
}

int main(int argc, char **argv)
{
    log_init();

    GtkApplication *app =
        gtk_application_new("com.henry.RSAlite", G_APPLICATION_DEFAULT_FLAGS);

    g_action_map_add_action_entries(
        G_ACTION_MAP(app),
        app_actions,
        G_N_ELEMENTS(app_actions),
        app);

    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);

    g_object_unref(app);
    log_shutdown();
    return status;
}
