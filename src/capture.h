#ifndef CAPTURE_H
#define CAPTURE_H

extern int stopcapturing;

int get_device_name(char *buf, int index);
int init_capture(void);
int do_capture(void);

#endif
