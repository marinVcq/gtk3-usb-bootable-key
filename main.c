#include <gtk/gtk.h>
#include <stdio.h>
#include <unistd.h>
#include <glib.h> 
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <pthread.h>
#include <ctype.h>
#include <stdbool.h>

// Structure: App Hold application widgets
typedef struct {
    GtkBuilder *builder;
    
    // Script output & percent status
    FILE *formatting_script_output;
    FILE *iso_script_output;
    gint formatting_percent_value;
    gint iso_percent_value;
    	
    // Thread ID
    pthread_t formatting_thread_id;     // STORE FORMAT THREAD ID
    pthread_t load_iso_thread_id;       // STORE LOAD ISO THREAD ID
    
    // Curent working directory
    gchar cwd[1024];
    
    // Shell command
    gchar *command;
    
    // Variables useful
    gchar *selected_usb_device; 		// USB NAME
    gchar *selected_format; 			// USB FORMAT
    gchar *selected_iso;				// SELECTED ISO FILE
    gchar *new_device_label;			// NEW USB LABEL
    gchar *device_label;				// FORMER USB LABEL

    
    // Grid Container from builder
    GtkGrid *main_grid;
    GtkGrid *format_grid;
    GtkGrid *select_device_grid;
    GtkGrid *create_bootable_grid;
    
    // Widgets from builder
    GtkProgressBar *format_progress_bar;
    GtkProgressBar *iso_progress_bar;
    GtkImage *header_image;
    GtkLabel *header_label;
    GtkComboBoxText *combobox_usb;
    GtkComboBoxText *combobox_format;
    GtkLabel *label_format_information;
    GtkLabel *label_load_iso_information;
    GtkEntry *entry_new_device_label;
    GtkWidget *file_chooser_iso;
    
    // Buttons from builder
    GtkWidget *format_button;
    GtkWidget *cancel_format_button;
    GtkWidget *load_iso_button;
    GtkWidget *cancel_load_iso_button;
    
} App;

// Flag to signal cancellation
volatile sig_atomic_t cancel_load_iso = 0;
volatile sig_atomic_t cancel_format = 0;

// Function prototypes
App *create_app(GtkBuilder *builder);

void on_combobox_usb_changed(GtkComboBoxText *combobox, App *app);
void on_format_button_clicked(GtkButton *button, gpointer user_data);
void on_cancel_format_button_clicked(GtkButton *button, gpointer user_data);
void on_cancel_load_iso_button_clicked(GtkButton *button, gpointer user_data);
void on_entry_new_device_label_changed(GtkEntry *entry, App *app);
void on_file_chooser_file_set(GtkFileChooserButton *file_chooser, App *app);

gboolean check_for_usb_changes(gpointer user_data);
void populate_usb_devices(GtkComboBoxText *combobox, App *app);
void populate_usb_devices_and_update(App *app);

void handle_child_exit(int signo);

void update_format_UI_information(gint percent, App *app, const char *message);
void update_iso_UI_information(gint percent, App *app, const char *message);

void *formatting_device_async(void *user_data);
void *load_iso_async(void *user_data);

int extract_percent_from_formatting_output(char buffer[256]);
int extract_percent_from_iso_output(char buffer[256], int iso_size);

void cancel_format_thread(App *app);
void cancel_iso_thread(App *app);

// Initialize the application structure
App *create_app(GtkBuilder *builder) {

    App *app = g_new(App, 1);
    app->builder = builder;
    
    // GRID CONTAINER 
    app->main_grid = GTK_GRID(gtk_builder_get_object(builder, "main_grid"));
    app->format_grid = GTK_GRID(gtk_builder_get_object(builder, "format_grid"));
    app->create_bootable_grid = GTK_GRID(gtk_builder_get_object(builder, "create_bootable_grid"));
    app->select_device_grid = GTK_GRID(gtk_builder_get_object(builder, "select_device_grid"));
    
    // HEADER
    app->header_label = GTK_LABEL(gtk_builder_get_object(builder, "header_label"));
    app->header_image = GTK_IMAGE(gtk_builder_get_object(builder, "header_image"));
    
    // COMBOBOX
    app->combobox_usb = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "combobox_usb"));
    app->combobox_format = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "combobox_format"));
    
    gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(app->combobox_format), 0, "Select Format");
    gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(app->combobox_format), 1, "NTFS");
    gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(app->combobox_format), 2, "VFAT");
    gtk_combo_box_text_insert_text(GTK_COMBO_BOX_TEXT(app->combobox_format), 3, "EXT4");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->combobox_format), 0); 
    
    // ENTRIES
    app->entry_new_device_label = GTK_ENTRY(gtk_builder_get_object(builder, "entry_new_device_label"));
    
    // FILE BROWSER
    app->file_chooser_iso = GTK_WIDGET(gtk_builder_get_object(builder, "file_chooser_iso"));
    
	// VARIABLES
	app->formatting_percent_value = 0;
	app->iso_percent_value = 0;
	
	// BUTTONS
    app->format_button = GTK_WIDGET(gtk_builder_get_object(builder, "format_button"));
    app->cancel_format_button = GTK_WIDGET(gtk_builder_get_object(app->builder, "cancel_format_button"));
    app->load_iso_button = GTK_WIDGET(gtk_builder_get_object(builder, "load_iso_button"));
    app->cancel_load_iso_button = GTK_WIDGET(gtk_builder_get_object(builder, "cancel_load_iso_button"));
    
    // LABEL
    app->label_format_information = GTK_LABEL(gtk_builder_get_object(builder, "label_format_information"));
    app->label_load_iso_information = GTK_LABEL(gtk_builder_get_object(builder, "label_load_iso_information"));  
    
    // PROGRESS BAR
    app->format_progress_bar = GTK_PROGRESS_BAR(gtk_builder_get_object(builder, "format_progress_bar"));
    app->iso_progress_bar = GTK_PROGRESS_BAR(gtk_builder_get_object(builder, "iso_progress_bar"));

    // Initialize the selected USB device to NULL
    app->selected_usb_device = NULL;
    app->selected_format = NULL;
    app->new_device_label = NULL;
    app->device_label = NULL;
    app->command = NULL;
    
    return app;
}

// Free allocate memory -- A MESS 
void destroy_app(App *app) {
    g_free(app->selected_usb_device);
    g_free(app->selected_format);
    g_free(app->new_device_label);
    g_free(app->device_label);

    g_free(app);
}

// Function to get the size of a file in GB (rounded to the nearest integer)
int get_file_size_gb(const char *file_path) {
    struct stat file_stat;

    // Use stat to get file information
    if (stat(file_path, &file_stat) == 0) {
        // Convert bytes to gigabytes (rounded to the nearest integer)
        int size_gb = (int)((file_stat.st_size + 1e9 - 1) / 1e9);
        return size_gb;
    }

    // Return -1 if there was an error
    return -1;
}
// Function: Signal handler for SIGCHLD
void handle_child_exit(int signo) {
    pid_t pid;
    int status;

    // Reap all terminated child processes
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Child process %d exited with status %d\n", pid, status);
    }
}

// Function to set the iso path
void on_file_chooser_file_set(GtkFileChooserButton *file_chooser, App *app) {
    // Get the selected file path
    app->selected_iso = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
    //g_print("Selected file path: %s\n", app->selected_iso);
}

// Function: Update UI information for format feature
void update_format_UI_information(gint percent, App *app, const char *message) {

    gdouble fraction = (gdouble)percent / 100.0;
    gtk_label_set_text(app->label_format_information, message);
    gtk_progress_bar_set_fraction(app->format_progress_bar, fraction);
    
    // Process GTK events to update the GUI
    while (gtk_events_pending()) {
        gtk_main_iteration_do(FALSE);
    }
}

// Function: Update UI information for format feature
void update_iso_UI_information(gint percent, App *app, const char *message) {

    gdouble fraction = (gdouble)percent / 100.0;
    gtk_label_set_text(app->label_load_iso_information, message);
    gtk_progress_bar_set_fraction(app->iso_progress_bar, fraction);
    
    // Process GTK events to update the GUI
    while (gtk_events_pending()) {
        gtk_main_iteration_do(FALSE);
    }
}

// Function: Extract Percentage from formatting script output
int extract_percent_from_formatting_output(char buffer[256]) {
    const char *percentStr = strstr(buffer, "%"); // Find the '%' character
    if (percentStr != NULL) {
        int percent;
        sscanf(percentStr - 4, "%d", &percent);
        return percent;
    }
    return -1; // If '%' is not found
}

// Function: Extract Percentage from iso script output
int extract_percent_from_iso_output(char buffer[256], int iso_size) {
    const char *gbStr = strstr(buffer, "GB"); // Find the 'GB' substring

    if (gbStr != NULL) {
        float gb;
        sscanf(gbStr - 4, "%f", &gb);
        int gbInt = (int)gb; // Cast the float to an integer

        if (iso_size > 0) {
            double percent = (double)(gbInt * 100) / iso_size;
            int roundedPercent = (int)(percent + 0.5); // Round to the nearest whole number

            // Assuming g_print is a custom print function, replace it with your actual print function
            //g_print("Percentage: %d%%\n", roundedPercent);

            return roundedPercent;
        } else {
            // Avoid division by zero
            g_print("Error: ISO size is not valid (zero or negative).\n");
            return -1;
        }
    }

    // If 'GB' is not found, check for the success message
    const char *successStr = strstr(buffer, "Bootable USB created successfully");
    if (successStr != NULL) {
        return 100;
    }

    // If neither 'GB' nor success message is found
    g_print("Error: 'GB' not found in the buffer, and Bootable USB creation message not found.\n");
    return -1;
}


// Function: Run the shred & mkfs commands asynchronously and format the selected device
void *formatting_device_async(void *user_data) {

    App *app = (App *)user_data;
    
    // Make the cancel button visible before starting the formatting process
    gtk_widget_set_visible(app->cancel_format_button, TRUE);
    gtk_widget_set_visible(app->format_button, FALSE);
	
	// Get the current working directory
    if (getcwd(app->cwd, sizeof(app->cwd)) != NULL) {
        // Print the current working directory
        //printf("Current working directory: %s\n", app->cwd);
		app->command = g_strdup_printf("pkexec sh -c '%s/format_usb.sh %s true %s %s'", app->cwd, app->selected_usb_device,g_utf8_strdown(app->selected_format, -1),app->new_device_label);
    }
	
	// Open the pipe for reading
    app->formatting_script_output = popen(app->command, "r");
    if (app->formatting_script_output == NULL) {
        g_printerr("Error running combined command\n");
        g_free(app->command);
        return NULL;
    }

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), app->formatting_script_output) != NULL) {
		//g_print("Buffer: %s", buffer);
		fflush(stdout);
		
		// Check for format cancellation
        if (cancel_format) {
            pclose(app->formatting_script_output);
            return NULL;
        }

		// Parse output to retrieve the percent status
		gchar *info_message = g_strdup(buffer);  // Allocate new memory and copy buffer
		app->formatting_percent_value = extract_percent_from_formatting_output(buffer);
		update_format_UI_information((gdouble)app->formatting_percent_value, app, info_message);
		g_free(info_message);  // Now free the allocated memory
	}

    // Close the process
    int pclose_status = pclose(app->formatting_script_output);
    
    // Hide the cancel button visible AFTER the formatting process
    gtk_widget_set_visible(app->cancel_format_button, FALSE);
    gtk_widget_set_visible(app->format_button, TRUE);

    // Free resources
    g_free(app->command);

    return NULL;
}

// Function: load iso run script asyncronously
void *load_iso_async(void *user_data) {

    App *app = (App *)user_data;
    int iso_size_gb = get_file_size_gb(app->selected_iso); // Should be set out of here btw
    
    gtk_widget_set_visible(app->cancel_load_iso_button, TRUE);
    gtk_widget_set_visible(app->load_iso_button, FALSE);
	
	// Get the current working directory
    if (getcwd(app->cwd, sizeof(app->cwd)) != NULL) {
		app->command = g_strdup_printf("pkexec sh -c '%s/load_iso.sh %s %s'", app->cwd, app->selected_usb_device, app->selected_iso);
    }
	
	// Open the pipe for reading
    app->iso_script_output = popen(app->command, "r");
    if (app->iso_script_output == NULL) {
        g_printerr("Error running combined command\n");
        g_free(app->command);
        return NULL;
    }

	char buffer[256];
	while (fgets(buffer, sizeof(buffer), app->iso_script_output) != NULL) {
		fflush(stdout);
		
		// Check for format cancellation
        if (cancel_load_iso) {
            pclose(app->iso_script_output);
            return NULL;
        }
        
		// Parse output to retrieve the percent status
		app->iso_percent_value = extract_percent_from_iso_output(buffer, iso_size_gb);
		gchar *info_message = g_strdup_printf("%d%% copied... Please Wait",app->iso_percent_value );  
		
		if(app->iso_percent_value > 0){
			update_iso_UI_information((gdouble)app->iso_percent_value, app, info_message);
		}
		
		if(app->iso_percent_value == 100){
			update_iso_UI_information((gdouble)app->iso_percent_value, app, "Bootable USB created successfully!");
		}
		
		g_free(info_message);  // Now free the allocated memory
	}

    // Close the process
    int pclose_status = pclose(app->iso_script_output);
    
    // Hide the cancel button visible AFTER the formatting process
    gtk_widget_set_visible(app->cancel_load_iso_button, FALSE);
    gtk_widget_set_visible(app->load_iso_button, TRUE);

    // Free resources
    g_free(app->command);

    return NULL;
}

// Function to cancel the formatting thread
void cancel_format_thread(App *app) {
    if (app->formatting_thread_id != 0) {
        pthread_cancel(app->formatting_thread_id);
        app->formatting_thread_id = 0;
    }
}

// Function to cancel the iso thread
void cancel_iso_thread(App *app) {
    if (app->load_iso_thread_id != 0) {
        pthread_cancel(app->load_iso_thread_id);
        app->load_iso_thread_id = 0;
    }
}

// Callback for cancel button clicked
void on_cancel_format_button_clicked(GtkButton *button, gpointer user_data) {
	App *app = (App *)user_data;
    // Set the cancellation flag
    cancel_format = 1;
    cancel_format_thread(app);
    gchar *cancel_message = "Formatting process aborted!";
    update_format_UI_information((gdouble)0, app, cancel_message);
    // Hide the cancel button visible AFTER the formatting process
    gtk_widget_set_visible(app->cancel_format_button, FALSE);
    gtk_widget_set_visible(app->format_button, TRUE);
}

// Callback for cancel button clicked
void on_cancel_load_iso_button_clicked(GtkButton *button, gpointer user_data) {
	App *app = (App *)user_data;
    // Set the cancellation flag
    cancel_load_iso = 1;
    cancel_iso_thread(app);
    gchar *cancel_message = "ISO copying process aborted!";
    update_iso_UI_information((gdouble)0, app, cancel_message);
    // Hide the cancel button visible AFTER the formatting process
    gtk_widget_set_visible(app->cancel_load_iso_button, FALSE);
    gtk_widget_set_visible(app->load_iso_button, TRUE);
}

// Callback function on_format_button_clicked function
void on_format_button_clicked(GtkButton *button, gpointer user_data) {
    // Get App structure (Format and selected devices)
    App *app = (App *)user_data;

    // Check if a USB device and format are selected
    if (app->selected_usb_device == NULL || app->selected_format == NULL) {
        g_print("Error: USB device or format not selected\n");
        return;
    }

    // Create a thread to call the formatting function asynchronously
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, formatting_device_async, app) != 0) {
        g_printerr("Error creating thread\n");
        g_free(app->new_device_label);
        return;
    }
    
    // Save the thread ID in the App structure
	app->formatting_thread_id = thread_id;

    // Detach the thread to allow it to run independently
    pthread_detach(app->formatting_thread_id);
}

// Callback function on_load_iso_button_clicked
void on_load_iso_button_clicked(GtkButton *button, gpointer user_data) {
    // Get App structure (Format and selected devices)
    App *app = (App *)user_data;

    // Check if a USB device and format are selected
    if (app->selected_usb_device == NULL) {
        g_print("Error: USB device not selected\n");
        return;
    }

    // Create a thread to call the load iso function asynchronously
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, load_iso_async, app) != 0) {
        g_printerr("Error creating thread\n");
        return;
    }
    
    // Save the thread ID in the App structure
	app->load_iso_thread_id = thread_id;

    // Detach the thread to allow it to run independently
    pthread_detach(app->load_iso_thread_id);
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
    //g_print("Selected USB device: %s\n", app->selected_usb_device);
}

void on_entry_new_device_label_changed(GtkEntry *entry, App *app){
    // Get the new text from the entry
    const gchar *new_text = gtk_entry_get_text(entry);

    // Update app->new_device_label with the new text
    g_free(app->new_device_label);
    app->new_device_label = g_strdup(new_text);

    // Print the updated label to the console (optional)
    //g_print("New Device Label: %s\n", app->new_device_label);
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

// Function ynamically update the USB devices list
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
    gtk_widget_set_name(GTK_WIDGET(app->format_grid), "format-grid");
    gtk_widget_set_name(GTK_WIDGET(app->header_image), "header_image");
    gtk_widget_set_name(GTK_WIDGET(app->create_bootable_grid), "create-bootable-grid");
    gtk_widget_set_name(GTK_WIDGET(app->main_grid), "main-grid");
    gtk_widget_set_name(GTK_WIDGET(app->select_device_grid), "select-device-grid");
    gtk_widget_set_name(GTK_WIDGET(app->format_button), "format-button");
    gtk_widget_set_name(GTK_WIDGET(app->cancel_format_button), "cancel-format-button");
    
    // Populate USB devices in the combo box
    populate_usb_devices(app->combobox_usb, app);

    // Signal Event connect
    g_signal_connect(app->format_button, "clicked", G_CALLBACK(on_format_button_clicked), app);
    g_signal_connect(app->cancel_format_button, "clicked", G_CALLBACK(on_cancel_format_button_clicked), app);
    
    g_signal_connect(app->load_iso_button, "clicked", G_CALLBACK(on_load_iso_button_clicked), app);
    g_signal_connect(app->cancel_load_iso_button, "clicked", G_CALLBACK(on_cancel_load_iso_button_clicked), app);
    
    g_signal_connect(app->combobox_usb, "changed", G_CALLBACK(on_combobox_usb_changed), app);
    g_signal_connect(app->combobox_format, "changed", G_CALLBACK(on_combobox_format_changed), app);
    g_signal_connect(app->entry_new_device_label, "changed", G_CALLBACK(on_entry_new_device_label_changed), app);
    g_signal_connect(app->file_chooser_iso, "file-set", G_CALLBACK(on_file_chooser_file_set), app);



    gtk_main();

    // Destroy the application structure
    destroy_app(app);

    return 0;
}

