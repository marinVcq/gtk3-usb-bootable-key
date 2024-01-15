#include <gtk/gtk.h>
#include <stdio.h>
#include <unistd.h>
#include <glib.h> 

// Define a structure to hold your application's widgets
typedef struct {
    GtkBuilder *builder;
    
    // Variable to store the selected USB device
    gchar *selected_usb_device;
    
    // Container
    GtkGrid *main_grid;
    GtkGrid *format_grid;
    GtkGrid *select_device_grid;
    GtkGrid *create_bootable_grid;
    
    // Components
    GtkProgressBar *progress_bar;
    GtkProgressBar *bootable_progress_bar;
    GtkImage *header_image;
    
    GtkLabel *header_label;
    GtkComboBoxText *combobox_usb;
    GtkComboBoxText *combobox_format;
    GtkLabel *label_format_information;
    GtkLabel *label_bootable_information;
    
    // Buttons
    GtkButton *format_button;
    GtkButton *create_bootable_button;

} App;

void on_combobox_usb_changed(GtkComboBoxText *combobox, App *app);
gboolean check_for_usb_changes(gpointer user_data);
void populate_usb_devices(GtkComboBoxText *combobox, App *app);

gboolean simulate_formating(gpointer user_data) {
    App *app = (App *)user_data;
    const char *process_message = "Formatting USB device... ";
    gtk_label_set_text(app->label_format_information, process_message);

    // Counter to simulate progress
    static int counter = 0;

    // Increment the counter at regular intervals
    counter++;

    // Update the progress bar
    double fraction = (double)counter / 5.0;  // Assuming 5 seconds is the total duration
    gtk_progress_bar_set_fraction(app->progress_bar, fraction);

    if (counter < 5) {
        // If the counter is less than 5, continue simulating
        return TRUE;
    } else {
        // After 5 seconds, decide the message based on success or failure
        const char *message;
        gboolean success = TRUE;  // Simulating success, you should set this based on your actual logic
        if (success) {
            message = "Message: Formatting successfully!";
            // Stop the progress bar
            gtk_progress_bar_set_fraction(app->progress_bar, 1.0);
        } else {
            message = "Message: Formatting failed!";
        }

        // Update the label with the message
        gtk_label_set_text(app->label_format_information, message);

        g_print("Formatting USB device... %s\n", message);

        // Returning FALSE removes the timeout source
        return FALSE;
    }
}

// Example usage in on_format_button_clicked
void on_format_button_clicked(GtkButton *button, gpointer user_data) {
    App *app = (App *)user_data;

    // Reset the counter
    static int counter = 0;
    counter = 0;

    // Start the timeout to simulate formatting
    g_timeout_add_seconds(1, simulate_formating, app);
}

// Signal handler for USB device selection change
void on_combobox_usb_changed(GtkComboBoxText *combobox, App *app) {

    // Get the selected USB device
    const gchar *selected_device = gtk_combo_box_text_get_active_text(combobox);
    
    if(selected_device != NULL){
		// Free the previously selected USB device
		g_free(app->selected_usb_device);
		// Store the selected USB device in the App structure
		app->selected_usb_device = g_strdup(selected_device);    
    }

    // Print the selected USB device to the console
    g_print("Selected USB device: %s\n", app->selected_usb_device);
}

// New function to dynamically update the USB devices list
void populate_usb_devices_and_update(App *app) {
    // Check for USB changes and update the list
    populate_usb_devices(app->combobox_usb, app);
}

// Periodic function to check for USB changes
gboolean check_for_usb_changes(gpointer user_data) {
    App *app = (App *)user_data;

    // Check for USB changes and update the list
    populate_usb_devices_and_update(app);

    // Returning TRUE allows the timeout to run periodically
    return TRUE;
}

// Modified populate_usb_devices function
void populate_usb_devices(GtkComboBoxText *combobox, App *app) {
    FILE *lsblk_output = popen("lsblk -I 8 -dnpo NAME,TRAN", "r");
    if (lsblk_output == NULL) {
        g_printerr("Error opening lsblk command\n");
        return;
    }

    char buffer[256];
    gboolean usb_found = FALSE;

    // Clear existing items
    gtk_combo_box_text_remove_all(combobox);
    
    if(app->selected_usb_device ==NULL){
    	gtk_combo_box_text_insert_text(combobox, 0, "Select USB Device");
    }else{
        gtk_combo_box_text_insert_text(combobox, 0, app->selected_usb_device);
    }


    while (fgets(buffer, sizeof(buffer), lsblk_output) != NULL) {
        gchar *device_name, *transport_type;
        if (sscanf(buffer, "%ms %ms", &device_name, &transport_type) == 2) {
            if (g_str_equal(transport_type, "usb")) {
                gtk_combo_box_text_append_text(combobox, device_name);
                usb_found = TRUE;
            }
            g_free(device_name);
            g_free(transport_type);
        }
    }

    pclose(lsblk_output);

    // If no USB devices are found, add a default item
    if (!usb_found) {
        gtk_combo_box_text_append_text(combobox, "No USB device detected");
    }

 
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 0);

    // You can set up a periodic check for USB changes using g_timeout_add
    if(app->selected_usb_device == NULL){
    	g_timeout_add_seconds(5, check_for_usb_changes, app);
    }
    
}

// Initialize the application structure
App *create_app(GtkBuilder *builder) {
    App *app = g_new(App, 1);
    app->builder = builder;
    app->main_grid = GTK_GRID(gtk_builder_get_object(builder, "main_grid"));
    app->format_grid = GTK_GRID(gtk_builder_get_object(builder, "format_grid"));
    app->create_bootable_grid = GTK_GRID(gtk_builder_get_object(builder, "create_bootable_grid"));
    app->select_device_grid = GTK_GRID(gtk_builder_get_object(builder, "select_device_grid"));
    app->progress_bar = GTK_PROGRESS_BAR(gtk_builder_get_object(builder, "progress_bar"));
    app->format_button = GTK_BUTTON(gtk_builder_get_object(builder, "format_button"));
    app->header_label = GTK_LABEL(gtk_builder_get_object(builder, "header_label"));
    app->header_image = GTK_IMAGE(gtk_builder_get_object(builder, "header_image"));
    app->combobox_usb = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "combobox_usb"));
    app->combobox_format = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "combobox_format"));
    gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(app->combobox_format), 0, "Select Format");
    gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(app->combobox_format), 1, "NTFS");
    gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(app->combobox_format), 2, "FAT32");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->combobox_format), 0);
    app->label_format_information = GTK_LABEL(gtk_builder_get_object(builder, "label_format_information"));
    
    // Initialize the selected USB device to NULL
    app->selected_usb_device = NULL;

    return app;
}

void destroy_app(App *app) {
	g_free(app->selected_usb_device);
    g_free(app);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkBuilder *builder;
    GObject *window;
    App *app;
    GError *error = NULL;

    builder = gtk_builder_new();
    if (gtk_builder_add_from_file(builder, "builder.ui", &error) == 0) {
        g_printerr("Error loading file: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    // Load CSS file and set the provider
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);
    gtk_css_provider_load_from_file(provider, g_file_new_for_path("style.css"), NULL);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    // Main window builder
    window = gtk_builder_get_object(builder, "window");
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    // Create the application structure
    app = create_app(builder);

    // (for CSS styling)
    gtk_widget_set_name(GTK_WIDGET(app->header_label), "custom-header-label");
    gtk_widget_set_name(GTK_WIDGET(app->label_format_information), "format-information-label");
    gtk_widget_set_name(GTK_WIDGET(app->progress_bar), "progress-bar");
    gtk_widget_set_name(GTK_WIDGET(app->format_grid), "format-grid");
    gtk_widget_set_name(GTK_WIDGET(app->header_image), "header_image");
    gtk_widget_set_name(GTK_WIDGET(app->create_bootable_grid), "create-bootable-grid");
    gtk_widget_set_name(GTK_WIDGET(app->main_grid), "main-grid");
    gtk_widget_set_name(GTK_WIDGET(app->select_device_grid), "select-device-grid");
    gtk_widget_set_name(GTK_WIDGET(app->format_button), "format-button");
    

    // Populate USB devices in the combo box
    populate_usb_devices(app->combobox_usb, app);

    // Button Format event
    g_signal_connect(app->format_button, "clicked", G_CALLBACK(on_format_button_clicked), app);
    g_signal_connect(app->combobox_usb, "changed", G_CALLBACK(on_combobox_usb_changed), app);
    app->label_format_information = GTK_LABEL(gtk_builder_get_object(builder, "label_format_information"));


    GObject *button_quit = gtk_builder_get_object(builder, "quit");
    g_signal_connect(button_quit, "clicked", G_CALLBACK(gtk_main_quit), NULL);

    gtk_main();

    // Destroy the application structure
    destroy_app(app);

    return 0;
}

