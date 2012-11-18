#ifndef CONF_H
#define CONF_H

struct remote_conf {
	int magic;

	uint32_t addr;
	uint16_t port;
};

void conf_edit(void);
void conf_set_defaults(void);

struct remote_conf conf_get_remote(void);

#endif
