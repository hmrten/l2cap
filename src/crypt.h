#ifndef CRYPT_H
#define CRYPT_H

void reset_crypt(void);
int decrypt_data(uchar *data, ushort size, int fromsv, int *first);

#endif
