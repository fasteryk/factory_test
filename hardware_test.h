/*
 * hardware_test.h
 *
 *  Created on: Jul 9, 2013
 *      Author: yukuai
 */

#ifndef HARDWARE_TEST_H_
#define HARDWARE_TEST_H_


enum {
	ICON = 0,
	CONTENT,
	RESULT,
	COLUMNS
};

enum {
	COLOR_RED,
	COLOR_GREEN
};

enum {
	TEST_SUCCESS,
	TEST_FAILURE,
	TEST_MANUAL_QUIT
};

enum {
	UPDATE_HIGHLIGHT,
	UPDATE_RESULT
};

struct test_status {
	GMutex update_mutex;
	GCond update_cond;
	int update;
	int type;
	int number;
	int success;
};


void setup_test_item_view();
void set_model();
void set_result_label_font();


#endif /* HARDWARE_TEST_H_ */
