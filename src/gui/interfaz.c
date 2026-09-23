#include <gtk/gtk.h>
#include "interfaz.h"
#include "../common/Stats.h"

#include <inttypes.h>
#include <string.h>
#include <sys/wait.h>

typedef struct {
    RunStats stats;
    int exit_status;
} BackendResult;

static const char *backend_names[] = {"serial", "fork", "pthread"};

// Ejecuta los programas de compresión o descompresión
static int run_backend(const char *backend, char mode, const char *input,
    const char *output, BackendResult *result) {
    char executable[64];
    char *arguments[5];
    char *stdout_text = NULL;
    char *stderr_text = NULL;
    GError *error = NULL;
    int wait_status;
    int parsed;

    snprintf(executable, sizeof(executable), "bin/huffman-%s", backend);
    arguments[0] = executable;
    arguments[1] = mode == 'c' ? "c" : "d";
    arguments[2] = (char *)input;
    arguments[3] = (char *)output;
    arguments[4] = NULL;

    memset(result, 0, sizeof(*result));
    // esperar a que termine cada proceso
    if (!g_spawn_sync(NULL, arguments, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
        &stdout_text, &stderr_text, &wait_status, &error)) {
        if (error != NULL)
            g_error_free(error);
        g_free(stdout_text);
        g_free(stderr_text);
        return 0;
    }

    result->exit_status = WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) : -1;
    // Se obtienen las estadisiticas
    if (mode == 'c') {
        parsed = sscanf(stdout_text, "variante=%*s operacion=c archivos=%zu "
            "original=%" SCNu64 " comprimido=%" SCNu64 " tiempo=%lf",
            &result->stats.files, &result->stats.original_bytes,
            &result->stats.compressed_bytes, &result->stats.seconds);
    } else {
        parsed = sscanf(stdout_text, "variante=%*s operacion=d archivos=%zu "
            "verificados=%zu salud=%*f original=%" SCNu64
            " comprimido=%" SCNu64 " tiempo=%lf", &result->stats.files,
            &result->stats.verified, &result->stats.original_bytes,
            &result->stats.compressed_bytes, &result->stats.seconds);
    }

    g_free(stdout_text);
    g_free(stderr_text);
    return parsed == (mode == 'c' ? 4 : 5);
}
// Nombres de labels en la pagina
static void set_result_label(GtkWidget *label, const char *format, double value) {
    char text[64];
    snprintf(text, sizeof(text), format, value);
    gtk_label_set_text(GTK_LABEL(label), text);
}

static void set_size_label(GtkWidget *label, uint64_t value) {
    char text[64];
    snprintf(text, sizeof(text), "%" PRIu64, value);
    gtk_label_set_text(GTK_LABEL(label), text);
}

// Volver a cargar la pagina
static void refresh_interface(void) {
    while (g_main_context_pending(NULL))
        g_main_context_iteration(NULL, FALSE);
}

// Estructura para las estadisticas
typedef struct {
    GtkWidget *cells[8][3];
} StatisticsPage;

// Estructura para los datos de compresión
typedef struct {
    // Nombres
    GtkWidget *directory_label;
    char *selected_path;
    GtkWidget *progress_bar;
    GtkWidget *status_label;
    // Tiempos de duración
    GtkWidget *serial_time;
    GtkWidget *process_time;
    GtkWidget *thread_time;
    // Aceleración
    GtkWidget *serial_speedup;
    GtkWidget *process_speedup;
    GtkWidget *thread_speedup;
    // Tamaños
    GtkWidget *serial_original_size;
    GtkWidget *process_original_size;
    GtkWidget *thread_original_size;
    // Tamaños comprimidos
    GtkWidget *serial_compressed_size;
    GtkWidget *process_compressed_size;
    GtkWidget *thread_compressed_size;
    // Ratios de compresión
    GtkWidget *serial_ratio;
    GtkWidget *process_ratio;
    GtkWidget *thread_ratio;

    StatisticsPage *statistics;
} CompressionPage;

// Estructura para los datos de descompresión
typedef struct {
    // Nombres
    GtkWidget *directory_label;
    char *selected_path;
    GtkWidget *output_directory_label;
    char *output_directory_path;
    GtkWidget *progress_bar;
    GtkWidget *status_label;
    // Tiempos de duración
    GtkWidget *serial_time;
    GtkWidget *process_time;
    GtkWidget *thread_time;
    // Aceleración
    GtkWidget *serial_speedup;
    GtkWidget *process_speedup;
    GtkWidget *thread_speedup;
    // Salud
    GtkWidget *health_serial;
    GtkWidget *health_process;
    GtkWidget *health_thread;

    StatisticsPage *statistics;
} DecompressionPage;

// Tablas de Compresión y Descompresión
typedef struct {
    CompressionPage compression;
    DecompressionPage decompression;
    StatisticsPage statistics;
} AppWidgets;

// Estadísticas de compresión
static void update_compression_statistics(StatisticsPage *page,
    const BackendResult results[3]) {
    int index;

    for (index = 0; index < 3; index++) {
        set_result_label(page->cells[0][index], "%.6f s", results[index].stats.seconds);
        set_result_label(page->cells[3][index], "%.2f%%", index == 0 ? 0.0 :
            stats_speedup_percent(results[0].stats.seconds, results[index].stats.seconds));
        set_size_label(page->cells[5][index], results[index].stats.original_bytes);
        set_size_label(page->cells[6][index], results[index].stats.compressed_bytes);
        set_result_label(page->cells[7][index], "%.2f%%", results[index].stats.original_bytes == 0 ?
            0.0 : 100.0 * results[index].stats.compressed_bytes /
            results[index].stats.original_bytes);
    }
}

// Estadísticas de descompresión
static void update_decompression_statistics(StatisticsPage *page,
    const BackendResult results[3]) {
    int index;

    for (index = 0; index < 3; index++) {
        set_result_label(page->cells[1][index], "%.6f s", results[index].stats.seconds);
        set_result_label(page->cells[2][index], "%.2f%%",
            stats_health_percent(&results[index].stats));
        set_result_label(page->cells[4][index], "%.2f%%", index == 0 ? 0.0 :
            stats_speedup_percent(results[0].stats.seconds, results[index].stats.seconds));
    }
}

// - - - - - CREAR TITULOS - - - - -
static GtkWidget *create_title(const char *text) {
    GtkWidget *label = gtk_label_new(NULL);
    char markup[512];

    snprintf(markup, sizeof(markup), "<span size='18000' weight='bold'>%s</span>",
        text
    );

    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_widget_set_halign(label, GTK_ALIGN_START);

    return label;
}


static GtkWidget *create_subtitle(const char *text) {
    GtkWidget *label = gtk_label_new(text);

    gtk_widget_set_halign(label, GTK_ALIGN_START);

    return label;
}

// - - - - - FUNCIONES DE LA INTERFAZ - - - - -
// Escoger Directorio para Compresión
static void compression_folder_selected(GObject *source_object, GAsyncResult *result,
    gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    CompressionPage *page = user_data;

    GError *error = NULL;

    GFile *folder = gtk_file_dialog_select_folder_finish(dialog, result, &error);

    if (folder != NULL) {
        char *path = g_file_get_path(folder);

        g_free(page->selected_path);
        page->selected_path = g_strdup(path);
        gtk_label_set_text(GTK_LABEL(page->directory_label), path);

        g_free(path);
        g_object_unref(folder);
    }

    if (error != NULL) {
        g_error_free(error);
    }
}

// Presionar botón de búsqueda de directorio para compresión
static void on_compression_search_clicked(GtkButton *button, gpointer user_data) {
    CompressionPage *page = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();

    gtk_file_dialog_set_title(dialog, "Seleccione el directorio a comprimir");

    GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(button));

    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(root), NULL, compression_folder_selected,
        page);
    g_object_unref(dialog);
}

// Escoger archivo comprimido para descompresión
static void decompression_file_selected(GObject *source_object, GAsyncResult *result,
    gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    DecompressionPage *page = user_data;

    GError *error = NULL;

    GFile *file = gtk_file_dialog_open_finish(dialog, result, &error);

    if (file != NULL) {
        char *path = g_file_get_path(file);

        g_free(page->selected_path);
        page->selected_path = g_strdup(path);
        gtk_label_set_text(GTK_LABEL(page->directory_label), path);

        g_free(path);
        g_object_unref(file);
    }

    if (error != NULL) {
        g_error_free(error);
    }
}

// Presionar botón de búsqueda de archivo para descompresión
static void on_decompression_search_clicked(GtkButton *button, gpointer user_data) {
    DecompressionPage *page = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();

    gtk_file_dialog_set_title(dialog, "Seleccione el archivo .huff");

    GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(button));

    gtk_file_dialog_open(dialog, GTK_WINDOW(root), NULL, decompression_file_selected, page);
    g_object_unref(dialog);
}

// Escoger directorio de destino para descompresión
static void decompression_output_folder_selected(GObject *source_object,
    GAsyncResult *result, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    DecompressionPage *page = user_data;
    GError *error = NULL;
    GFile *folder = gtk_file_dialog_select_folder_finish(dialog, result, &error);

    if (folder != NULL) {
        char *path = g_file_get_path(folder);

        g_free(page->output_directory_path);
        page->output_directory_path = g_strdup(path);
        gtk_label_set_text(GTK_LABEL(page->output_directory_label), path);

        g_free(path);
        g_object_unref(folder);
    }

    if (error != NULL)
        g_error_free(error);
}

// Presionar botón de búsqueda del directorio de destino
static void on_decompression_output_search_clicked(GtkButton *button,
    gpointer user_data) {
    DecompressionPage *page = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    GtkRoot *root = gtk_widget_get_root(GTK_WIDGET(button));

    gtk_file_dialog_set_title(dialog, "Seleccione el directorio de destino");
    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(root), NULL,
        decompression_output_folder_selected, page);
    g_object_unref(dialog);
}

// Botón de compresión se presiona
static void on_compress_clicked(GtkButton *button, gpointer user_data) {
    CompressionPage *page = user_data;
    // Estadisitcas
    BackendResult results[3];
    GtkWidget *times[3] = {page->serial_time, page->process_time, page->thread_time};
    GtkWidget *speedups[3] = {page->serial_speedup, page->process_speedup,
        page->thread_speedup};
    GtkWidget *original_sizes[3] = {page->serial_original_size,
        page->process_original_size, page->thread_original_size};
    GtkWidget *compressed_sizes[3] = {page->serial_compressed_size,
        page->process_compressed_size, page->thread_compressed_size};
    GtkWidget *ratios[3] = {page->serial_ratio, page->process_ratio, page->thread_ratio};
    char *archive;
    int index;
    (void)button;

    if (page->selected_path == NULL) {
        gtk_label_set_text(GTK_LABEL(page->status_label),
            "Seleccione un directorio antes de comprimir.");
        return;
    }
    // Se crea el nombre del archivo comprimido
    archive = g_strdup_printf("%s.huff", page->selected_path);
    gtk_label_set_text(GTK_LABEL(page->status_label), "Ejecutando las tres variantes...");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(page->progress_bar), 0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "En progreso");
    
    // Se ejecutan las tres variantes
    for (index = 0; index < 3; index++) {
        char status[128];

        snprintf(status, sizeof(status), "Comprimiendo con %s (%d/3)...",
            backend_names[index], index + 1);
        gtk_label_set_text(GTK_LABEL(page->status_label), status);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), status);
        refresh_interface();
        if (!run_backend(backend_names[index], 'c', page->selected_path, archive,
            &results[index]) || results[index].exit_status != 0) {
            gtk_label_set_text(GTK_LABEL(page->status_label),
                "Error durante la compresión.");
            gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "Error");
            g_free(archive);
            return;
        }
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(page->progress_bar),
            (double)(index + 1) / 3.0);
        refresh_interface();
    }
    // Se actualizan las estadisticas
    for (index = 0; index < 3; index++) {
        set_result_label(times[index], "%.6f s", results[index].stats.seconds);
        set_result_label(speedups[index], "%.2f%%", index == 0 ? 0.0 :
            stats_speedup_percent(results[0].stats.seconds, results[index].stats.seconds));
        set_size_label(original_sizes[index], results[index].stats.original_bytes);
        set_size_label(compressed_sizes[index], results[index].stats.compressed_bytes);
        set_result_label(ratios[index], "%.2f%%", results[index].stats.original_bytes == 0 ?
            0.0 : 100.0 * results[index].stats.compressed_bytes /
            results[index].stats.original_bytes);
    }
            update_compression_statistics(page->statistics, results);

    gtk_label_set_text(GTK_LABEL(page->status_label), "Compresión terminada.");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(page->progress_bar), 1.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "100%");
    g_free(archive);
}

// Botón de descompresión se presiona
static void on_decompress_clicked(GtkButton *button, gpointer user_data) {
    DecompressionPage *page = user_data;
    // Estadisitcas
    BackendResult results[3];
    GtkWidget *times[3] = {page->serial_time, page->process_time, page->thread_time};
    GtkWidget *speedups[3] = {page->serial_speedup, page->process_speedup,
        page->thread_speedup};
    GtkWidget *health[3] = {page->health_serial, page->health_process,
        page->health_thread};
    int index;
    (void)button;

    if (page->selected_path == NULL) {
        gtk_label_set_text(GTK_LABEL(page->status_label),
            "Seleccione un archivo .huff antes de descomprimir.");
        return;
    }

    if (page->output_directory_path == NULL) {
        gtk_label_set_text(GTK_LABEL(page->status_label),
            "Seleccione un directorio de destino antes de descomprimir.");
        return;
    }

    gtk_label_set_text(GTK_LABEL(page->status_label),
        "Ejecutando las tres variantes...");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(page->progress_bar), 0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "En progreso");

    // Se ejecutan las tres variantes
    for (index = 0; index < 3; index++) {
        char status[128];

        snprintf(status, sizeof(status), "Descomprimiendo con %s (%d/3)...",
            backend_names[index], index + 1);
        gtk_label_set_text(GTK_LABEL(page->status_label), status);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), status);
        refresh_interface();
        if (!run_backend(backend_names[index], 'd', page->selected_path,
            page->output_directory_path, &results[index]) ||
            (results[index].exit_status != 0 && results[index].stats.verified ==
            results[index].stats.files)) {
            gtk_label_set_text(GTK_LABEL(page->status_label),
                "Error durante la descompresión.");
            gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "Error");
            return;
        }
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(page->progress_bar),
            (double)(index + 1) / 3.0);
        refresh_interface();
    }
    // Se actualizan las estadisticas
    for (index = 0; index < 3; index++) {
        set_result_label(times[index], "%.6f s", results[index].stats.seconds);
        set_result_label(speedups[index], "%.2f%%", index == 0 ? 0.0 :
            stats_speedup_percent(results[0].stats.seconds, results[index].stats.seconds));
        set_result_label(health[index], "%.2f%%",
            stats_health_percent(&results[index].stats));
    }
    update_decompression_statistics(page->statistics, results);

    gtk_label_set_text(GTK_LABEL(page->status_label), "Descompresión terminada.");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(page->progress_bar), 1.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "100%");
}

// - - - - - TABLAS - - - - -
// Tabla de Compresión
static GtkWidget *create_compression_table(CompressionPage *page) {
    GtkWidget *grid = gtk_grid_new();

    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);

    gtk_grid_set_column_spacing(GTK_GRID(grid), 30);

    // Los titulos de las columnas
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Métrica"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Serial"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Procesos"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Hilos"), 3, 0, 1, 1);

    // Tiempos
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Tiempo"), 0, 1, 1, 1);
    page->serial_time = gtk_label_new("--");
    page->process_time = gtk_label_new("--");
    page->thread_time = gtk_label_new("--");

    gtk_grid_attach(GTK_GRID(grid), page->serial_time, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->process_time, 2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->thread_time, 3, 1, 1, 1);

    // Aceleración
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Aceleración"), 0, 2, 1, 1);
    page->serial_speedup = gtk_label_new("--");
    page->process_speedup = gtk_label_new("--");
    page->thread_speedup = gtk_label_new("--");

    gtk_grid_attach(GTK_GRID(grid), page->serial_speedup, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->process_speedup, 2, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->thread_speedup, 3, 2, 1, 1);

    // El tamaño original
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Tamaño original"), 0, 3, 1, 1);
    page->serial_original_size = gtk_label_new("--");
    page->process_original_size = gtk_label_new("--");
    page->thread_original_size = gtk_label_new("--");

    gtk_grid_attach(GTK_GRID(grid), page->serial_original_size, 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->process_original_size, 2, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->thread_original_size, 3, 3, 1, 1);

    // Tamaño comprimido
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Tamaño comprimido"), 0, 4, 1, 1);
    page->serial_compressed_size = gtk_label_new("--");
    page->process_compressed_size = gtk_label_new("--");
    page->thread_compressed_size = gtk_label_new("--");

    gtk_grid_attach(GTK_GRID(grid), page->serial_compressed_size, 1, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->process_compressed_size, 2, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->thread_compressed_size, 3, 4, 1, 1);

    // Ratio de compresión
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Ratio de compresión"), 0, 5, 1, 1);
    page->serial_ratio = gtk_label_new("--");
    page->process_ratio = gtk_label_new("--");
    page->thread_ratio = gtk_label_new("--");

    gtk_grid_attach(GTK_GRID(grid), page->serial_ratio, 1, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->process_ratio, 2, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->thread_ratio, 3, 5, 1, 1);

    return grid;
}

/// Tabla de Descompresión
static GtkWidget *create_decompression_table(DecompressionPage *page) {
    GtkWidget *grid = gtk_grid_new();

    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 30);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Métrica"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Serial"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Procesos"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Hilos"), 3, 0, 1, 1);

    // Tiempos
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Tiempo"), 0, 1, 1, 1);
    page->serial_time = gtk_label_new("--");
    page->process_time = gtk_label_new("--");
    page->thread_time = gtk_label_new("--");

    gtk_grid_attach(GTK_GRID(grid), page->serial_time, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->process_time, 2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->thread_time, 3, 1, 1, 1);

    // Aceleración
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Aceleración"), 0, 2, 1, 1);
    page->serial_speedup = gtk_label_new("--");
    page->process_speedup = gtk_label_new("--");
    page->thread_speedup = gtk_label_new("--");

    gtk_grid_attach(GTK_GRID(grid), page->serial_speedup, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->process_speedup, 2, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->thread_speedup, 3, 2, 1, 1);

    // Salud
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Salud"), 0, 3, 1, 1);
    page->health_serial = gtk_label_new("--");
    page->health_process = gtk_label_new("--");
    page->health_thread = gtk_label_new("--");

    gtk_grid_attach(GTK_GRID(grid), page->health_serial, 1, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->health_process, 2, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), page->health_thread, 3, 3, 1, 1);

    return grid;
}

// - - - - - VENTANAS - - - - -
// Ventana de compresión
static GtkWidget *create_compression_page(CompressionPage *page) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);

    gtk_widget_set_margin_top(box, 25);
    gtk_widget_set_margin_bottom(box, 25);
    gtk_widget_set_margin_start(box, 25);
    gtk_widget_set_margin_end(box, 25);

    // Parte del directorio
    GtkWidget *section_title = create_title("Escoger Directorio");
    gtk_box_append(GTK_BOX(box), section_title);

    GtkWidget *directory_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    page->directory_label = gtk_label_new("Ningún directorio seleccionado");
    gtk_widget_set_hexpand(page->directory_label, TRUE);

    gtk_widget_set_halign(page->directory_label, GTK_ALIGN_START);

    GtkWidget *search_button = gtk_button_new_with_label("Buscar");

    g_signal_connect(search_button, "clicked", G_CALLBACK(on_compression_search_clicked),
        page);
    gtk_box_append(GTK_BOX(directory_row), page->directory_label);
    gtk_box_append(GTK_BOX(directory_row), search_button);
    gtk_box_append(GTK_BOX(box), directory_row);


    // El botón de inicio de compresión
    GtkWidget *compress_button = gtk_button_new_with_label("Iniciar compresión");
    g_signal_connect(compress_button, "clicked", G_CALLBACK(on_compress_clicked),
        page);
    gtk_box_append(GTK_BOX(box), compress_button);

    // Estado del programa
    page->status_label = gtk_label_new("Esperando para iniciar...");
    gtk_widget_set_halign(page->status_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), page->status_label);

    // Barra
    page->progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(page->progress_bar),
        TRUE);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "0%");

    gtk_box_append(GTK_BOX(box), page->progress_bar);
    
    // Tabla de resultados
    GtkWidget *results_title = create_title("Resultados de Compresión");
    gtk_box_append(GTK_BOX(box), results_title);
    GtkWidget *table = create_compression_table(page);
    gtk_box_append(GTK_BOX(box), table);

    return box;
}

// Ventana de descompresión
static GtkWidget *create_decompression_page(DecompressionPage *page) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);

    gtk_widget_set_margin_top(box, 25);
    gtk_widget_set_margin_bottom(box, 25);
    gtk_widget_set_margin_start(box, 25);
    gtk_widget_set_margin_end(box, 25);

    // Parte del archivo comprimido
    gtk_box_append(GTK_BOX(box), create_title("Escoger archivo comprimido"));
    GtkWidget *directory_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    page->directory_label = gtk_label_new("Ningún archivo seleccionado");
    gtk_widget_set_hexpand(page->directory_label, TRUE);

    gtk_widget_set_halign(page->directory_label, GTK_ALIGN_START);

    GtkWidget *search_button = gtk_button_new_with_label("Buscar archivo");
    g_signal_connect(search_button, "clicked", G_CALLBACK(on_decompression_search_clicked),
        page);
    gtk_box_append(GTK_BOX(directory_row), page->directory_label);
    gtk_box_append(GTK_BOX(directory_row), search_button);
    gtk_box_append(GTK_BOX(box), directory_row);

    // Parte del directorio de destino
    gtk_box_append(GTK_BOX(box), create_title("Escoger directorio de destino"));
    GtkWidget *output_directory_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    page->output_directory_label = gtk_label_new("Ningún directorio seleccionado");
    gtk_widget_set_hexpand(page->output_directory_label, TRUE);
    gtk_widget_set_halign(page->output_directory_label, GTK_ALIGN_START);

    GtkWidget *output_directory_button = gtk_button_new_with_label("Buscar directorio");
    g_signal_connect(output_directory_button, "clicked",
        G_CALLBACK(on_decompression_output_search_clicked), page);
    gtk_box_append(GTK_BOX(output_directory_row), page->output_directory_label);
    gtk_box_append(GTK_BOX(output_directory_row), output_directory_button);
    gtk_box_append(GTK_BOX(box), output_directory_row);

    // El botón de inicio de descompresión
    GtkWidget *decompress_button = gtk_button_new_with_label("Iniciar descompresión");
    g_signal_connect(decompress_button, "clicked", G_CALLBACK(on_decompress_clicked),
        page);
    gtk_box_append(GTK_BOX(box), decompress_button);

    // Estado del programa
    page->status_label = gtk_label_new("Esperando para iniciar...");

    gtk_widget_set_halign(page->status_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), page->status_label);

    // Barra
    page->progress_bar = gtk_progress_bar_new();

    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(page->progress_bar), TRUE);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "0%");
    gtk_box_append(GTK_BOX(box), page->progress_bar);

    // Tabla de resultados
    gtk_box_append(GTK_BOX(box), create_title("Resultados de Descompresión"));
    gtk_box_append(GTK_BOX(box), create_decompression_table(page));

    return box;
}

// Ventana de estadísticas
static GtkWidget *create_statistics_page(StatisticsPage *page) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);

    gtk_widget_set_margin_top(box, 25);
    gtk_widget_set_margin_bottom(box, 25);
    gtk_widget_set_margin_start(box, 25);
    gtk_widget_set_margin_end(box, 25);

    gtk_box_append(GTK_BOX(box), create_title("Estadísticas Generales"));
    gtk_box_append(GTK_BOX(box), create_subtitle(
            "Aquí se mostrarán los resultados de la última ejecución."));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 30);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Métrica"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Serial"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Procesos"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Hilos"), 3, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Tiempo compresión"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Tiempo descompresión"), 0, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Salud"), 0, 3, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Aceleración compresión"), 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Aceleración descompresión"), 0, 5, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Tamaño original"), 0, 6, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Tamaño comprimido"), 0, 7, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Ratio de compresión"), 0, 8, 1, 1);

    // Al inicio todo está vacío
    for (int row = 0; row < 8; row++) {
        for (int column = 0; column < 3; column++) {
            page->cells[row][column] = gtk_label_new("--");
            gtk_grid_attach(GTK_GRID(grid), page->cells[row][column],
                column + 1, row + 1, 1, 1);
        }
    }

    gtk_box_append(GTK_BOX(box), grid);

    return box;
}

// La ventana inicial
void create_main_window(GtkApplication *app, gpointer user_data) {
    AppWidgets *widgets = g_new0(AppWidgets, 1);
    (void)user_data;

    GtkWidget *window = gtk_application_window_new(app);

    gtk_window_set_title(GTK_WINDOW(window), "Compresor Huffman");

    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_start(main_box, 25);
    gtk_widget_set_margin_end(main_box, 25);
    gtk_widget_set_margin_bottom(main_box, 20);

    // Título
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        "<span size='24000' weight='bold'>"
        "Compresor y Descompresor utilizando Huffman"
        "</span>");

    gtk_box_append(GTK_BOX(main_box), title);

    // Subtítulo
    GtkWidget *subtitle = gtk_label_new(
            "Comparación de las versiones Serial, de Procesos y de Hilos");

    gtk_box_append(GTK_BOX(main_box), subtitle);

    // Las ventanas
    GtkWidget *notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(notebook, TRUE);

    widgets->compression.statistics = &widgets->statistics;
    widgets->decompression.statistics = &widgets->statistics;

    // Compresión
    GtkWidget *compression_page = create_compression_page(&widgets->compression);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), compression_page,
        gtk_label_new("Compresión"));

    // Descompresión
    GtkWidget *decompression_page = create_decompression_page( &widgets->decompression);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), decompression_page,
        gtk_label_new("Descompresión"));


    // Estadísticas
    GtkWidget *statistics_page = create_statistics_page(&widgets->statistics);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), statistics_page,
        gtk_label_new("Estadísticas"));

    gtk_box_append(GTK_BOX(main_box), notebook);

    gtk_window_set_child(GTK_WINDOW(window), main_box);
    gtk_window_present(GTK_WINDOW(window));
}