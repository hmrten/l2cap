#ifndef MAIN_H
#define MAIN_H

/*enum {
	LOG_INFO,
	LOG_ERROR,
	LOG_WARNING,
	LOG_GAME,
	LOG_GAME_SV,
	LOG_GAME_CL,

	LOG_NUMTYPES
};*/

void fatal_error(const char *msg);
//void dbg_log(int type, const char *fmt, ...);

void log_msg(const char *type, const char *fmt, ...);
void add_device(const char *name, const char *desc, int dnum);

#endif
