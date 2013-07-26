/*
 * profile.h
 *
 *  Created on: Jun 9, 2013
 *      Author: developer
 */

#ifndef PROFILE_H_
#define PROFILE_H_


struct action_cmd {
	char *cmd;
	union {
		int check_result;
		char *pattern;
	};
	int timeout;
	int delay;
};

struct install_action {
	char *name;
	struct action_cmd *command;
	int cmd_count;
	struct install_action *next;
};

struct test_item {
	char *name;
	char *cmd;
	int key;
	struct test_item *next;
};


extern char *tty_device;
extern struct install_action *uboot_action_head, *shell_action_head;
extern struct test_item *test_item_list;


int read_profile(char *filename);


#endif /* PROFILE_H_ */
