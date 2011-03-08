#include "abrtlib.h"
#include <gtk/gtk.h>

static GtkWidget *option_table;
static GtkWidget *parent_dialog;
static int last_row = 0;

enum
{
    COLUMN_EVENT_NAME,
    COLUMN_EVENT,
    NUM_COLUMNS
};

static void show_event_config_dialog(event_config_t* event);

GtkWidget *gtk_label_new_justify_left(const gchar *label_str)
{
    GtkWidget *label = gtk_label_new(label_str);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment(GTK_MISC(label), /*xalign:*/ 0, /*yalign:*/ 0.5);
    return label;
}

static void add_option_to_dialog(event_option_t *option)
{
    GtkWidget *label;
    GtkWidget *option_input;
    GtkWidget *option_hbox = gtk_hbox_new(FALSE, 0);
    switch(option->type)
    {
        case OPTION_TYPE_TEXT:
        case OPTION_TYPE_NUMBER:
            label = gtk_label_new_justify_left(option->label);
            gtk_table_attach(GTK_TABLE(option_table), label,
                             0, 1,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);
            option_input = gtk_entry_new();
            gtk_table_attach(GTK_TABLE(option_table), option_input,
                             1, 2,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);

            break;
        case OPTION_TYPE_BOOL:
            option_input = gtk_check_button_new_with_label(option->label);
            gtk_table_attach(GTK_TABLE(option_table), option_input,
                             0, 2,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);
            break;
        case OPTION_TYPE_PASSWORD:
            label = gtk_label_new_justify_left(option->label);
            gtk_table_attach(GTK_TABLE(option_table), label,
                             0, 1,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);
            option_input = gtk_entry_new();
            gtk_table_attach(GTK_TABLE(option_table), option_input,
                             1, 2,
                             last_row, last_row+1,
                             GTK_FILL, GTK_FILL,
                             0,0);

            gtk_entry_set_visibility(GTK_ENTRY(option_input), 0);
            break;
        default:
            option_input = gtk_label_new_justify_left("WTF?");
            g_print("unsupported option type\n");
    }
    last_row++;

    gtk_widget_show_all(GTK_WIDGET(option_hbox));
}

static void add_option(gpointer data, gpointer user_data)
{
    event_option_t * option = (event_option_t *)data;
    add_option_to_dialog(option);
}

static void on_close_event_list_cb(GtkWidget *button, gpointer user_data)
{
    GtkWidget *window = (GtkWidget *)user_data;
    gtk_widget_destroy(window);
}

static event_config_t *get_event_config_from_row(GtkTreeView *treeview)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    event_config_t *event_config = NULL;
    if (selection)
    {
        GtkTreeIter iter;
        GtkTreeModel *store = gtk_tree_view_get_model(treeview);
        if (gtk_tree_selection_get_selected(selection, &store, &iter) == TRUE)
        {
            GValue value = { 0 };
            gtk_tree_model_get_value(store, &iter, COLUMN_EVENT, &value);
            event_config = (event_config_t*)g_value_get_pointer(&value);
        }
    }
    return event_config;
}

static void on_configure_event_cb(GtkWidget *button, gpointer user_data)
{
    GtkTreeView *events_tv = (GtkTreeView *)user_data;
    event_config_t *ec = get_event_config_from_row(events_tv);
    show_event_config_dialog(ec);
}

static void on_event_row_activated_cb(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    event_config_t *ec = get_event_config_from_row(treeview);
    show_event_config_dialog(ec);
}

static void add_event_to_liststore(gpointer key, gpointer value, gpointer user_data)
{
    GtkListStore *events_list_store = (GtkListStore *)user_data;
    event_config_t *ec = (event_config_t *)value;
    char *event_label = NULL;
    if(ec->name != NULL && ec->description != NULL)
        event_label = xasprintf("<b>%s</b>\n%s", ec->name, ec->description);
    else
        //if event has no xml description
        event_label = xasprintf("<b>%s</b>\nNo description available", key);

    GtkTreeIter iter;
    gtk_list_store_append(events_list_store, &iter);
    gtk_list_store_set(events_list_store, &iter,
                      COLUMN_EVENT_NAME, event_label,
                      COLUMN_EVENT, value,
                      -1);
}

static void show_event_config_dialog(event_config_t* event)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
                        event->name,
                        GTK_WINDOW(parent_dialog),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_OK,
                        GTK_RESPONSE_APPLY,
                        GTK_STOCK_CANCEL,
                        GTK_RESPONSE_CANCEL,
                        NULL);
    if(parent_dialog != NULL)
    {
        gtk_window_set_icon_name(GTK_WINDOW(dialog),
                gtk_window_get_icon_name(GTK_WINDOW(parent_dialog)));
    }
    int length = g_list_length(event->options);
    option_table = gtk_table_new(length, 2, 0);
    g_list_foreach(event->options, &add_option, NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(content), option_table, 0, 0, 10);
    gtk_widget_show_all(option_table);
    int result = gtk_dialog_run(GTK_DIALOG(dialog));
    if(result == GTK_RESPONSE_APPLY)
        g_print("apply\n");
    else if(result == GTK_RESPONSE_CANCEL)
        g_print("cancel\n");
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

void show_events_list_dialog(GtkWindow *parent)
{
        /*remove this line if we want to reload the config
         *everytime we show the config dialog
         */
        if(g_event_config_list == NULL)
            load_event_config_data();
        if(g_event_config_list == NULL)
            g_print("can't load event's config\n");
        parent_dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(parent_dialog), _("Event Config"));
        gtk_window_set_default_size(GTK_WINDOW(parent_dialog), 450, 400);
        if(parent != NULL)
        {
            gtk_window_set_transient_for(GTK_WINDOW(parent_dialog), parent);
            // modal = parent window can't steal focus
            gtk_window_set_modal(GTK_WINDOW(parent_dialog), true);
            gtk_window_set_icon_name(GTK_WINDOW(parent_dialog),
                gtk_window_get_icon_name(parent));
        }

        GtkWidget *main_vbox = gtk_vbox_new(0, 0);
        GtkWidget *events_scroll = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(events_scroll),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        /* event list treeview */
        GtkWidget *events_tv = gtk_tree_view_new();
        /* column with event name and description */
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;

        /* add column to tree view */
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_("Event"),
                                                     renderer,
                                                     "markup",
                                                     COLUMN_EVENT_NAME,
                                                     NULL);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_sort_column_id(column, COLUMN_EVENT_NAME);
        gtk_tree_view_append_column(GTK_TREE_VIEW(events_tv), column);

        /* Create data store for the list and attach it
        * COLUMN_EVENT_NAME -> name+description
        * COLUMN_EVENT -> event_conf_t* so we can retrieve the event_config from the row
        */
        GtkListStore *events_list_store = gtk_list_store_new(NUM_COLUMNS,
                                                    G_TYPE_STRING, /* Event name + description */
                                                    G_TYPE_POINTER);
        gtk_tree_view_set_model(GTK_TREE_VIEW(events_tv), GTK_TREE_MODEL(events_list_store));

        g_hash_table_foreach(g_event_config_list,
                            &add_event_to_liststore,
                            events_list_store);

        /* Double click/Enter handler */
        g_signal_connect(events_tv, "row-activated", G_CALLBACK(on_event_row_activated_cb), NULL);

        gtk_container_add(GTK_CONTAINER(events_scroll), events_tv);

        GtkWidget *configure_event_btn = gtk_button_new_with_label(_("Configure"));
        g_signal_connect(configure_event_btn, "clicked", G_CALLBACK(on_configure_event_cb), events_tv);

        GtkWidget *close_btn = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
        g_signal_connect(close_btn, "clicked", G_CALLBACK(on_close_event_list_cb), parent_dialog);

        GtkWidget *btnbox = gtk_hbutton_box_new();
        gtk_box_pack_end(GTK_BOX(btnbox), configure_event_btn, false, false, 0);
        gtk_box_pack_end(GTK_BOX(btnbox), close_btn, false, false, 0);

        gtk_box_pack_start(GTK_BOX(main_vbox), events_scroll, true, true, 10);
        gtk_box_pack_start(GTK_BOX(main_vbox), btnbox, false, false, 0);
        gtk_container_add(GTK_CONTAINER(parent_dialog), main_vbox);

        gtk_widget_show_all(parent_dialog);
}
