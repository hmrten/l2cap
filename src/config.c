#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>

#include <string.h>

#include "config.h"

void read_config(void)
{
	char filename[MAX_PATH];
	char buf[512];

	_getcwd(filename, MAX_PATH);
	strcat(filename, "\\l2cap.ini");

	if (GetPrivateProfileString("Capture", "Server", NULL, buf, sizeof(buf), filename)) {
		strcpy(config.server, buf);
	}

	if (GetPrivateProfileString("Capture", "Device", NULL, buf, sizeof(buf), filename)) {
		strcpy(config.device, buf);
	}
}

void write_config(void)
{
	char filename[MAX_PATH];

	_getcwd(filename, MAX_PATH);
	strcat(filename, "\\l2cap.ini");

	WritePrivateProfileString("Capture", "Server", config.server, filename);
	WritePrivateProfileString("Capture", "Device", config.device, filename);
}
