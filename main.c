#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

#include "profile.h"


#define LINE_LEN			2048


int tty_fd;
GtkWidget *main_window, *install_progress_bar, *install_info_view,
		*start_install_button;


static int open_tty(char *device, speed_t baudrate)
{
    int fd;
    struct termios options;

    fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
    	perror("open tty");
        return -1;
    } else
        fcntl(fd, F_SETFL, 0);

    tcgetattr(fd, &options);

    cfsetispeed(&options, baudrate);
    cfsetospeed(&options, baudrate);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;         /* no parity */
    options.c_cflag &= ~CSTOPB;         /* one stop bit */
    options.c_cflag &= ~CSIZE;          /* mask the character size bits */
    options.c_cflag |= CS8;             /* select 8 data bits */
    options.c_cflag &= ~CRTSCTS;        /* disable hardware flow control */
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  /* choosing raw input */
    options.c_iflag &= ~(IXON | IXOFF | IXANY);          /* disable software flow control */
    options.c_oflag &= ~OPOST;          /* choosing raw output */
    options.c_cc[VTIME] = 1;
    options.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);
    tcsetattr(fd, TCSANOW, &options);

    return fd;
}

static void display_error_dialog(const gchar *msg)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(GTK_WINDOW(main_window),
									GTK_DIALOG_MODAL,
									GTK_MESSAGE_ERROR,
									GTK_BUTTONS_CLOSE,
									msg);

	gtk_window_set_title(GTK_WINDOW(dialog), "Error");
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

static void setup_run_env()
{
	FILE *fp;
	char msg[100];

	if (read_profile("profile.xml") < 0) {
		sprintf(msg, "Configuration file error.");
		goto _err;
	}

	tty_fd = open_tty(tty_device, B115200);
	if (tty_fd < 0) {
		sprintf(msg, "Can't open tty device: %s", tty_device);
		goto _err;
	}

	return;

_err:
	display_error_dialog(msg);
	exit(-1);
}

void destroy(GtkWidget *window, gpointer data)
{
	gtk_main_quit();
}

gboolean delete_event(GtkWidget *window, GdkEvent *event, gpointer data)
{
	GtkWidget *dialog;
	gint result;

	dialog = gtk_message_dialog_new(GTK_WINDOW(window),
										GTK_DIALOG_MODAL,
										GTK_MESSAGE_QUESTION,
										GTK_BUTTONS_OK_CANCEL,
										"Exit Factory Test Program?");

	gtk_window_set_title(GTK_WINDOW(dialog), "Confirm Exit");

	result = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	return result == GTK_RESPONSE_OK ? FALSE : TRUE;
}

int main(int argc, char *argv[])
{
	GtkBuilder *builder;
	char device[20];

	gtk_init(&argc, &argv);

	builder = gtk_builder_new();
	gtk_builder_add_from_file(builder, "gui.glade", NULL);
	main_window = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
	gtk_window_maximize(GTK_WINDOW(main_window));

	install_progress_bar =
				GTK_WIDGET(gtk_builder_get_object(builder, "install_progressbar"));
	install_info_view =
				GTK_WIDGET(gtk_builder_get_object(builder, "install_info_view"));
	start_install_button =
			GTK_WIDGET(gtk_builder_get_object(builder, "start_install_button"));

	gtk_builder_connect_signals(builder, NULL);

	gtk_widget_show_all(main_window);

	setup_run_env();

	/* Hand control over to the main loop. */
	gtk_main();

	return 0;
}
