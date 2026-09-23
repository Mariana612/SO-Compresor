#include <gtk/gtk.h>
#include "interfaz.h"
#include "../Code/Compressor.h"
#include "../Code/Decompressor.h"

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
} DecompressionPage;

// Tablas de Compresión y Descompresión
typedef struct {
    CompressionPage compression;
    DecompressionPage decompression;
} AppWidgets;

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
    (void)button;

    if (page->selected_path == NULL) {
        gtk_label_set_text(GTK_LABEL(page->status_label),
            "Seleccione un directorio antes de comprimir.");
        return;
    }

    gtk_label_set_text(GTK_LABEL(page->status_label), "Comprimiendo con la versión serial...");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(page->progress_bar), 0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "En progreso");

    if (!compress_directory(page->selected_path)) {
        gtk_label_set_text(GTK_LABEL(page->status_label), "Error durante la compresión serial.");
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "Error");
        return;
    }

    gtk_label_set_text(GTK_LABEL(page->status_label), "Compresión serial terminada.");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(page->progress_bar), 1.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "100%");
    gtk_label_set_text(GTK_LABEL(page->serial_time), "Completado");
    gtk_label_set_text(GTK_LABEL(page->process_time), "Pendiente");
    gtk_label_set_text(GTK_LABEL(page->thread_time), "Pendiente");
    gtk_label_set_text(GTK_LABEL(page->serial_speedup), "--");
    gtk_label_set_text(GTK_LABEL(page->process_speedup), "--");
    gtk_label_set_text(GTK_LABEL(page->thread_speedup), "--");
}

// Botón de descompresión se presiona
static void on_decompress_clicked(GtkButton *button, gpointer user_data) {
    DecompressionPage *page = user_data;
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
        "Descomprimiendo con la versión serial...");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(page->progress_bar), 0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "En progreso");

    if (!decompress_file(page->selected_path, page->output_directory_path)) {
        gtk_label_set_text(GTK_LABEL(page->status_label),
            "Error durante la descompresión serial.");
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "Error");
        return;
    }

    gtk_label_set_text(GTK_LABEL(page->status_label), "Descompresión serial terminada.");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(page->progress_bar), 1.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(page->progress_bar), "100%");
    gtk_label_set_text(GTK_LABEL(page->serial_time), "Completado");
    gtk_label_set_text(GTK_LABEL(page->process_time), "Pendiente");
    gtk_label_set_text(GTK_LABEL(page->thread_time), "Pendiente");
    gtk_label_set_text(GTK_LABEL(page->health_serial), "Correcto");
    gtk_label_set_text(GTK_LABEL(page->health_process), "Pendiente");
    gtk_label_set_text(GTK_LABEL(page->health_thread), "Pendiente");
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
static GtkWidget *create_statistics_page(void) {
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
    for (int row = 1; row <= 8; row++) {
        for (int column = 1; column <= 3; column++) {
            gtk_grid_attach(GTK_GRID(grid), gtk_label_new("--"), column, row, 1, 1);
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

    // Compresión
    GtkWidget *compression_page = create_compression_page(&widgets->compression);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), compression_page,
        gtk_label_new("Compresión"));

    // Descompresión
    GtkWidget *decompression_page = create_decompression_page( &widgets->decompression);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), decompression_page,
        gtk_label_new("Descompresión"));


    // Estadísticas
    GtkWidget *statistics_page = create_statistics_page();

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), statistics_page,
        gtk_label_new("Estadísticas"));

    gtk_box_append(GTK_BOX(main_box), notebook);

    gtk_window_set_child(GTK_WINDOW(window), main_box);
    gtk_window_present(GTK_WINDOW(window));
}