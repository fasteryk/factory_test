/*
 * profile.c
 *
 *  Created on: Jun 9, 2013
 *      Author: developer
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "profile.h"


char *tty_device = NULL;
struct install_action *uboot_action_head = NULL,
						*shell_action_head = NULL;


static int get_element_count(xmlDoc *doc, char *path)
{
	xmlXPathContextPtr xpath_ctx;
	xmlXPathObjectPtr xpath_obj;
	xmlNodeSetPtr nodes;
	int ret;

	xpath_ctx = xmlXPathNewContext(doc);
	if(xpath_ctx == NULL)
		return -1;

	xpath_obj = xmlXPathEvalExpression(path, xpath_ctx);
	if(xpath_obj == NULL) {
		xmlXPathFreeContext(xpath_ctx);
		return -1;
	}

	nodes = xpath_obj->nodesetval;
	ret = nodes->nodeNr;

	xmlXPathFreeObject(xpath_obj);
	xmlXPathFreeContext(xpath_ctx);
	return ret;
}

static int get_element_attribute_value(xmlDoc *doc, const char *path, char *attr, char **value)
{
	xmlXPathContextPtr xpath_ctx;
	xmlXPathObjectPtr xpath_obj;
	xmlNodeSetPtr nodes;
	xmlNodePtr cur;
	char *v;
	int ret = 0;

	xpath_ctx = xmlXPathNewContext(doc);
	if(xpath_ctx == NULL)
		return -1;

	xpath_obj = xmlXPathEvalExpression(path, xpath_ctx);
	if(xpath_obj == NULL) {
		ret = -1;
		goto _exit;
	}

	nodes = xpath_obj->nodesetval;
	if (xpath_obj->nodesetval->nodeNr == 0) {
		*value = NULL;
		ret = -1;
		goto _exit2;
	}
	cur = nodes->nodeTab[0];

	v = xmlGetProp(cur, attr);
	if (v) {
		*value = malloc(strlen(v)+1);
		strcpy(*value, v);
	} else {
		*value = NULL;
		ret = -1;
	}

_exit2:
	xmlXPathFreeObject(xpath_obj);
_exit:
	xmlXPathFreeContext(xpath_ctx);
	return ret;
}

static int get_install_action(xmlDoc *doc, const char *path, struct install_action **action)
{
	int act_cnt, cmd_cnt, i;
	char buf[100], *str;
	struct install_action *ia;
	size_t size;

	sprintf(buf, "%s/ACTION", path);

	act_cnt = get_element_count(doc, buf);
	if (act_cnt) {
		for ( ;act_cnt; act_cnt--) {
			ia = malloc(sizeof(struct install_action));
			ia->next = *action == NULL ? NULL : *action;
			*action = ia;

			sprintf(buf, "%s/ACTION[%d]", path, act_cnt);
			get_element_attribute_value(doc, buf, "desc", &ia->name);

			sprintf(buf, "%s/ACTION[%d]/CMD", path, act_cnt);
			cmd_cnt = get_element_count(doc, buf);

			ia->cmd_count = cmd_cnt;
			size = sizeof(struct action_cmd)*cmd_cnt;
			ia->command = malloc(size);
			memset(ia->command, 0, size);

			for (i = 0; i < cmd_cnt; i++) {
				sprintf(buf, "%s/ACTION[%d]/CMD[%d]", path, act_cnt, i+1);
				get_element_attribute_value(doc, buf, "body",
											&ia->command[i].cmd);

				if (get_element_attribute_value(doc, buf, "delay", &str) < 0)
					ia->command[i].delay = 2;
				else
					ia->command[i].delay = atoi(str);
				free(str);

				if (get_element_attribute_value(doc, buf, "timeout", &str) < 0)
					ia->command[i].timeout = 0;
				else
					ia->command[i].timeout = atoi(str);
				free(str);

				if (get_element_attribute_value(doc, buf, "pattern",
												&ia->command[i].pattern) < 0) {
					if (get_element_attribute_value(doc, buf,
													"check_result", &str) == 0) {
						ia->command[i].check_result = strcmp(str, "1") == 0 ? 1 : 0;
						free(str);
					}
				}
			}
		}
	}

	return 0;
}

static int get_config_content(xmlDoc *doc)
{
	if (get_element_attribute_value(doc, "/FT/CFG/TTY_DEVICE",
									"name", &tty_device) < 0)
		return -1;

	get_install_action(doc, "/FT/INSTALL_STEPS/UBOOT", &uboot_action_head);
	get_install_action(doc, "/FT/INSTALL_STEPS/SHELL", &shell_action_head);

	return 0;
}

static void print_content()
{
	int i;
	struct install_action *act;

	act = uboot_action_head;
	while (act) {
	   	printf("uboot action: %s\n", act->name);

	   	for (i = 0; i < act->cmd_count; i++) {
	   		if (act->command[i].cmd)
	   			printf("	cmd%d: %s  ", i, act->command[i].cmd);

	    	if (act->command[i].pattern)
	    		printf("pattern: \"%s\"  ", act->command[i].pattern);

	    	printf("delay: %dms  ", act->command[i].delay*100);
	    	printf("timeout: %dms  \n", act->command[i].timeout*100);
	    }

	   	act = act->next;
	}

	act = shell_action_head;
	while (act) {
	   	printf("shell action: %s\n", act->name);

		for (i = 0; i < act->cmd_count; i++) {
			if (act->command[i].cmd)
				printf("	cmd%d: %s  ", i, act->command[i].cmd);

		  	printf("check_result: %d  ", act->command[i].check_result);
		  	printf("timeout: %dms  \n", act->command[i].timeout*100);
		}

		act = act->next;
	}
}

int read_profile(char *filename)
{
	xmlDoc *file = NULL;
	int ret = 0;

	xmlInitParser();

	file = xmlParseFile(filename);
    if (file == NULL) {
        printf("error: could not parse file %s\n", filename);
        ret = -1;
        goto _exit1;
    }

    if (get_config_content(file) < 0) {
    	printf("error: could not get configure content\n");
    	ret = -1;
    	goto _exit2;
    }

    printf("tty_device: %s\n", tty_device);

    print_content();

_exit2:
    xmlFreeDoc(file);
_exit1:
    xmlCleanupParser();
    return ret;
}
