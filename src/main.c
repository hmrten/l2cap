#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "res.h"
#include "capture.h"
#include "main.h"
#include "config.h"

#define WM_USER_CAPTURE_DONE (WM_USER+1)

static HINSTANCE _inst;

static HANDLE _capturethread = 0;
static DWORD _capturetid;

static HWND _dlgwnd;
static WNDPROC _origrichproc;

static int CALLBACK dlg_main_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam);
static LRESULT WINAPI riched_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam);

static void init_controls(HWND wnd);
static void add_text(const char *string, DWORD color);

static DWORD WINAPI capture_func(void *param);

int WINAPI WinMain(HINSTANCE inst, HINSTANCE previnst, LPSTR cmdline, int showcmd)
{
	int result;
	INITCOMMONCONTROLSEX cc;

	_inst = GetModuleHandle(NULL);

	read_config();

	LoadLibrary("RichEd20.dll");

	cc.dwSize = sizeof(INITCOMMONCONTROLSEX);
	cc.dwICC = ICC_STANDARD_CLASSES;

	InitCommonControlsEx(&cc);
	
	result = DialogBox(_inst, MAKEINTRESOURCE(IDC_DLG_MAIN), 0, dlg_main_proc);

	if (result == -1) {
		DWORD errcode = GetLastError();
		char buffer[50];
		sprintf(buffer, "Error: %u", errcode);
		MessageBox(0, buffer, "Error", MB_OK);
	}

	if (_capturethread) {
		TerminateThread(_capturethread, 0);
		CloseHandle(_capturethread);
	}

	write_config();

	return result;
}

static char *get_combo_text(HWND wnd, int id)
{
	HWND cb;
	int cursel;
	int len;
	char *text;

	cb = GetDlgItem(wnd, id);
	cursel = SendMessage(cb, CB_GETCURSEL, 0, 0);
	len = SendMessage(cb, CB_GETLBTEXTLEN, cursel, 0);
	text = malloc(len+1);
	SendMessage(cb, CB_GETLBTEXT, cursel, (LPARAM)text);
	return text;
}

static int get_combo_data(HWND wnd, int id)
{
	HWND cb;
	int cursel;
	int data;

	cb = GetDlgItem(wnd, id);
	cursel = SendMessage(cb, CB_GETCURSEL, 0, 0);
	data = SendMessage(cb, CB_GETITEMDATA, cursel, 0);
	return data;
}

static int CALLBACK dlg_main_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
		case WM_INITDIALOG:
			_dlgwnd = wnd;
			init_controls(wnd);
		break;

		case WM_COMMAND:
			switch (LOWORD(wparam)) {
				case IDC_BN_CAPTURE:
					if (HIWORD(wparam) == BN_CLICKED) {
						char *sv;
						int dev;

						sv = get_combo_text(wnd, IDC_CB_SERVER);
						dev = get_combo_data(wnd, IDC_CB_DEVICE);

						strcpy(config.server, sv);
						get_device_name(config.device, dev);

						free(sv);
						
						_capturethread = CreateThread(0, 0, capture_func, 0, 0, &_capturetid);
						
						EnableWindow(GetDlgItem(wnd, IDC_BN_CAPTURE), 0);
						EnableWindow(GetDlgItem(wnd, IDC_BN_STOP), 1);
						SetFocus(GetDlgItem(wnd, IDC_RICHED_LOG));
					}
				break;

				case IDC_BN_STOP:
					if (HIWORD(wparam) == BN_CLICKED) {
						stopcapturing = 1;
					}
				break;

				case IDC_MENU_EDIT_CUT:
					//MessageBox(0, "test", "test", MB_OK);
					//fatal_error("test");
				break;
			}
		break;

		case WM_USER_CAPTURE_DONE:
			EnableWindow(GetDlgItem(wnd, IDC_BN_CAPTURE), 1);
			EnableWindow(GetDlgItem(wnd, IDC_BN_STOP), 0);
		break;

		case WM_CLOSE:
			EndDialog(wnd, 0);
		break;

		default:
			return 0;
	}
	
	return 1;
}

static LRESULT WINAPI riched_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_RBUTTONUP) {
		HMENU edit_menu;
		HMENU edit_sub_menu;
		POINT pt;

		edit_menu = LoadMenu(_inst, MAKEINTRESOURCE(IDC_MENU_EDIT));
		edit_sub_menu = GetSubMenu(edit_menu, 0);

		pt.x = LOWORD(lparam);
		pt.y = HIWORD(lparam);
		ClientToScreen(wnd, &pt);

		TrackPopupMenu(edit_sub_menu, TPM_LEFTALIGN|TPM_TOPALIGN|TPM_RIGHTBUTTON, pt.x, pt.y, 0, _dlgwnd, 0);

		DestroyMenu(edit_menu);

		return 0;
	}

	return CallWindowProc(_origrichproc, wnd, msg, wparam, lparam);
}

static void init_controls(HWND wnd)
{
	HWND svhandle;
	HWND devhandle;
	HWND richhandle;
	const char *svitems[] = { "Chronos", "Naia" };
	int numsv = 2;
	int i;
	int index;
	int indexsel=0;

	svhandle = GetDlgItem(wnd, IDC_CB_SERVER);
	devhandle = GetDlgItem(wnd, IDC_CB_DEVICE);
	richhandle = GetDlgItem(wnd, IDC_RICHED_LOG);

	for (i=0; i<numsv; ++i) {
		index = SendMessage(svhandle, CB_ADDSTRING, 0, (LPARAM)svitems[i]);
		if (strcmp(config.server, svitems[i]) == 0) {
			indexsel = index;
		}
	}

	SendMessage(svhandle, CB_SETCURSEL, indexsel, 0);

	_origrichproc = (WNDPROC)SetWindowLongPtr(richhandle, GWL_WNDPROC, (LONG_PTR)riched_proc);

	SetClassLongPtr(wnd, GCL_HICON, (LONG_PTR)LoadIcon(0, IDI_APPLICATION));
	SetClassLongPtr(wnd, GCL_HICONSM, (LONG_PTR)LoadIcon(0, IDI_APPLICATION));

	indexsel = init_capture();

	if (indexsel == -1) {
		MessageBox(wnd, "No interfaces found. Make sure WinPcap is installed.", "Error", MB_OK);
	}

	SendMessage(devhandle, CB_SETCURSEL, indexsel, 0);
}

static void add_text(const char *text, DWORD color)
{
	HWND riched_handle;
	CHARRANGE cr;
	CHARFORMAT cf;

	riched_handle = GetDlgItem(_dlgwnd, IDC_RICHED_LOG);
	
	cr.cpMin = -1;
	cr.cpMax = -1;

	cf.cbSize = sizeof(CHARFORMAT);
	cf.dwMask = CFM_COLOR;
	cf.crTextColor = color;

	SendMessage(riched_handle, EM_EXSETSEL, 0, (LPARAM)&cr);
	SendMessage(riched_handle, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
	SendMessage(riched_handle, EM_REPLACESEL, 0, (LPARAM)text);

	SendMessage(riched_handle, EM_LINESCROLL, 0, (LPARAM)1);
}

static DWORD WINAPI capture_func(void *param)
{
	int result;

	result = do_capture();

	SendMessage(_dlgwnd, WM_USER_CAPTURE_DONE, 0, (LPARAM)result);

	return result;
}

void fatal_error(const char *msg)
{
	MessageBox(0, msg, "Fatal error", MB_OK|MB_ICONERROR);
	ExitProcess(-1);
}

static FILE *_dbgfile = NULL;
/*static const char *log_str[LOG_NUMTYPES] = {
	"*** ",
	"*** [ERROR] ",
	"*** [WARNING] ",
	"",
	"[S] ",
	"[C] "
};*/
void log_msg(const char *type, const char *fmt, ...)
{
	va_list args;
	char buf[4*1024];
	char *p;
	size_t len;
	int res;

	sprintf(buf, "[%s]: ", type);
	len = strlen(buf);
	p = (char *)buf+len;

	va_start(args, fmt); 
	res = vsprintf(p, fmt, args);
	va_end(args);
	p[res] = 0;

	if (strncmp("dbg", type, 3) == 0) {
		if (_dbgfile == NULL) {
			_dbgfile = fopen("dbglog.txt", "w");
		}

		fwrite(buf, res+len, 1, _dbgfile);
		fflush(_dbgfile);
	}
	add_text(buf, 0);
}

void add_device(const char *name, const char *desc, int dnum)
{
	HWND devhandle;
	int index;

	devhandle = GetDlgItem(_dlgwnd, IDC_CB_DEVICE);
	index = SendMessage(devhandle, CB_ADDSTRING, 0, (LPARAM)desc);
	SendMessage(devhandle, CB_SETITEMDATA, index, (LPARAM)dnum);	
}
