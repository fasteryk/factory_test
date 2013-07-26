#include <gtk/gtk.h>

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

#define CTRLC			0x03
#define IMAGE_FILE		"/tftpboot/system_update.img"
#define IH_MAGIC		0x27051956	/* Image Magic Number		*/
#define IH_NMLEN		32	/* Image Name Length		*/


struct image_header {
	__be32		ih_magic;	/* Image Header Magic Number	*/
	__be32		ih_hcrc;	/* Image Header CRC Checksum	*/
	__be32		ih_time;	/* Image Creation Timestamp	*/
	__be32		ih_size;	/* Image Data Size		*/
	__be32		ih_load;	/* Data	 Load  Address		*/
	__be32		ih_ep;		/* Entry Point Address		*/
	__be32		ih_dcrc;	/* Image Data CRC Checksum	*/
	uint8_t		ih_os;		/* Operating System		*/
	uint8_t		ih_arch;	/* CPU architecture		*/
	uint8_t		ih_type;	/* Image Type			*/
	uint8_t		ih_comp;	/* Compression Type		*/
	uint8_t		ih_name[IH_NMLEN];	/* Image Name		*/
};

struct install_info {
	GMutex data_mutex;
	GCond data_cond;
	int dirty;
	char msg[200];
};


static struct install_info ins_info = {
		.dirty = 0,
};

static int oper_cnt = 0, oper_counter = 0;


static gboolean update_install_info(gpointer data)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	struct install_info *info = (struct install_info *)data;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(install_info_view));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
	gtk_text_buffer_insert(buffer, &iter, info->msg, -1);

	g_mutex_lock(&info->data_mutex);
	info->dirty = 0;
	g_cond_signal(&info->data_cond);
	g_mutex_unlock(&info->data_mutex);

	return FALSE;
}

static void show_info(char *msg)
{
	g_mutex_lock(&ins_info.data_mutex);

	if (ins_info.dirty) {
		while (ins_info.dirty)
			g_cond_wait(&ins_info.data_cond, &ins_info.data_mutex);
	}

	ins_info.dirty = 1;

	strcpy(ins_info.msg, msg);

	g_idle_add(update_install_info, &ins_info);

	g_mutex_unlock(&ins_info.data_mutex);
}

static gboolean finish_up_install(gpointer data)
{
	gtk_widget_set_sensitive(start_install_button, TRUE);
	return FALSE;
}

static gboolean update_progress_bar(gpointer data)
{
	int counter = *(int *)data;
	gdouble f = (gdouble)counter/(gdouble)oper_cnt;
	char buf[50];

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(install_progress_bar), f);
	sprintf(buf, "%d%%", (int)(f*100));
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(install_progress_bar), buf);

	return FALSE;
}

static int check_update_image()
{
	u_char buf[64];
	FILE *fp;
	struct image_header *ih;

	fp = fopen(IMAGE_FILE, "rb");
	if (fp == NULL) {
		show_info("Can't open update image\n");
		return -1;
	}

	if (fread(buf, 64, 1, fp) != 1) {
		show_info("Invalid update image\n");
		return -1;
	}

	ih = (struct image_header *)buf;

	if (__cpu_to_be32(ih->ih_magic) != IH_MAGIC) {
		show_info("Invalid update image magic number\n");
		return -1;
	}

	show_info("System software update image version: ");
	show_info(ih->ih_name);
	show_info("\n\n");
	return 0;
}

static int exec_uboot_phase()
{
	char buf[200];
	struct install_action *ia = uboot_action_head;
	int i;

	while (ia) {
		if (ia->name) {
			sprintf(buf, "%s  ...  ", ia->name);
			show_info(buf);
		}

		for (i = 0; i < ia->cmd_count; i++) {
			if (ia->command[i].cmd == NULL)
				continue;

			sprintf(buf, "%s\n", ia->command[i].cmd);
			printf("exec uboot cmd: %s", buf);

			tcflush(tty_fd, TCIOFLUSH);
			usleep( ia->command[i].delay*100000);

			send_cmd(tty_fd, buf);

			if (ia->command[i].pattern) {
				printf("check pattern: \"%s\"\n", ia->command[i].pattern);

				if (check_line(tty_fd, ia->command[i].pattern,
								ia->command[i].timeout) < 0) {
					show_info("failed\n");
					return -1;
				}
			}

			oper_counter++;
			g_idle_add(update_progress_bar, &oper_counter);
		}

		show_info("done\n");
		ia = ia-> next;
	}

	return 0;
}

static int exec_shell_phase()
{
	char buf[200];
	struct install_action *ia = shell_action_head;
	int i, ret;

	while (ia) {
		if (ia->name) {
			sprintf(buf, "%s  ...  ", ia->name);
			show_info(buf);
		}

		for (i = 0; i < ia->cmd_count; i++) {
			if (ia->command[i].cmd == NULL)
				continue;

			sprintf(buf, "%s\n", ia->command[i].cmd);
			printf("exec shell cmd: %s", buf);

			tcflush(tty_fd, TCIOFLUSH);

			ret = run_shell_command(tty_fd, buf, ia->command[i].timeout);
			if (ia->command[i].check_result && ret != 0) {
				show_info("failed\n");
				return -1;
			}

			usleep(100000);
			oper_counter++;
			g_idle_add(update_progress_bar, &oper_counter);
		}

		show_info("done\n");
		ia = ia->next;
	}

	return 0;
}

static int connect_to_target()
{
	const char ctrlc[2] = {CTRLC, '\0'};
	int cnt = 300, n;
	char c;

	close(tty_fd);
	usleep(500000);
	tty_fd = open_tty(tty_device, B115200);

	recycle_target_power();

	while (cnt--) {
		tcflush(tty_fd, TCIOFLUSH);
		send_cmd(tty_fd, ctrlc);
		usleep(100000);

		while (read(tty_fd, &c, 1) == 1) {
			if (c == '#')
				return 0;
		}
	}

	return -1;
}

static gpointer install_thread(gpointer data)
{
	int i, ret, failure = 0;

	if (check_update_image() < 0) {
		g_idle_add(finish_up_install, NULL);
		return NULL;
	}

	show_info("Connect to target board  ...  ");
	if (connect_to_target() < 0) {
		g_idle_add(finish_up_install, NULL);
		show_info("failure\n");
		return NULL;
	}
	show_info("done\n");

	oper_counter++;
	g_idle_add(update_progress_bar, &oper_counter);

	if (exec_uboot_phase() < 0) {
		failure = 1;
		goto _exit;
	}

	sleep(1);

	if (exec_shell_phase() < 0) {
		failure = 1;
		goto _exit;
	}

	//show_info("Recycle target board power ...  ");
	//recycle_target_power();
	//show_info("done\n");

_exit:
	if (failure)
		show_info("\nSystem software installation failed!");
	else
		show_info("\nSystem software successfully installed!");

	g_idle_add(finish_up_install, NULL);
	return NULL;
}

static int stat_operation_count()
{
	struct install_action *act;
	int cnt = 0;

	act = uboot_action_head;
	while (act) {
		cnt += act->cmd_count;
		act = act->next;
	}

	act = shell_action_head;
	while (act) {
		cnt += act->cmd_count;
		act = act->next;
	}

	return cnt;
}

static void clear_install_status()
{
	GtkTextBuffer *text_buffer;

	text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(install_info_view));
	gtk_text_buffer_set_text(text_buffer, "", -1);

	oper_cnt = stat_operation_count()+1;
	oper_counter = 0;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(install_progress_bar), 0);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(install_progress_bar), "0%");
}

void start_install_button_clicked(GtkWidget *widget, gpointer data)
{
	gtk_widget_set_sensitive(widget, FALSE);
	clear_install_status();

	g_thread_new("install thread", install_thread, NULL);
}
