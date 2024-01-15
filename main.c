#include <gtk/gtk.h>
#include <stdio.h>

void on_format_button_clicked(GtkButton *button, gpointer user_data) {
    // Add your formatting logic here
    g_print("Formatting USB device...\n");
}

void populate_usb_devices(GtkComboBoxText *combobox) {
    FILE *lsblk_output = popen("lsblk -I 8 -dnpo NAME,TRAN", "r");
    if (lsblk_output == NULL) {
        g_printerr("Error opening lsblk command\n");
        return;
    }

    char buffer[256];
    gboolean usb_found = FALSE;

    // Clear existing items
    gtk_combo_box_text_remove_all(combobox);
    gtk_combo_box_text_insert_text(combobox, 0, "Select USB Device");

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

    // Set the first item as active
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 0);
}


int main(int argc, char *argv[]) {
    GtkBuilder *builder;
    GObject *window;
    GObject *header_label;
    GObject *combobox_usb;
    GObject *combobox_format;
    GObject *button_format;
    GObject *button_quit;
    GError *error = NULL;
    
    gtk_init(&argc, &argv);

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
    
    // App Header
    header_label = gtk_builder_get_object(builder, "header_label");
    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(header_label));
    gtk_widget_set_name(GTK_WIDGET(header_label), "custom-header-label");

	
	// Label & Combobox USB device selection builder
	GObject *label_combobox_usb = gtk_builder_get_object(builder, "label_combo_usb");
    combobox_usb = gtk_builder_get_object(builder, "combobox_usb");
    
    // Label & Combobox Format selection 
    GObject *label_combobox_format = gtk_builder_get_object(builder, "label_combo_format");
    combobox_format = gtk_builder_get_object(builder, "combobox_format");
    gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(combobox_format), 0, "Select Format");
    gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(combobox_format), 1, "NTFS");
    gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(combobox_format), 2, "FAT32");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_format), 0);


    // Populate USB devices in the combo box
    populate_usb_devices(GTK_COMBO_BOX_TEXT(combobox_usb));

    button_format = gtk_builder_get_object(builder, "format");
    g_signal_connect(button_format, "clicked", G_CALLBACK(on_format_button_clicked), NULL);

    button_quit = gtk_builder_get_object(builder, "quit");
    g_signal_connect(button_quit, "clicked", G_CALLBACK(gtk_main_quit), NULL);

    gtk_main();

    return 0;
}

