/*
 * common.h
 *
 *  Created on: Jun 8, 2013
 *      Author: developer
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <gtk/gtk.h>

extern GtkWidget *install_progress_bar, *install_info_view,
			*start_install_button;
extern int tty_fd;


void send_cmd(int tty, const char *cmd);
int check_line(int tty, char* content, int retry);
int run_shell_command(int tty, char *cmd, int timeout);

#endif /* COMMON_H_ */
