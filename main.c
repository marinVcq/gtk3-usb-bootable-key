#include <gtk/gtk.h>
#include <stdio.h>
#include <unistd.h>
#include <glib.h> 
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <ctype.h>


GMutex mutex;

// Structure: Hold data of the shred command
typedef struct {
    gchar *shred_command;
    gchar *shred_message;
    FILE *shred_output;
    gint shred_percent;
} ShredData;

// Structure: Hold data of the mkfs command
typedef struct {
    gchar *mkfs_command;
    gchar *mkfs_message;
    FILE *mkfs_output;
    gint mkfs_percent;
} MkfsData;

// Structure: App Hold application widgets
typedef struct {
    GtkBuilder *builder;
    
    // Variables useful
    gchar *selected_usb_device; 		// USB NAME
    gchar *selected_format; 			// USB FORMAT
    gchar *selected_iso;				// SELECTED ISO FILE
    gint format_progress_bar_value ; 	// FORMAT: PROGRESS BAR VAL
    gint iso_progress_bar_value ;		// ISO : PROGRESS BAR VAL
    ShredData *shred_data;				// STORE SHRED INFO
    MkfsData *mkfs_data; 				// STORE MKFS INFO
    
    // Grid Container from builder
    GtkGrid *main_grid;
    GtkGrid *format_grid;
    GtkGrid *select_device_grid;
    GtkGrid *create_bootable_grid;
    
    // Components from builder
    GtkProgressBar *progress_bar;
    GtkProgressBar *bootable_progress_bar;
    GtkImage *header_image;
    GtkLabel *header_label;
    GtkComboBoxText *combobox_usb;
    GtkComboBoxText *combobox_format;
    GtkLabel *label_format_information;
    GtkLabel *label_bootable_information;
    
    // Buttons from builder
    GtkButton *format_button;
    GtkButton *create_bootable_button;
    
} App;

// Function prototypes
void on_combobox_usb_changed(GtkComboBoxText *combobox, App *app);
gboolean check_for_usb_changes(gpointer user_data);
void populate_usb_devices(GtkComboBoxText *combobox, App *app);

// Function: Signal handler for SIGCHLD
void handle_child_exit(int signo) {
    pid_t pid;
    int status;

    // Reap all terminated child processes
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Child process %d exited with status %d\n", pid, status);
    }
}

// Function: Update UI information for format feature
void update_format_UI_information(gint percent, App *app, const char *message) {

    gdouble fraction = (gdouble)percent / 100.0;
    gtk_label_set_text(app->label_format_information, message);
    gtk_progress_bar_set_fraction(app->progress_bar, fraction);
    
    // Process GTK events to update the GUI
    while (gtk_events_pending()) {
        gtk_main_iteration_do(FALSE);
    }
    
    g_print("update progress bar\n");
}


// Function: Format usb device
void format_usb_device(App *app) {

    // Build the mkfs command based selected format and selected device
    gchar *mkfs_command;
    if (g_strcmp0(app->selected_format, "FAT32") == 0) {
        mkfs_command = g_strdup_printf("mkfs.vfat %s", app->selected_usb_device);
    } else if (g_strcmp0(app->selected_format, "NTFS") == 0) {
        mkfs_command = g_strdup_printf("mkfs.ntfs %s", app->selected_usb_device);
    } else {
        g_print("Error: Unsupported format\n");
        return;
    }

    // Execute the mkfs command using popen
    FILE *mkfs_output = popen(mkfs_command, "r");
    if (mkfs_output == NULL) {
        g_printerr("Error running mkfs command\n");
        g_free(mkfs_command);
        return;
    }

    // Read and print the output of the mkfs command
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), mkfs_output) != NULL) {
        g_print("Formatting: %s\n", buffer);
    }

    // Close the mkfs process
    pclose(mkfs_output);

    // Free the mkfs command string
    g_free(mkfs_command);

    g_print("FORMATTING ENDED...\n"); // Temp
}

void format_usb_device_async(App *app) {
    g_idle_add((GSourceFunc)format_usb_device, app);
}

// Function: Extract Percentage from shred output
int extract_percent_from_shred_output(char buffer[256]) {
    const char *percentStr = strstr(buffer, "%"); // Find the '%' character
    if (percentStr != NULL) {
        int percent;
        sscanf(percentStr - 4, "%d", &percent); // Assuming the percentage is always two digits
        return percent;
    }
    return -1; // If '%' is not found
}

// Function: Extract Percentage from mkfs output
int extract_percent_from_mkfs_output(char buffer[256]) {
    return 100;// If '%' is not found
}

// Function: Run the shred & mkfs command asynchronously and format the selected device
void *formatting_device_async(void *user_data) {
    App *app = (App *)user_data;

    // Build the shred command with error redirection
    gchar *shred_command_with_redirect = g_strdup_printf("%s 2>&1", app->shred_data->shred_command);

    // Open the pipe for reading
    app->shred_data->shred_output = popen(shred_command_with_redirect, "r");
    if (app->shred_data->shred_output == NULL) {
        g_printerr("Error running shred command\n");
        g_free(app->shred_data->shred_command);
        g_free(app->shred_data);
        g_free(shred_command_with_redirect);
        return NULL;
    }

    // Read and print the output of the shred command
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), app->shred_data->shred_output) != NULL) {
        g_print("Buffer: %s", buffer);
        fflush(stdout);
        
        // Parse output to retrive the percent status
		app->shred_data->shred_percent = extract_percent_from_shred_output(buffer);
		
		// Update format information 
		if (app->shred_data->shred_percent != -1) {
		    printf("Percentage: %d\n", app->shred_data->shred_percent);
		    app->shred_data->shred_message = g_strdup_printf("Overwriting your device with shred command...\nStatus: %d%%", app->shred_data->shred_percent);
		    update_format_UI_information((gdouble)app->shred_data->shred_percent, app, app->shred_data->shred_message);
		    g_free(app->shred_data->shred_message);
		}else {
		    printf("Overwriting your device with shred command...\nStatus: 0%%");
		}
    }

    if (feof(app->shred_data->shred_output)) {
    	
    	// Inform User shred command succesfully execute
    	app->shred_data->shred_message = g_strdup_printf("Overwriting your device with shred command...\nStatus: Completed");
        update_format_UI_information((gdouble)app->shred_data->shred_percent, app, app->shred_data->shred_message);
        
        // Shred command completed lets run mkfs command and format the device
        // g_idle_add((GSourceFunc)format_usb_device_async, app); -- Not in use for now
        
		// Build the mkfs command based selected format and selected device
		if (g_strcmp0(app->selected_format, "FAT32") == 0) {
		    app->mkfs_data->mkfs_command = g_strdup_printf("pkexec mkfs.vfat -v %s", app->selected_usb_device);
		} else if (g_strcmp0(app->selected_format, "NTFS") == 0) {
		    app->mkfs_data->mkfs_command = g_strdup_printf("mkfs.ntfs -v %s", app->selected_usb_device);
		} else {
		    g_print("Error: Unsupported format\n");
		}

		// Execute the mkfs command using popen
		app->mkfs_data->mkfs_output = popen(app->mkfs_data->mkfs_command, "r");
		if (app->mkfs_data->mkfs_output == NULL) {
		    g_printerr("Error running mkfs command\n");
		    g_free(app->mkfs_data->mkfs_command);
		    return NULL;
		}

		
	    // Read and print the output of the mkfs command
		char buffer[256];
		while (fgets(buffer, sizeof(buffer), app->mkfs_data->mkfs_output) != NULL) {
		    g_print("Buffer: %s", buffer);
		    fflush(stdout);
		    
		    // Parse output to retrive the percent status
			app->mkfs_data->mkfs_percent = extract_percent_from_mkfs_output(buffer);
			
			// Update format information
			if (app->mkfs_data->mkfs_percent != -1) {
				printf("Percentage: %d\n", app->mkfs_data->mkfs_percent);
				app->mkfs_data->mkfs_message = g_strdup_printf("Building file system with mkfs command...\nStatus: %d%%", app->mkfs_data->mkfs_percent);
				update_format_UI_information((gdouble)app->mkfs_data->mkfs_percent, app, app->mkfs_data->mkfs_message);
				g_free(app->mkfs_data->mkfs_message);
			}else {
				printf("Overwriting your device with shred command...\nStatus: 0%%");
			}
		}

		// Close the mkfs process
		pclose(app->mkfs_data->mkfs_output);
		
		app->mkfs_data->mkfs_message = g_strdup_printf("Formatting completed");
		update_format_UI_information((gdouble)app->mkfs_data->mkfs_percent, app, app->mkfs_data->mkfs_message);

		// Free the mkfs command string
		g_free(app->mkfs_data->mkfs_command);
        
    } else if (ferror(app->shred_data->shred_output)) {
        g_print("Error reading file (ferror)\n");
        perror("fgets");
    } else {
        g_print("Buffer reading loop exited normally\n");
    }
    
    // Close the pipe and store status
    int pclose_status = pclose(app->shred_data->shred_output);

    // Free the shred command string and data structure
    g_free(app->shred_data->shred_command);
    g_free(app->shred_data);
    g_free(shred_command_with_redirect); // Free the modified command

    // Return NULL as required by pthread_create
    return NULL;
}


// Modified on_format_button_clicked function
void on_format_button_clicked(GtkButton *button, gpointer user_data) {
    // Get App structure (Format and selected devices)
    App *app = (App *)user_data;

    // Check if a USB device and format are selected
    if (app->selected_usb_device == NULL || app->selected_format == NULL) {
        g_print("Error: USB device or format not selected\n");
        return;
    }

    // Build the shred command
    gchar *shred_command = g_strdup_printf("pkexec shred -v -n 1 -s 1G --random-source=/dev/urandom %s", app->selected_usb_device);

    // Create a data structure to pass to the thread
    ShredData *shred_data = g_new(ShredData, 1);
    app->shred_data = shred_data;
    app->shred_data->shred_command = shred_command;

    // Create a thread to run the shred command asynchronously
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, formatting_device_async, app) != 0) {
        g_printerr("Error creating thread\n");
        g_free(shred_command);
        g_free(app->shred_data);
        return;
    }

    // Detach the thread to allow it to run independently
    pthread_detach(thread_id);
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

// Signal handler for Format selection change
void on_combobox_format_changed(GtkComboBoxText *combobox, App *app) {
    // Get the selected format
    const gchar *selected_format = gtk_combo_box_text_get_active_text(combobox);
    
    if (selected_format != NULL) {
        // Free the previously selected format
        g_free(app->selected_format);
        // Store the selected format in the App structure
        app->selected_format = g_strdup(selected_format);    
    }

    // Print the selected format to the console
    g_print("Selected Format: %s\n", app->selected_format);
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
    app->selected_format = NULL;


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
    
        // Set up signal handler for SIGCHLD
    signal(SIGCHLD, handle_child_exit);

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
    g_signal_connect(app->combobox_format, "changed", G_CALLBACK(on_combobox_format_changed), app);
    app->label_format_information = GTK_LABEL(gtk_builder_get_object(builder, "label_format_information"));


    GObject *button_quit = gtk_builder_get_object(builder, "quit");
    g_signal_connect(button_quit, "clicked", G_CALLBACK(gtk_main_quit), NULL);

    gtk_main();

    // Destroy the application structure
    destroy_app(app);

    return 0;
}

