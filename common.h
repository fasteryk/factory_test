/*
 * common.h
 *
 *  Created on: Jun 8, 2013
 *      Author: developer
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <gtk/gtk.h>

extern GtkWidget *main_window, *install_progress_bar, *install_info_view,
			*start_install_button, *start_test_button, *stop_test_button,
			*test_item_view, *result_label;;
extern int tty_fd;


void send_cmd(int tty, const char *cmd);
int check_line(int tty, char* content, int retry);
int run_shell_command(int tty, char *cmd, int timeout);
int open_tty(char *device, speed_t baudrate);
void recycle_target_power();


#endif /* COMMON_H_ */
