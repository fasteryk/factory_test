/*
 * hardware_test.c
 *
 *  Created on: Jul 9, 2013
 *      Author: yukuai
 */

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <asm/byteorder.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "common.h"
#include "profile.h"
#include "hardware_test.h"


static int thread_exit = 0, end_state = TEST_MANUAL_QUIT;


void setup_test_item_view()
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, "Content");

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, renderer, "stock-id",
										ICON, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer, "text",
										CONTENT, NULL);

	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(column, 400);

	gtk_tree_view_append_column(GTK_TREE_VIEW(test_item_view), column);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, "Result");

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer, "stock-id",
										RESULT, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(test_item_view), column);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, "");
	gtk_tree_view_append_column(GTK_TREE_VIEW(test_item_view), column);
}

void set_model()
{
	GtkTreeIter iter;
	GtkListStore *store;
	struct test_item *ti;

	store = gtk_list_store_new(COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
						ICON, GTK_STOCK_EXECUTE,
						CONTENT, "0. Recycle target board power",
						RESULT, NULL,
						-1);

	ti= test_item_list;
	while (ti) {
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
							ICON, GTK_STOCK_EXECUTE,
							CONTENT, ti->name,
							RESULT, NULL,
							-1);
		ti = ti->next;;
	}

	gtk_tree_view_set_model(GTK_TREE_VIEW(test_item_view), GTK_TREE_MODEL(store));
	g_object_unref(store);
}

void set_result_label_font()
{
	PangoFontDescription *font_desc = pango_font_description_from_string("Ubuntu");

	pango_font_description_set_size(font_desc, 100*PANGO_SCALE);
	gtk_widget_modify_font(result_label, font_desc);
	gtk_label_set_text(GTK_LABEL(result_label), "");
}

static void clear_status()
{
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(test_item_view));

	path = gtk_tree_path_new_from_string("0");
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);

	do {
		gtk_list_store_set(GTK_LIST_STORE(model), &iter, RESULT, NULL, -1);
	} while(gtk_tree_model_iter_next(model, &iter));
}

static void set_color(GtkWidget *widget, gint color)
{
	GdkColor c;

	if (color == COLOR_RED) {
		c.red = 0xffff;
		c.green = 0;
		c.blue = 0;
	} else {
		c.red = 0;
		c.green = 0xffff;
		c.blue = 0;
	}

	gtk_widget_modify_fg(widget, GTK_STATE_NORMAL, &c);
}

static gboolean update_status(gpointer data)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeModel *model;
	struct test_status *ts = (struct test_status *)data;
	gchar *stock_id;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(test_item_view));

	path =  gtk_tree_path_new_from_indices(ts->number, -1);
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);

	if (ts->type == UPDATE_HIGHLIGHT) {
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(test_item_view));
		gtk_tree_selection_select_iter(selection, &iter);
	} else {
		stock_id = ts->success ? GTK_STOCK_OK : GTK_STOCK_STOP;
		gtk_list_store_set(GTK_LIST_STORE(model), &iter, RESULT,
							stock_id, -1);
	}

	g_mutex_lock(&ts->update_mutex);
	ts->update = 0;
	g_cond_signal(&ts->update_cond);
	g_mutex_unlock(&ts->update_mutex);

	return FALSE;
}

static gboolean finish_test(gpointer data)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(test_item_view));
	gtk_tree_selection_unselect_all(selection);

	gtk_widget_set_sensitive(start_test_button, TRUE);
	gtk_widget_set_sensitive(stop_test_button, FALSE);

	switch(*(gint *)data) {
	case TEST_SUCCESS:
		set_color(result_label, COLOR_GREEN);
		gtk_label_set_text(GTK_LABEL(result_label), "PASS");
		break;

	case TEST_FAILURE:
		set_color(result_label, COLOR_RED);
		gtk_label_set_text(GTK_LABEL(result_label), "FAILURE");
		break;

	case TEST_MANUAL_QUIT:
		gtk_label_set_text(GTK_LABEL(result_label), "");
		break;

	default: ;
	}

	return FALSE;
}

static void update_test_result(struct test_status *r)
{
	r->update = 1;
	g_idle_add(update_status, r);

	g_mutex_lock(&r->update_mutex);
	while (r->update)
		g_cond_wait(&r->update_cond, &r->update_mutex);
	g_mutex_unlock(&r->update_mutex);
}

static gboolean run_msg_dialog(gpointer data)
{
	GtkWidget **dlg;

	dlg = (GtkWidget **)data;

	*dlg = gtk_message_dialog_new(GTK_WINDOW(main_window),
			GTK_DIALOG_MODAL,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_NONE,
			"Updating application program, please wait ... ");

	gtk_dialog_run(GTK_DIALOG(*dlg));

	return FALSE;
}

static gboolean destroy_msg_dialog(gpointer data)
{
	GtkWidget **dlg;

	dlg = (GtkWidget **)data;
	gtk_widget_destroy(*dlg);
	return FALSE;
}

static gpointer test_process(gpointer data)
{
	struct test_status status;
	struct test_item *ti;
	int ret;
	static GtkWidget *msg_dlg = NULL;
	char wd[100], path[100];

	status.update = 0;
	status.number = 0;
	g_mutex_init(&status.update_mutex);
	g_cond_init(&status.update_cond);

	end_state = TEST_SUCCESS;

	ti = test_item_list;

	while(!thread_exit) {
		status.type = UPDATE_HIGHLIGHT;
		update_test_result(&status);

		if (status.number == 0) {
			recycle_target_power();
			ret = 0;
		} else {
			printf("exec test cmd: %s\n", ti->cmd);
			ret = WEXITSTATUS(system(ti->cmd));
			printf("	result %d\n", ret);
		}

		status.type = UPDATE_RESULT;
		status.success = ret == 0 ? 1 : 0;
		update_test_result(&status);

		if (ret != 0) {
			end_state = TEST_FAILURE;
			if (ti->key)
				break;
		}

		if (status.number != 0)	{
			ti = ti->next;
			if (ti == NULL)
				break;
		}

		status.number++;
	}

	if (end_state == TEST_SUCCESS) {
		getcwd(wd, 100);
		sprintf(path, "%s/application", wd);
		chdir(path);

		g_idle_add(run_msg_dialog, &msg_dlg);
		system("./update_app");
		g_idle_add(destroy_msg_dialog, &msg_dlg);

		chdir(wd);
	}

	g_idle_add(finish_test, &end_state);

	return NULL;
}

void start_test_button_clicked(GtkWidget *widget, gpointer data)
{
	gtk_widget_set_sensitive(widget, FALSE);
	gtk_widget_set_sensitive(stop_test_button, TRUE);

	clear_status();

	gtk_label_set_text(GTK_LABEL(result_label), "");

	thread_exit = 0;
	g_thread_new("test process", test_process, widget);
}

void stop_test_button_clicked(GtkWidget *widget, gpointer data)
{
	//thread_exit = 1;
	//clear_status();
}

