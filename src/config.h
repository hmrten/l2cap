#ifndef CONFIG_H
#define CONFIG_H

struct {
	char server[512];
	char device[512];
} config;

void read_config(void);
void write_config(void);

#endif
