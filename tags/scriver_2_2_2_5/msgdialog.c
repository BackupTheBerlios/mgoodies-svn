/*
Scriver

Copyright 2000-2003 Miranda ICQ/IM project, 
Copyright 2005 Piotr Piastucki

all portions of this codebase are copyrighted to the people 
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#include "commonheaders.h"
// IEVIew MOD Begin
#include "m_ieview.h"
// IEVIew MOD End
#define MS_SMILEYADD_SHOWSELECTION  "SmileyAdd/ShowSmileySelection"
#pragma hdrstop

#define TIMERID_MSGSEND      0
#define TIMERID_FLASHWND     1
#define TIMERID_TYPE         2
#define TIMEOUT_FLASHWND     900
#define TIMEOUT_ANTIBOMB     4000       //multiple-send bombproofing: send max 3 messages every 4 seconds
#define ANTIBOMB_COUNT       3
#define TIMEOUT_TYPEOFF      10000      //send type off after 10 seconds of inactivity
#define SB_CHAR_WIDTH        45
#define SB_TYPING_WIDTH      35
#define VALID_AVATAR(x)      (x==PA_FORMAT_PNG||x==PA_FORMAT_JPEG||x==PA_FORMAT_ICON||x==PA_FORMAT_BMP||x==PA_FORMAT_GIF)

#if defined(_UNICODE)
	#define SEND_FLAGS PREF_UNICODE
#else
	#define SEND_FLAGS 0
#endif

extern HCURSOR hCurSplitNS, hCurSplitWE, hCurHyperlinkHand, hDragCursor;
extern HANDLE hHookWinEvt;
extern struct CREOleCallback reOleCallback;
extern HINSTANCE g_hInst;

static void UpdateReadChars(HWND hwndDlg, struct MessageWindowData * dat);

static WNDPROC OldMessageEditProc, OldSplitterProc;
static const UINT buttonLineControls[] = { IDC_USERMENU, IDC_DETAILS, IDC_SMILEYS, IDC_ADD, IDC_HISTORY, IDCANCEL, IDOK, IDC_QUOTE};
static const UINT sendControls[] = { IDC_MESSAGE };

static DWORD CALLBACK StreamOutCallback(DWORD dwCookie, LPBYTE pbBuff, LONG cb, LONG * pcb)
{
	struct MessageSendInfo * msi = (struct MessageSendInfo *) dwCookie;
	msi->sendBuffer = (char *)realloc(msi->sendBuffer, msi->sendBufferSize + cb + 2);
	memcpy (msi->sendBuffer + msi->sendBufferSize, pbBuff, cb);
	msi->sendBufferSize += cb;
	*((TCHAR *)(msi->sendBuffer+msi->sendBufferSize)) = '\0';
	*pcb = cb;
    return 0;
}

static TCHAR *GetRichEditSelection(HWND hwndDlg) {
	CHARRANGE sel;
	SendDlgItemMessage(hwndDlg, IDC_LOG, EM_EXGETSEL, 0, (LPARAM)&sel);
	if (sel.cpMin!=sel.cpMax) {
		struct MessageSendInfo msi;
		EDITSTREAM stream;
		DWORD dwFlags = 0;
		ZeroMemory(&stream, sizeof(stream));
		stream.pfnCallback = StreamOutCallback;
		stream.dwCookie = (DWORD) &msi;
#if defined( _UNICODE )
		dwFlags = SF_TEXT|SF_UNICODE|SFF_SELECTION;
#else
        dwFlags = SF_TEXT|SFF_SELECTION;
#endif
		msi.sendBuffer = NULL;
		msi.sendBufferSize = 0;
		SendDlgItemMessage(hwndDlg, IDC_LOG, EM_STREAMOUT, (WPARAM)dwFlags, (LPARAM) & stream);
		return (TCHAR *)msi.sendBuffer;
	}
	return NULL;
}

static TCHAR *GetIEViewSelection(struct MessageWindowData *dat) {
	TCHAR *buffer;
	IEVIEWEVENT event;
	event.cbSize = sizeof(IEVIEWEVENT);
#ifdef _UNICODE
	event.dwFlags = 0;
#else
	event.dwFlags = IEEF_NO_UNICODE;
#endif
	event.codepage = dat->codePage;
	event.hwnd = dat->hwndLog;
	event.hContact = dat->hContact;
	event.iType = IEE_GET_SELECTION;
	buffer = (TCHAR *)CallService(MS_IEVIEW_EVENT, 0, (LPARAM)&event);
	return buffer;
}

static TCHAR *GetQuotedTextW(TCHAR * text) {
	int i, j, l, newLine, wasCR;
	TCHAR *out;
#ifdef _UNICODE
	l = wcslen(text);
#else
	l = strlen(text);
#endif
	newLine = 1;
	wasCR = 0;
	for (i=j=0; i<l; i++) {
		if (text[i]=='\r') {
			wasCR = 1;
			newLine = 1;
			j += text[i+1]!='\n' ? 2 : 1;
		} else if (text[i]=='\n') {
			newLine = 1;
			j += wasCR ? 1 : 2;
			wasCR = 0;
		} else {
			j++;
			if (newLine) {
				//for (;i<l && text[i]=='>';i++) j--;
				j+=2;
			}
			newLine = 0;
			wasCR = 0;
		}
	}
	j+=3;
	out = (TCHAR *)malloc(sizeof(TCHAR) * j);
	newLine = 1;
	wasCR = 0;
	for (i=j=0; i<l; i++) {
		if (text[i]=='\r') {
			wasCR = 1;
			newLine = 1;
			out[j++] = '\r';
			if (text[i+1]!='\n') {
				out[j++]='\n';
			}
		} else if (text[i]=='\n') {
			newLine = 1;
			if (!wasCR) {
				out[j++]='\r';
			}
			out[j++]='\n';
			wasCR = 0;
		} else {
			if (newLine) {
				out[j++]='>';
				out[j++]=' ';
				//for (;i<l && text[i]=='>';i++) j--;
			}
			newLine = 0;
			wasCR = 0;
			out[j++]=text[i];
		}
	}
	out[j++]='\r';
	out[j++]='\n';
	out[j++]='\0';
	return out;
}



static void RemoveSendBuffer(struct MessageWindowData *dat, int i) {
	if (dat->sendInfo[i].sendBuffer) {
 		free(dat->sendInfo[i].sendBuffer);
		dat->sendInfo[i].sendBuffer = NULL;
		dat->sendInfo[i].hSendId = NULL;
		for (i = 0; i < dat->sendCount; i++)
			if (dat->sendInfo[i].sendBuffer)
				break;
		if (i == dat->sendCount) {
			//all messages sent
			dat->sendCount = 0;
			free(dat->sendInfo);
			dat->sendInfo = NULL;
			KillTimer(dat->hwnd, TIMERID_MSGSEND);
		}
	}
}

static void NotifyLocalWinEvent(HANDLE hContact, HWND hwnd, unsigned int type) {
	MessageWindowEventData mwe = { 0 };

	if (hContact==NULL || hwnd==NULL) return;
	mwe.cbSize = sizeof(mwe);
	mwe.hContact = hContact;
	mwe.hwndWindow = hwnd;
	mwe.szModule = SRMMMOD;
	mwe.uType = type;
	mwe.uFlags = MSG_WINDOW_UFLAG_MSG_BOTH;
	NotifyEventHooks(hHookWinEvt, 0, (LPARAM)&mwe);
}

static char *MsgServiceName(HANDLE hContact)
{
#ifdef _UNICODE
    char szServiceName[100];
    char *szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);
    if (szProto == NULL)
        return PSS_MESSAGE;

    mir_snprintf(szServiceName, sizeof(szServiceName), "%s%sW", szProto, PSS_MESSAGE);
    if (ServiceExists(szServiceName))
        return PSS_MESSAGE "W";
#endif
    return PSS_MESSAGE;
}

static void AddToFileList(char ***pppFiles,int *totalCount,const char *szFilename) {
	*pppFiles=(char**)realloc(*pppFiles,(++*totalCount+1)*sizeof(char*));
	(*pppFiles)[*totalCount]=NULL;
	(*pppFiles)[*totalCount-1]=_strdup(szFilename);
	if(GetFileAttributesA(szFilename)&FILE_ATTRIBUTE_DIRECTORY) {
		WIN32_FIND_DATAA fd;
		HANDLE hFind;
		char szPath[MAX_PATH];
		lstrcpyA(szPath,szFilename);
		lstrcatA(szPath,"\\*");
		if(hFind=FindFirstFileA(szPath,&fd)) {
			do {
				if(!lstrcmpA(fd.cFileName,".") || !lstrcmpA(fd.cFileName,"..")) continue;
				lstrcpyA(szPath,szFilename);
				lstrcatA(szPath,"\\");
				lstrcatA(szPath,fd.cFileName);
				AddToFileList(pppFiles,totalCount,szPath);
			} while(FindNextFileA(hFind,&fd));
			FindClose(hFind);
		}
	}
}

static void ShowMultipleControls(HWND hwndDlg, const UINT * controls, int cControls, int state)
{
	int i;
	for (i = 0; i < cControls; i++)
		ShowWindow(GetDlgItem(hwndDlg, controls[i]), state);
}

static void SetDialogToType(HWND hwndDlg)
{
	struct MessageWindowData *dat;
	struct ParentWindowData *pdat;
	WINDOWPLACEMENT pl = { 0 };

	dat = (struct MessageWindowData *) GetWindowLong(hwndDlg, GWL_USERDATA);
	pdat = (struct ParentWindowData *) GetWindowLong(GetParent(hwndDlg), GWL_USERDATA);
	if (dat->hContact) {
		ShowMultipleControls(hwndDlg, buttonLineControls, sizeof(buttonLineControls) / sizeof(buttonLineControls[0]), (pdat->flags&SMF_SHOWBTNS) ? SW_SHOW : SW_HIDE);
		if (!DBGetContactSettingByte(dat->hContact, "CList", "NotOnList", 0))
			ShowWindow(GetDlgItem(hwndDlg, IDC_ADD), SW_HIDE);
	}
	else {
		ShowMultipleControls(hwndDlg, buttonLineControls, sizeof(buttonLineControls) / sizeof(buttonLineControls[0]), SW_HIDE);
	}
// IEVIew MOD Begin
	if (dat->hwndLog != NULL) {
		ShowWindow (GetDlgItem(hwndDlg, IDC_LOG), SW_HIDE);
	}
// IEVIew MOD End
	ShowMultipleControls(hwndDlg, sendControls, sizeof(sendControls) / sizeof(sendControls[0]), SW_SHOW);
	UpdateReadChars(hwndDlg, dat);
	ShowWindow(GetDlgItem(hwndDlg, IDC_SPLITTER), SW_SHOW);
	EnableWindow(GetDlgItem(hwndDlg, IDOK), GetWindowTextLength(GetDlgItem(hwndDlg, IDC_MESSAGE))?TRUE:FALSE);
	SendMessage(hwndDlg, DM_UPDATETITLE, 0, 0);
	SendMessage(hwndDlg, WM_SIZE, 0, 0);
	pl.length = sizeof(pl);
	GetWindowPlacement(hwndDlg, &pl);
	if (!IsWindowVisible(hwndDlg))
		pl.showCmd = SW_HIDE;
	SetWindowPlacement(hwndDlg, &pl);   //in case size is smaller than new minimum
}

struct SavedMessageData
{
	UINT message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD keyStates;            //use MOD_ defines from RegisterHotKey()
};

struct MsgEditSubclassData
{
	DWORD lastEnterTime;
	struct SavedMessageData *keyboardMsgQueue;
	int	msgQueueCount;
};

static void SaveKeyboardMessage(struct MsgEditSubclassData *dat, UINT message, WPARAM wParam, LPARAM lParam)
{
	dat->keyboardMsgQueue = (struct SavedMessageData *) realloc(dat->keyboardMsgQueue, sizeof(struct SavedMessageData) * (dat->msgQueueCount + 1));
	dat->keyboardMsgQueue[dat->msgQueueCount].message = message;
	dat->keyboardMsgQueue[dat->msgQueueCount].wParam = wParam;
	dat->keyboardMsgQueue[dat->msgQueueCount].lParam = lParam;
	dat->keyboardMsgQueue[dat->msgQueueCount].keyStates = (GetKeyState(VK_SHIFT) & 0x8000 ? MOD_SHIFT : 0) | (GetKeyState(VK_CONTROL) & 0x8000 ? MOD_CONTROL : 0) | (GetKeyState(VK_MENU) & 0x8000 ? MOD_ALT : 0);
	dat->msgQueueCount++;
}

#define EM_REPLAYSAVEDKEYSTROKES  (WM_USER+0x100)
#define EM_SUBCLASSED             (WM_USER+0x101)
#define EM_UNSUBCLASSED           (WM_USER+0x102)
#define ENTERCLICKTIME   1000   //max time in ms during which a double-tap on enter will cause a send
#define EDITMSGQUEUE_PASSTHRUCLIPBOARD  //if set the typing queue won't capture ctrl-C etc because people might want to use them on the read only text
                                                  //todo: decide if this should be set or not
static LRESULT CALLBACK MessageEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	struct MsgEditSubclassData *dat;
	struct MessageWindowData *pdat;
	SETTEXTEX  st;
	st.flags = ST_DEFAULT;
	#ifdef _UNICODE
		st.codepage = 1200;
	#else
		st.codepage = CP_ACP;
	#endif

	pdat=(struct MessageWindowData *)GetWindowLong(GetParent(hwnd),GWL_USERDATA);
	dat = (struct MsgEditSubclassData *) GetWindowLong(hwnd, GWL_USERDATA);
	switch (msg) {
	case EM_SUBCLASSED:
		dat = (struct MsgEditSubclassData *) malloc(sizeof(struct MsgEditSubclassData));
		SetWindowLong(hwnd, GWL_USERDATA, (LONG) dat);
		dat->lastEnterTime = 0;
		dat->keyboardMsgQueue = NULL;
		dat->msgQueueCount = 0;
		return 0;
	case EM_SETREADONLY:
		if (wParam) {
			if (dat->keyboardMsgQueue)
				free(dat->keyboardMsgQueue);
			dat->keyboardMsgQueue = NULL;
			dat->msgQueueCount = 0;
		}
		return 0;
		break;
		//for saved msg queue the keyup/keydowns generate wm_chars themselves
	case EM_REPLAYSAVEDKEYSTROKES:
		{
			int i;
			BYTE keyStateArray[256], originalKeyStateArray[256];
			MSG msg;

			while (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			GetKeyboardState(originalKeyStateArray);
			GetKeyboardState(keyStateArray);
			for (i = 0; i < dat->msgQueueCount; i++) {
				keyStateArray[VK_SHIFT] = dat->keyboardMsgQueue[i].keyStates & MOD_SHIFT ? 0x80 : 0;
				keyStateArray[VK_CONTROL] = dat->keyboardMsgQueue[i].keyStates & MOD_CONTROL ? 0x80 : 0;
				keyStateArray[VK_MENU] = dat->keyboardMsgQueue[i].keyStates & MOD_ALT ? 0x80 : 0;
				SetKeyboardState(keyStateArray);
				PostMessage(hwnd, dat->keyboardMsgQueue[i].message, dat->keyboardMsgQueue[i].wParam, dat->keyboardMsgQueue[i].lParam);
				while (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
			if (dat->keyboardMsgQueue)
				free(dat->keyboardMsgQueue);
			dat->keyboardMsgQueue = NULL;
			dat->msgQueueCount = 0;
			SetKeyboardState(originalKeyStateArray);
			return 0;
		}
	case WM_CHAR:
		if (GetWindowLong(hwnd, GWL_STYLE) & ES_READONLY) {
			break;
		}
		if (wParam == 1 && GetKeyState(VK_CONTROL) & 0x8000) {      //ctrl-a
			SendMessage(hwnd, EM_SETSEL, 0, -1);
			return 0;
		}
		if (wParam == 12 && GetKeyState(VK_CONTROL) & 0x8000) {     // ctrl-l
			SendMessage(GetParent(hwnd), DM_CLEARLOG, 0, 0);
			return 0;
		}
		if (wParam == 20 && GetKeyState(VK_CONTROL) & 0x8000 && GetKeyState(VK_SHIFT) & 0x8000) {     // ctrl-shift-t
			SendMessage(GetParent(GetParent(hwnd)), DM_SWITCHTOOLBAR, 0, 0);
			return 0;
		}
		if (wParam == 19 && GetKeyState(VK_CONTROL) & 0x8000 && GetKeyState(VK_SHIFT) & 0x8000) {     // ctrl-shift-s
			SendMessage(GetParent(GetParent(hwnd)), DM_SWITCHSTATUSBAR, 0, 0);
			return 0;
		}
		if (wParam == 18 && GetKeyState(VK_CONTROL) & 0x8000 && GetKeyState(VK_SHIFT) & 0x8000) {     // ctrl-shift-r
			SendMessage(GetParent(hwnd), DM_SWITCHRTL, 0, 0);
			return 0;
		}
		if (wParam == 13 && GetKeyState(VK_CONTROL) & 0x8000 && GetKeyState(VK_SHIFT) & 0x8000) {     // ctrl-shift-m
			SendMessage(GetParent(GetParent(hwnd)), DM_SWITCHTITLEBAR, 0, 0);
			return 0;
		}
		if (wParam == 23 && GetKeyState(VK_CONTROL) & 0x8000) {     // ctrl-w
			SendMessage(GetParent(hwnd), WM_CLOSE, 0, 0);
			return 0;
		}
		break;
	case WM_KEYUP:
		if (GetWindowLong(hwnd, GWL_STYLE) & ES_READONLY) {
			int i;
			//mustn't allow keyups for which there is no keydown
			for (i = 0; i < dat->msgQueueCount; i++)
				if (dat->keyboardMsgQueue[i].message == WM_KEYDOWN && dat->keyboardMsgQueue[i].wParam == wParam)
					break;
			if (i == dat->msgQueueCount)
				break;
	#ifdef EDITMSGQUEUE_PASSTHRUCLIPBOARD
			if (GetKeyState(VK_CONTROL) & 0x8000) {
				if (wParam == 'X' || wParam == 'C' || wParam == 'V' || wParam == VK_INSERT)
					break;
			}
			if (GetKeyState(VK_SHIFT) & 0x8000) {
				if (wParam == VK_INSERT || wParam == VK_DELETE)
					break;
			}
	#endif
			SaveKeyboardMessage(dat, msg, wParam, lParam);
			return 0;
		}
		break;
	case WM_KEYDOWN:
		if (GetWindowLong(hwnd, GWL_STYLE) & ES_READONLY) {
	#ifdef EDITMSGQUEUE_PASSTHRUCLIPBOARD
			if (GetKeyState(VK_CONTROL) & 0x8000) {
				if (wParam == 'X' || wParam == 'C' || wParam == 'V' || wParam == VK_INSERT)
					break;
			}
			if (GetKeyState(VK_SHIFT) & 0x8000) {
				if (wParam == VK_INSERT || wParam == VK_DELETE)
					break;
			}
	#endif
			SaveKeyboardMessage(dat, msg, wParam, lParam);
			return 0;
		}
		if (wParam == VK_UP && (GetKeyState(VK_CONTROL) & 0x8000) && DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_CTRLSUPPORT, SRMSGDEFSET_CTRLSUPPORT) && !DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_AUTOCLOSE, SRMSGDEFSET_AUTOCLOSE)) {
			if (pdat->cmdList) {
				if (!pdat->cmdListCurrent) {
					pdat->cmdListCurrent = tcmdlist_last(pdat->cmdList);
				//	SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
					SendMessage(hwnd, EM_SETTEXTEX, (WPARAM) &st, (LPARAM)pdat->cmdListCurrent->szCmd);
//					SetWindowText(hwnd, pdat->cmdListCurrent->szCmd);
					SendMessage(hwnd, EM_SCROLLCARET, 0,0);
				//	SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
					//SendMessage(hwnd, EM_SETSEL, 0, -1);
				}
				else if (pdat->cmdListCurrent->prev) {
					pdat->cmdListCurrent = pdat->cmdListCurrent->prev;
				//	SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
					SendMessage(hwnd, EM_SETTEXTEX, (WPARAM) &st, (LPARAM)pdat->cmdListCurrent->szCmd);
//					SetWindowText(hwnd, pdat->cmdListCurrent->szCmd);
					SendMessage(hwnd, EM_SCROLLCARET, 0,0);
				//	SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
					//SendMessage(hwnd, EM_SETSEL, 0, -1);
				}
			}
			EnableWindow(GetDlgItem(GetParent(hwnd), IDOK), GetWindowTextLength(GetDlgItem(GetParent(hwnd), IDC_MESSAGE)) != 0);
			UpdateReadChars(GetParent(hwnd), pdat);
			return 0;
		}
		else if (wParam == VK_DOWN && (GetKeyState(VK_CONTROL) & 0x8000) && DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_CTRLSUPPORT, SRMSGDEFSET_CTRLSUPPORT) && !DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_AUTOCLOSE, SRMSGDEFSET_AUTOCLOSE)) {
			if (pdat->cmdList) {
				if (!pdat->cmdListCurrent)
					pdat->cmdListCurrent = tcmdlist_last(pdat->cmdList);
				if (pdat->cmdListCurrent->next) {
					pdat->cmdListCurrent = pdat->cmdListCurrent->next;
					//SendMessage(hwnd, WM_SETREDRAW, FALSE, 0);
					SendMessage(hwnd, EM_SETTEXTEX, (WPARAM) &st, (LPARAM)pdat->cmdListCurrent->szCmd);
//					SetWindowText(hwnd, pdat->cmdListCurrent->szCmd);
					SendMessage(hwnd, EM_SCROLLCARET, 0,0);
					//SendMessage(hwnd, WM_SETREDRAW, TRUE, 0);
					//SendMessage(hwnd, EM_SETSEL, 0, -1);
				}
				else {
					pdat->cmdListCurrent = 0;
					SetWindowTextA(hwnd, "");
				}
			}
			EnableWindow(GetDlgItem(GetParent(hwnd), IDOK), GetWindowTextLength(GetDlgItem(GetParent(hwnd), IDC_MESSAGE)) != 0);
			UpdateReadChars(GetParent(hwnd), pdat);
			return 0;
		}
		if(wParam == VK_INSERT && (GetKeyState(VK_SHIFT) & 0x8000)) {
			SendMessage(hwnd, EM_PASTESPECIAL, CF_TEXT, 0); // shift insert
			return 0;
		}
		if ((GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) {
			if (wParam == VK_TAB) {	// ctrl-shift tab
				SendMessage(GetParent(GetParent(hwnd)), DM_ACTIVATEPREV, 0, (LPARAM)GetParent(hwnd));
				return 0;
			}
		}
		if ((GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)) {
			if (wParam == 'V') {    // ctrl v
				SendMessage(hwnd, EM_PASTESPECIAL, CF_TEXT, 0);
				return 0;
			}
			if (wParam == VK_TAB) { // ctrl tab
				SendMessage(GetParent(GetParent(hwnd)), DM_ACTIVATENEXT, 0, (LPARAM)GetParent(hwnd));
				return 0;
			}
		}
		if (wParam == VK_TAB && !(GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000)) {
//			SendMessage(hwnd, EM_REPLACESEL, FALSE, (LPARAM) "\t");
			return 0;
		}
		if(wParam == VK_ESCAPE && (GetKeyState(VK_SHIFT) & 0x8000)) {
			ShowWindow(GetParent(GetParent(hwnd)), SW_MINIMIZE);
			return 0;
		}
		if (wParam == VK_RETURN) {
			if (GetKeyState(VK_CONTROL) & 0x8000 && GetKeyState(VK_MENU) & 0x8000) {
				PostMessage(GetParent(hwnd), WM_COMMAND, IDC_SENDALL, 0);
				return 0;
			}
			if (((GetKeyState(VK_CONTROL) & 0x8000) != 0) ^ (0 != DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_SENDONENTER, SRMSGDEFSET_SENDONENTER))) {
				PostMessage(GetParent(hwnd), WM_COMMAND, IDOK, 0);
				return 0;
			}
			if (DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_SENDONDBLENTER, SRMSGDEFSET_SENDONDBLENTER)) {
				if (dat->lastEnterTime + ENTERCLICKTIME < GetTickCount())
					dat->lastEnterTime = GetTickCount();
				else {
					SendMessage(hwnd, WM_KEYDOWN, VK_BACK, 0);
					SendMessage(hwnd, WM_KEYUP, VK_BACK, 0);
//					SendMessage(hwnd, WM_CHAR, '\b', 0);
					PostMessage(GetParent(hwnd), WM_COMMAND, IDOK, 0);
					return 0;
				}
			}
		}
		else
			dat->lastEnterTime = 0;
			break;
		//fall through
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_MOUSEWHEEL:
	case WM_KILLFOCUS:
		dat->lastEnterTime = 0;
		break;
	case WM_SYSKEYDOWN:
		if ((wParam == VK_LEFT) && GetKeyState(VK_MENU) & 0x8000) {
			SendMessage(GetParent(GetParent(hwnd)), DM_ACTIVATEPREV, 0, (LPARAM)GetParent(hwnd));
			return 0;
		}
		if ((wParam == VK_RIGHT) && GetKeyState(VK_MENU) & 0x8000) {
			SendMessage(GetParent(GetParent(hwnd)), DM_ACTIVATENEXT, 0, (LPARAM)GetParent(hwnd));
			return 0;
		}
		break;
	case WM_SYSKEYUP:
		if ((wParam == VK_LEFT) && GetKeyState(VK_MENU) & 0x8000) {
			return 0;
		}
		if ((wParam == VK_RIGHT) && GetKeyState(VK_MENU) & 0x8000) {
			return 0;
		}
		break;
	case WM_SYSCHAR:
		dat->lastEnterTime = 0;
		if ((wParam == 's' || wParam == 'S') && GetKeyState(VK_MENU) & 0x8000) {
			if (GetWindowLong(hwnd, GWL_STYLE) & ES_READONLY)
				SaveKeyboardMessage(dat, msg, wParam, lParam);
			else
				PostMessage(GetParent(hwnd), WM_COMMAND, IDOK, 0);
			return 0;
		}
		break;
	case WM_DROPFILES:
		SendMessage(GetParent(hwnd), WM_DROPFILES, wParam, lParam);
		return 0;
	case EM_UNSUBCLASSED:
		if (dat->keyboardMsgQueue)
			free(dat->keyboardMsgQueue);
		free(dat);
		return 0;
	}
	return CallWindowProc(OldMessageEditProc, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK SplitterSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
		case WM_NCHITTEST:
		  return HTCLIENT;
		case WM_SETCURSOR:
		{
			RECT rc;
				GetClientRect(hwnd, &rc);
				SetCursor(rc.right > rc.bottom ? hCurSplitNS : hCurSplitWE);
				return TRUE;
		}
		case WM_LBUTTONDOWN:
			SetCapture(hwnd);
			return 0;
		case WM_MOUSEMOVE:
			if (GetCapture() == hwnd) {
				RECT rc;
				GetClientRect(hwnd, &rc);
				SendMessage(GetParent(hwnd), DM_SPLITTERMOVED, rc.right > rc.bottom ? (short) HIWORD(GetMessagePos()) + rc.bottom / 2 : (short) LOWORD(GetMessagePos()) + rc.right / 2, (LPARAM) hwnd);
			}
			return 0;
		case WM_LBUTTONUP:
			ReleaseCapture();
			return 0;
	}
	return CallWindowProc(OldSplitterProc, hwnd, msg, wParam, lParam);
}

static void MessageDialogResize(HWND hwndDlg, struct MessageWindowData *dat, int w, int h) {
	HDWP hdwp;
	struct ParentWindowData *pdat = (struct ParentWindowData *) GetWindowLong(GetParent(hwndDlg), GWL_USERDATA);
	int vSplitterPos = 0, hSplitterPos = dat->splitterPos, toolbarHeight = pdat->flags&SMF_SHOWBTNS ? dat->toolbarHeight : 0;
	int hSplitterMinTop = toolbarHeight + dat->minLogBoxHeight, hSplitterMinBottom = dat->minEditBoxHeight;

	if (h-hSplitterPos < hSplitterMinTop) {
		hSplitterPos = h - hSplitterMinTop;
	}
	if (hSplitterPos < hSplitterMinBottom) {
		hSplitterPos = hSplitterMinBottom;
	}
	dat->splitterPos = hSplitterPos;
	SendMessage(hwndDlg, DM_AVATARCALCSIZE, 0, 0);
	if (hSplitterPos + toolbarHeight < dat->avatarHeight) {
		hSplitterPos = dat->avatarHeight - toolbarHeight;
	}
	dat->splitterPos = hSplitterPos;
	hdwp = BeginDeferWindowPos(12);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_LOG), 0, 0, 0, w-vSplitterPos, h-hSplitterPos-toolbarHeight-1, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_MESSAGE), 0, 0, h-hSplitterPos+2, w-(dat->avatarWidth ? dat->avatarWidth+1 : 0), hSplitterPos-2, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_SPLITTER), 0, 0, h - hSplitterPos-1, w-dat->avatarWidth, 3, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_USERMENU), 0, 0, h - hSplitterPos - toolbarHeight+1, 24, 24, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_DETAILS), 0, 24, h - hSplitterPos - toolbarHeight+1, 24, 24, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_SMILEYS), 0, 60, h - hSplitterPos - toolbarHeight+1, 24, 24, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_ADD), 0, w-4*24-38-dat->avatarWidth, h - hSplitterPos - toolbarHeight+1, 24, 24, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_HISTORY), 0, w-3*24-38-dat->avatarWidth, h - hSplitterPos - toolbarHeight+1, 24, 24, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_QUOTE), 0, w-2*24-38-dat->avatarWidth, h - hSplitterPos - toolbarHeight+1, 24, 24, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDCANCEL), 0, w-24-38-dat->avatarWidth, h - hSplitterPos - toolbarHeight+1, 24, 24, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDOK), 0, w-38-dat->avatarWidth, h - hSplitterPos - toolbarHeight+1, 38, 24, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_AVATAR), 0, w-dat->avatarWidth, h - dat->avatarHeight, dat->avatarWidth, dat->avatarHeight, SWP_NOZORDER);
//	hdwp = DeferWindowPos(hdwp, GetDlgItem(hwndDlg, IDC_AVATAR), 0, w-dat->avatarWidth, h - (hSplitterPos + toolbarHeight + dat->avatarHeight)/2, dat->avatarWidth, dat->avatarHeight, SWP_NOZORDER);
	EndDeferWindowPos(hdwp);
	if (dat->hwndLog != NULL) {
		IEVIEWWINDOW ieWindow;
		ieWindow.cbSize = sizeof(IEVIEWWINDOW);
		ieWindow.iType = IEW_SETPOS;
		ieWindow.parent = hwndDlg;
		ieWindow.hwnd = dat->hwndLog;
        ieWindow.x = 0;
        ieWindow.y = 0;
        ieWindow.cx = w-vSplitterPos;
        ieWindow.cy = h-hSplitterPos - toolbarHeight - 1;
		CallService(MS_IEVIEW_WINDOW, 0, (LPARAM)&ieWindow);
	} else {
		RedrawWindow(GetDlgItem(hwndDlg, IDC_LOG), NULL, NULL, RDW_INVALIDATE);
	}
}

static void UpdateReadChars(HWND hwndDlg, struct MessageWindowData * dat)
{
	if (dat->parent->hwndActive == hwndDlg && dat->parent->hwndStatus) {
		TCHAR buf[128];
		int len = GetWindowTextLength(GetDlgItem(hwndDlg, IDC_MESSAGE));
		_sntprintf(buf, sizeof(buf), _T("%d"), len);
		SendMessage(dat->parent->hwndStatus, SB_SETTEXT, 1, (LPARAM) buf);
	}
}

void ShowAvatar(HWND hwndDlg, struct MessageWindowData *dat) {
	DBVARIANT dbv;

	if (dat->avatarPic) {
		DeleteObject(dat->avatarPic);
        dat->avatarPic=0;
	}
	if (DBGetContactSetting(dat->hContact, SRMMMOD, SRMSGSET_AVATAR, &dbv)) {
		SendMessage(hwndDlg, DM_AVATARCALCSIZE, 0, 0);
		SendMessage(hwndDlg, WM_SIZE, 0, 0);
	}
	else {
		HANDLE hFile;

		if((hFile = CreateFileA(dbv.pszVal, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
			SendMessage(hwndDlg, DM_AVATARCALCSIZE, 0, 0);
		}
		else {
			dat->avatarPic=(HBITMAP)CallService(MS_UTILS_LOADBITMAP,0,(LPARAM)dbv.pszVal);
			SendMessage(hwndDlg, DM_AVATARCALCSIZE, 0, 0);
			CloseHandle(hFile);
		}
		DBFreeVariant(&dbv);
	}
	SendMessage(hwndDlg, DM_SCROLLLOGTOBOTTOM, 0, 0);
	SendMessage(hwndDlg, WM_SIZE, 0, 0);
}

static void NotifyTyping(struct MessageWindowData *dat, int mode)
{
	DWORD protoStatus;
	DWORD protoCaps;
	DWORD typeCaps;

	if (!dat->hContact)
		return;
	// Don't send to protocols who don't support typing
	// Don't send to users who are unchecked in the typing notification options
	// Don't send to protocols that are offline
	// Don't send to users who are not visible and
	// Don't send to users who are not on the visible list when you are in invisible mode.
	if (!DBGetContactSettingByte(dat->hContact, SRMMMOD, SRMSGSET_TYPING, DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_TYPINGNEW, SRMSGDEFSET_TYPINGNEW)))
		return;
	if (!dat->szProto)
		return;
	protoStatus = CallProtoService(dat->szProto, PS_GETSTATUS, 0, 0);
	protoCaps = CallProtoService(dat->szProto, PS_GETCAPS, PFLAGNUM_1, 0);
	typeCaps = CallProtoService(dat->szProto, PS_GETCAPS, PFLAGNUM_4, 0);

	if (!(typeCaps & PF4_SUPPORTTYPING))
		return;
	if (protoStatus < ID_STATUS_ONLINE)
		return;
	if (protoCaps & PF1_VISLIST && DBGetContactSettingWord(dat->hContact, dat->szProto, "ApparentMode", 0) == ID_STATUS_OFFLINE)
		return;
	if (protoCaps & PF1_INVISLIST && protoStatus == ID_STATUS_INVISIBLE && DBGetContactSettingWord(dat->hContact, dat->szProto, "ApparentMode", 0) != ID_STATUS_ONLINE)
		return;
	if (DBGetContactSettingByte(dat->hContact, "CList", "NotOnList", 0)
		&& !DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_TYPINGUNKNOWN, SRMSGDEFSET_TYPINGUNKNOWN))
		return;
	// End user check
	dat->nTypeMode = mode;
	CallService(MS_PROTO_SELFISTYPING, (WPARAM) dat->hContact, dat->nTypeMode);
}

BOOL CALLBACK DlgProcMessage(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	struct MessageWindowData *dat;
	dat = (struct MessageWindowData *) GetWindowLong(hwndDlg, GWL_USERDATA);
	if (!dat && msg!=WM_INITDIALOG) return FALSE;
	switch (msg) {
	case WM_INITDIALOG:
		{
			int notifyUnread = 0;
			struct NewMessageWindowLParam *newData = (struct NewMessageWindowLParam *) lParam;
			TranslateDialogDefault(hwndDlg);
			dat = (struct MessageWindowData *) malloc(sizeof(struct MessageWindowData));
			SetWindowLong(hwndDlg, GWL_USERDATA, (LONG) dat);
			{
				dat->hContact = newData->hContact;
				NotifyLocalWinEvent(dat->hContact, hwndDlg, MSG_WINDOW_EVT_OPENING);
				if (newData->szInitialText) {
					int len;
					SetDlgItemTextA(hwndDlg, IDC_MESSAGE, newData->szInitialText);
					len = GetWindowTextLength(GetDlgItem(hwndDlg, IDC_MESSAGE));
					PostMessage(GetDlgItem(hwndDlg, IDC_MESSAGE), EM_SETSEL, len, len);
				}
			}
			dat->hwnd = hwndDlg;
			dat->hwndParent = GetParent(hwndDlg);
			dat->hwndLog = NULL;
			dat->parent = (struct ParentWindowData *) GetWindowLong(dat->hwndParent, GWL_USERDATA);
			dat->szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) dat->hContact, 0);
			RichUtil_SubClass(GetDlgItem(hwndDlg, IDC_LOG));
			{ // avatar stuff
				dat->avatarPic = 0;
//				dat->limitAvatarMaxH = DBGetContactSettingDword(NULL, SRMMMOD, SRMSGSET_AVHEIGHT, SRMSGDEFSET_AVHEIGHT);
//				dat->limitAvatarMinH = DBGetContactSettingDword(NULL, SRMMMOD, SRMSGSET_AVHEIGHTMIN, SRMSGDEFSET_AVHEIGHTMIN);
			}
			if (dat->hContact && dat->szProto != NULL)
				dat->wStatus = DBGetContactSettingWord(dat->hContact, dat->szProto, "Status", ID_STATUS_OFFLINE);
			else
				dat->wStatus = ID_STATUS_OFFLINE;
			dat->wOldStatus = dat->wStatus;
			dat->sendInfo = NULL;
			dat->hDbEventFirst = NULL;
			dat->hDbEventLast = NULL;
//			dat->sendBuffer = NULL;
			dat->sendCount = 0;
			dat->messagesInProgress = 0;
//			dat->nFlash = 0;
			dat->nTypeSecs = 0;
			dat->nLastTyping = 0;
			dat->showTyping = 0;
			dat->showUnread = 0;
			dat->cmdList = 0;
			dat->cmdListCurrent = 0;
			dat->nTypeMode = PROTOTYPE_SELFTYPING_OFF;
			SetTimer(hwndDlg, TIMERID_TYPE, 1000, NULL);
			dat->lastMessage = 0;
			dat->lastEventType = -1;
			dat->lastEventTime = time(NULL);
			dat->startTime = time(NULL);
			dat->flags = 0;
			if (DBGetContactSettingByte(dat->hContact, SRMMMOD, "UseRTL", (BYTE) 0)) {
				PARAFORMAT2 pf2;
				ZeroMemory((void *)&pf2, sizeof(pf2));
				pf2.cbSize = sizeof(pf2);
				pf2.dwMask = PFM_RTLPARA;
				pf2.wEffects = PFE_RTLPARA;
				SetWindowLong(GetDlgItem(hwndDlg, IDC_MESSAGE),GWL_EXSTYLE,GetWindowLong(GetDlgItem(hwndDlg, IDC_MESSAGE),GWL_EXSTYLE) | WS_EX_RIGHT | WS_EX_RTLREADING | WS_EX_LEFTSCROLLBAR);
				SetWindowLong(GetDlgItem(hwndDlg, IDC_LOG),GWL_EXSTYLE,GetWindowLong(GetDlgItem(hwndDlg, IDC_LOG),GWL_EXSTYLE) | WS_EX_RIGHT | WS_EX_RTLREADING | WS_EX_LEFTSCROLLBAR);
				SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETPARAFORMAT, 0, (LPARAM)&pf2);
				dat->flags |= SMF_RTL;
			}
			if (DBGetContactSettingByte(dat->hContact, SRMMMOD, "DisableUnicode", (BYTE) 0)) {
				dat->flags |= SMF_DISABLE_UNICODE;
			}
			dat->flags |= ServiceExists(MS_IEVIEW_WINDOW) ? g_dat->flags & SMF_USEIEVIEW : 0;
			dat->codePage = DBGetContactSettingWord(dat->hContact, SRMMMOD, "CodePage", (WORD) CP_ACP);
//			dat->nFlashMax = DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_FLASHCOUNT, SRMSGDEFSET_FLASHCOUNT);
			{
				RECT rc;
				POINT pt;
				GetWindowRect(GetDlgItem(hwndDlg, IDC_SPLITTER), &rc);
				pt.y = (rc.top + rc.bottom) / 2;
				pt.x = 0;
				ScreenToClient(hwndDlg, &pt);
				dat->originalSplitterPos = pt.y;
				if (dat->splitterPos == -1)
					dat->splitterPos = dat->originalSplitterPos;// + 60;
				GetWindowRect(GetDlgItem(hwndDlg, IDC_ADD), &rc);
				dat->toolbarHeight = 24 +3;//rc.bottom - rc.top + 3;
			}
			WindowList_Add(g_dat->hMessageWindowList, hwndDlg, dat->hContact);
			GetWindowRect(GetDlgItem(hwndDlg, IDC_MESSAGE), &dat->minEditInit);
			dat->minEditBoxHeight = dat->minEditInit.bottom - dat->minEditInit.top;
			dat->minLogBoxHeight = dat->minEditBoxHeight;

			SendMessage(hwndDlg, DM_CHANGEICONS, 0, 0);
			// Make them flat buttons
			{
				int i;
				for (i = 0; i < sizeof(buttonLineControls) / sizeof(buttonLineControls[0]); i++)
					SendMessage(GetDlgItem(hwndDlg, buttonLineControls[i]), BUTTONSETASFLATBTN, 0, 0);
			}
			SendMessage(GetDlgItem(hwndDlg, IDC_ADD), BUTTONADDTOOLTIP, (WPARAM) Translate("Add Contact Permanently to List"), 0);
			SendMessage(GetDlgItem(hwndDlg, IDC_USERMENU), BUTTONADDTOOLTIP, (WPARAM) Translate("User Menu"), 0);
			SendMessage(GetDlgItem(hwndDlg, IDC_DETAILS), BUTTONADDTOOLTIP, (WPARAM) Translate("View User's Details"), 0);
			SendMessage(GetDlgItem(hwndDlg, IDC_HISTORY), BUTTONADDTOOLTIP, (WPARAM) Translate("View User's History"), 0);

			SendMessage(GetDlgItem(hwndDlg, IDC_QUOTE), BUTTONADDTOOLTIP, (WPARAM) Translate("Quote Text"), 0);
			SendMessage(GetDlgItem(hwndDlg, IDC_SMILEYS), BUTTONADDTOOLTIP, (WPARAM) Translate("Insert Emoticon"), 0);
			SendMessage(GetDlgItem(hwndDlg, IDOK), BUTTONADDTOOLTIP, (WPARAM) Translate("Send Message"), 0);
			SendMessage(GetDlgItem(hwndDlg, IDCANCEL), BUTTONADDTOOLTIP, (WPARAM) Translate("Close Session"), 0);

			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_SETOLECALLBACK, 0, (LPARAM) & reOleCallback);
			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_SETEVENTMASK, 0, ENM_MOUSEEVENTS | ENM_LINK);
			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_SETEDITSTYLE, SES_EXTENDBACKCOLOR, SES_EXTENDBACKCOLOR);

			SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETEVENTMASK, 0, ENM_MOUSEEVENTS | ENM_KEYEVENTS | ENM_CHANGE);
// IEVIew MOD Begin
			if (dat->flags & SMF_USEIEVIEW) {
				IEVIEWWINDOW ieWindow;
				ieWindow.cbSize = sizeof(IEVIEWWINDOW);
				ieWindow.iType = IEW_CREATE;
				ieWindow.dwFlags = 0;
				ieWindow.dwMode = IEWM_SCRIVER;
				ieWindow.parent = hwndDlg;
				ieWindow.x = 0;
				ieWindow.y = 0;
				ieWindow.cx = 200;
				ieWindow.cy = 300;
				CallService(MS_IEVIEW_WINDOW, 0, (LPARAM)&ieWindow);
				dat->hwndLog = ieWindow.hwnd;
			}
// IEVIew MOD End
			/* duh, how come we didnt use this from the start? */
			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_AUTOURLDETECT, (WPARAM) TRUE, 0);
			if (dat->hContact) {
				if (dat->szProto) {
					int nMax;
					nMax = CallProtoService(dat->szProto, PS_GETCAPS, PFLAG_MAXLENOFMESSAGE, (LPARAM) dat->hContact);
					if (nMax)
						SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_LIMITTEXT, (WPARAM) nMax, 0);
					/* get around a lame bug in the Windows template resource code where richedits are limited to 0x7FFF */
					SendDlgItemMessage(hwndDlg, IDC_LOG, EM_LIMITTEXT, (WPARAM) sizeof(TCHAR) * 0x7FFFFFFF, 0);
				}
			}
			OldMessageEditProc = (WNDPROC) SetWindowLong(GetDlgItem(hwndDlg, IDC_MESSAGE), GWL_WNDPROC, (LONG) MessageEditSubclassProc);
			SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SUBCLASSED, 0, 0);
			OldSplitterProc = (WNDPROC) SetWindowLong(GetDlgItem(hwndDlg, IDC_SPLITTER), GWL_WNDPROC, (LONG) SplitterSubclassProc);
			if (dat->hContact) {
				int historyMode = DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_LOADHISTORY, SRMSGDEFSET_LOADHISTORY);
				// This finds the first message to display, it works like shit
				dat->hDbEventFirst = (HANDLE) CallService(MS_DB_EVENT_FINDFIRSTUNREAD, (WPARAM) dat->hContact, 0);
				if (dat->hDbEventFirst != NULL) {
					DBEVENTINFO dbei = { 0 };
					dbei.cbSize = sizeof(dbei);
					CallService(MS_DB_EVENT_GET, (WPARAM) dat->hDbEventFirst, (LPARAM) & dbei);
					if (dbei.eventType == EVENTTYPE_MESSAGE && !(dbei.flags & DBEF_READ) && !(dbei.flags & DBEF_SENT)) {
						notifyUnread = 1;
					}
				}
				switch (historyMode) {
				case LOADHISTORY_COUNT:
					{
						int i;
						HANDLE hPrevEvent;
						DBEVENTINFO dbei = { 0 };
						dbei.cbSize = sizeof(dbei);
						for (i = DBGetContactSettingWord(NULL, SRMMMOD, SRMSGSET_LOADCOUNT, SRMSGDEFSET_LOADCOUNT); i > 0; i--) {
							if (dat->hDbEventFirst == NULL)
								hPrevEvent = (HANDLE) CallService(MS_DB_EVENT_FINDLAST, (WPARAM) dat->hContact, 0);
							else
								hPrevEvent = (HANDLE) CallService(MS_DB_EVENT_FINDPREV, (WPARAM) dat->hDbEventFirst, 0);
							if (hPrevEvent == NULL)
								break;
							dbei.cbBlob = 0;
							dat->hDbEventFirst = hPrevEvent;
							CallService(MS_DB_EVENT_GET, (WPARAM) dat->hDbEventFirst, (LPARAM) & dbei);
							if (!DbEventIsShown(&dbei, dat))
								i++;
						}
						break;
					}
				case LOADHISTORY_TIME:
					{
						HANDLE hPrevEvent;
						DBEVENTINFO dbei = { 0 };
						DWORD firstTime;

						dbei.cbSize = sizeof(dbei);
						if (dat->hDbEventFirst == NULL)
							dbei.timestamp = time(NULL);
						else
							CallService(MS_DB_EVENT_GET, (WPARAM) dat->hDbEventFirst, (LPARAM) & dbei);
						firstTime = dbei.timestamp - 60 * DBGetContactSettingWord(NULL, SRMMMOD, SRMSGSET_LOADTIME, SRMSGDEFSET_LOADTIME);
						for (;;) {
							if (dat->hDbEventFirst == NULL)
								hPrevEvent = (HANDLE) CallService(MS_DB_EVENT_FINDLAST, (WPARAM) dat->hContact, 0);
							else
								hPrevEvent = (HANDLE) CallService(MS_DB_EVENT_FINDPREV, (WPARAM) dat->hDbEventFirst, 0);
							if (hPrevEvent == NULL)
								break;
							dbei.cbBlob = 0;
							CallService(MS_DB_EVENT_GET, (WPARAM) hPrevEvent, (LPARAM) & dbei);
							if (dbei.timestamp < firstTime)
								break;
							dat->hDbEventFirst = hPrevEvent;
						}
						break;
					}
				}
			}
			{
				int saveSplitterPerContact = DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_SAVESPLITTERPERCONTACT, SRMSGDEFSET_SAVESPLITTERPERCONTACT);
				dat->splitterPos = (int) DBGetContactSettingDword(saveSplitterPerContact ? dat->hContact : NULL, SRMMMOD, "splitterPos", (DWORD) - 1);
			}
			SendMessage(dat->hwndParent, DM_ADDCHILD, 0, (LPARAM) dat);
			SendMessage(hwndDlg, DM_OPTIONSAPPLIED, 0, 0);
//			SendMessage(hwndDlg, DM_AVATARCALCSIZE, 0, 0);
			{
				DBEVENTINFO dbei = { 0 };
				HANDLE hdbEvent;

				dbei.cbSize = sizeof(dbei);
				hdbEvent = (HANDLE) CallService(MS_DB_EVENT_FINDLAST, (WPARAM) dat->hContact, 0);
				if (hdbEvent) {
					do {
						ZeroMemory(&dbei, sizeof(dbei));
						dbei.cbSize = sizeof(dbei);
						CallService(MS_DB_EVENT_GET, (WPARAM) hdbEvent, (LPARAM) & dbei);
						if (dbei.eventType == EVENTTYPE_MESSAGE && !(dbei.flags & DBEF_SENT)) {
							dat->lastMessage = dbei.timestamp;
							SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
							break;
						}
					}
					while (hdbEvent = (HANDLE) CallService(MS_DB_EVENT_FINDPREV, (WPARAM) hdbEvent, 0));
				}
			}
			SendMessage(dat->hwndParent, DM_ACTIVATENEWCHILD, 0, (LPARAM) hwndDlg);
//			ShowWindow(hwndDlg, SW_SHOWNORMAL);
			SetFocus(GetDlgItem(hwndDlg, IDC_MESSAGE));
			if (newData->minimized) {
				ShowWindow(dat->hwndParent, SW_MINIMIZE);
			} else {
				ShowWindow(dat->hwndParent, SW_SHOW);
			}
			NotifyLocalWinEvent(dat->hContact, hwndDlg, MSG_WINDOW_EVT_OPEN);
			if (notifyUnread) {
				SendMessage(dat->hwndParent, DM_STARTFLASHING, 0, 0); 
				if (GetActiveWindow() != dat->hwndParent || GetForegroundWindow() != dat->hwndParent || dat->parent->hwndActive != hwndDlg) {
					dat->showUnread = 0;
					SetTimer(hwndDlg, TIMERID_FLASHWND, TIMEOUT_FLASHWND, NULL);
				}
			}
			return TRUE;
		}
	case WM_CONTEXTMENU:
	{
		if (dat->hwndParent == (HWND) wParam) {
			POINT pt;
			HMENU hMenu = (HMENU) CallService(MS_CLIST_MENUBUILDCONTACT, (WPARAM) dat->hContact, 0);

			GetCursorPos(&pt);
			TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, hwndDlg, NULL);
			DestroyMenu(hMenu);
		}
		break;
	}
	case WM_DROPFILES:
	{
		if (dat->szProto==NULL) break;
		if (!(CallProtoService(dat->szProto, PS_GETCAPS, PFLAGNUM_1,0)&PF1_FILESEND)) break;
		if (dat->wStatus==ID_STATUS_OFFLINE) break;
		if (dat->hContact!=NULL) {
			HDROP hDrop;
			char **ppFiles=NULL;
			char szFilename[MAX_PATH];
			int fileCount,totalCount=0,i;

			hDrop=(HDROP)wParam;
			fileCount=DragQueryFile(hDrop,-1,NULL,0);
			ppFiles=NULL;
			for(i=0;i<fileCount;i++) {
				DragQueryFileA(hDrop, i, szFilename, sizeof(szFilename));
				AddToFileList(&ppFiles, &totalCount, szFilename);
			}
			CallServiceSync(MS_FILE_SENDSPECIFICFILES, (WPARAM)dat->hContact, (LPARAM)ppFiles);
			for(i=0;ppFiles[i];i++) free(ppFiles[i]);
			free(ppFiles);
		}
		break;
	}
	case HM_AVATARACK:
	{
		ACKDATA *pAck = (ACKDATA *)lParam;
		PROTO_AVATAR_INFORMATION *pai = (PROTO_AVATAR_INFORMATION *)pAck->hProcess;
		if (pAck->hContact!=dat->hContact)
			return 0;
		if (pAck->type != ACKTYPE_AVATAR)
			return 0;
		if (pai==NULL)
			return 0;
		if (pAck->result == ACKRESULT_SUCCESS) {
			if (pai->filename&&strlen(pai->filename)&&VALID_AVATAR(pai->format)) {
				DBWriteContactSettingString(dat->hContact, SRMMMOD, SRMSGSET_AVATAR, pai->filename);
				ShowAvatar(hwndDlg, dat);
			}
		}
		else if (pAck->result == ACKRESULT_STATUS) {
			SendMessage(hwndDlg, DM_GETAVATAR, 0, 0);
		}
		else if (pAck->result == ACKRESULT_FAILED) {
			DBDeleteContactSetting(dat->hContact, SRMMMOD, SRMSGSET_AVATAR);
			SendMessage(hwndDlg, DM_GETAVATAR, 0, 0);
		}
		break;
	}
	case DM_CHANGEICONS:
		SendDlgItemMessage(hwndDlg, IDC_ADD, BM_SETIMAGE, IMAGE_ICON, (LPARAM) g_dat->hIcons[SMF_ICON_ADD]);
		SendDlgItemMessage(hwndDlg, IDC_DETAILS, BM_SETIMAGE, IMAGE_ICON, (LPARAM) g_dat->hIcons[SMF_ICON_USERDETAILS]);
		SendDlgItemMessage(hwndDlg, IDC_HISTORY, BM_SETIMAGE, IMAGE_ICON, (LPARAM) g_dat->hIcons[SMF_ICON_HISTORY]);
		SendDlgItemMessage(hwndDlg, IDC_QUOTE, BM_SETIMAGE, IMAGE_ICON, (LPARAM) g_dat->hIcons[SMF_ICON_QUOTE]);
		SendDlgItemMessage(hwndDlg, IDC_SMILEYS, BM_SETIMAGE, IMAGE_ICON, (LPARAM) g_dat->hIcons[SMF_ICON_SMILEY]);
		SendDlgItemMessage(hwndDlg, IDOK, BM_SETIMAGE, IMAGE_ICON, (LPARAM) g_dat->hIcons[SMF_ICON_SEND]);
		SendDlgItemMessage(hwndDlg, IDCANCEL, BM_SETIMAGE, IMAGE_ICON, (LPARAM) g_dat->hIcons[SMF_ICON_CANCEL]);
		SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
		break;
	case DM_AVATARCALCSIZE:
	{
		BITMAP bminfo;
		int avatarH;
		struct ParentWindowData *pdat;
		pdat = (struct ParentWindowData *) GetWindowLong(dat->hwndParent, GWL_USERDATA);
		dat->avatarWidth = 0;
		dat->avatarHeight = 0;
		if (dat->avatarPic==0||!(g_dat->flags&SMF_AVATAR)) {
			ShowWindow(GetDlgItem(hwndDlg, IDC_AVATAR), SW_HIDE);
			return 0;
		}
		GetObject(dat->avatarPic, sizeof(bminfo), &bminfo);
		dat->avatarHeight = avatarH = dat->splitterPos + ((pdat->flags&SMF_SHOWBTNS) ? dat->toolbarHeight : 0);//- 3;
		if (g_dat->flags & SMF_LIMITAVATARH) {
			if (avatarH < g_dat->limitAvatarMinH) {
				avatarH = g_dat->limitAvatarMinH;
			}
			if (avatarH > g_dat->limitAvatarMaxH) {
				avatarH = g_dat->limitAvatarMaxH;
			}
		}
		{
			RECT rc;
			double aspect = 0;
			GetClientRect(hwndDlg, &rc);
			dat->avatarHeight = avatarH;
			aspect = (double)dat->avatarHeight / (double)bminfo.bmHeight;
			dat->avatarWidth = (int)(bminfo.bmWidth * aspect);
			// if edit box width < min then adjust avatarWidth
			if (rc.right - dat->avatarWidth < 240) {
				dat->avatarWidth = rc.right - 240;
				if (dat->avatarWidth < 0) dat->avatarWidth = 0;
				aspect = (double)dat->avatarWidth / (double)bminfo.bmWidth;
				dat->avatarHeight = (int)(bminfo.bmHeight * aspect);

			}
		}
		ShowWindow(GetDlgItem(hwndDlg, IDC_AVATAR), SW_SHOW);
		break;
	}
	case DM_GETAVATAR:
	{
		PROTO_AVATAR_INFORMATION pai;
		int result;
		SetWindowLong(hwndDlg, DWL_MSGRESULT, 0);
		//Disable avatars
        if (!(g_dat->flags&SMF_AVATAR)) {
			SendMessage(hwndDlg, DM_AVATARCALCSIZE, 0, 0);
			SetWindowLong(hwndDlg, DWL_MSGRESULT, 1);
			return 0;
		}
		//Use contact photo
        if (!(CallProtoService(dat->szProto, PS_GETCAPS, PFLAGNUM_4, 0)&PF4_AVATARS)) {
			DBVARIANT dbv;
			if (!DBGetContactSetting(dat->hContact, "ContactPhoto", "File", &dbv)) {
				DBWriteContactSettingString(dat->hContact, SRMMMOD, SRMSGSET_AVATAR, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			ShowAvatar(hwndDlg, dat);
			SetWindowLong(hwndDlg, DWL_MSGRESULT, 1);
			return 0;
		}
		if(DBGetContactSettingWord(dat->hContact, dat->szProto, "Status", ID_STATUS_OFFLINE) == ID_STATUS_OFFLINE) {
			ShowAvatar(hwndDlg, dat);
			SetWindowLong(hwndDlg, DWL_MSGRESULT, 1);
			return 0;
		}
		ZeroMemory((void *)&pai, sizeof(pai));
		pai.cbSize = sizeof(pai);
		pai.hContact = dat->hContact;
		pai.format = PA_FORMAT_UNKNOWN;
		strcpy(pai.filename, "");
		result = CallProtoService(dat->szProto, PS_GETAVATARINFO, GAIF_FORCE, (LPARAM)&pai);
		if (result==GAIR_SUCCESS) {
			if (VALID_AVATAR(pai.format)) {
				DBVARIANT dbv;
				DBWriteContactSettingString(dat->hContact, SRMMMOD, SRMSGSET_AVATAR, pai.filename);
				if (DBGetContactSetting(dat->hContact, "ContactPhoto", "File", &dbv)) {
					DBWriteContactSettingString(dat->hContact, "ContactPhoto", "File", pai.filename);
				} else {
					DBFreeVariant(&dbv);
				}
			} else DBDeleteContactSetting(dat->hContact, SRMMMOD, SRMSGSET_AVATAR);
			ShowAvatar(hwndDlg, dat);
		} else if (result==GAIR_NOAVATAR) {
			DBVARIANT dbv;
			DBDeleteContactSetting(dat->hContact, SRMMMOD, SRMSGSET_AVATAR);
			if (!DBGetContactSetting(dat->hContact, "ContactPhoto", "File", &dbv)) {
				DBWriteContactSettingString(dat->hContact, SRMMMOD, SRMSGSET_AVATAR, dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			ShowAvatar(hwndDlg, dat);
		}
		SetWindowLong(hwndDlg, DWL_MSGRESULT, 1);
		break;
	}
	case DM_TYPING:
		{
			dat->nTypeSecs = (int) lParam > 0 ? (int) lParam : 0;
			break;
		}
	case DM_UPDATEWINICON:
		{
			SendMessage(dat->hwndParent, DM_UPDATEWINICON, wParam, (LPARAM)dat);
			break;
		}
    case DM_USERNAMETOCLIP:
		{
			CONTACTINFO ci;
			char buf[128];
			HGLOBAL hData;

			buf[0] = 0;
			if(dat->hContact) {
				ZeroMemory(&ci, sizeof(ci));
				ci.cbSize = sizeof(ci);
				ci.hContact = dat->hContact;
				ci.szProto = dat->szProto;
				ci.dwFlag = CNF_UNIQUEID;
				if (!CallService(MS_CONTACT_GETCONTACTINFO, 0, (LPARAM) & ci)) {
					switch (ci.type) {
						case CNFT_ASCIIZ:
							mir_snprintf(buf, sizeof(buf), "%s", ci.pszVal);
							miranda_sys_free(ci.pszVal);
							break;
						case CNFT_DWORD:
							mir_snprintf(buf, sizeof(buf), "%u", ci.dVal);
							break;
					}
				}
				if (!OpenClipboard(hwndDlg) || !lstrlenA(buf)) break;
				EmptyClipboard();
				hData = GlobalAlloc(GMEM_MOVEABLE, lstrlenA(buf) + 1);
				lstrcpyA(GlobalLock(hData), buf);
				GlobalUnlock(hData);
				SetClipboardData(CF_TEXT, hData);
				CloseClipboard();
			}
			break;
		}
	case DM_OPTIONSAPPLIED:
		{
			// avatar stuff
//			dat->avatarPic = 0;
//			dat->limitAvatarMaxH = 0;
//			dat->limitAvatarMaxH = 0;
//			if (CallProtoService(dat->szProto, PS_GETCAPS, PFLAGNUM_4, 0)&PF4_AVATARS) {
//			dat->limitAvatarMinH = DBGetContactSettingDword(NULL, SRMMMOD, SRMSGSET_AVHEIGHTMIN, SRMSGDEFSET_AVHEIGHTMIN);
//			dat->limitAvatarMaxH = DBGetContactSettingDword(NULL, SRMMMOD, SRMSGSET_AVHEIGHT, SRMSGDEFSET_AVHEIGHT);
//			}
			SendMessage(hwndDlg, DM_GETAVATAR, 0, 0);
		}
		SetDialogToType(hwndDlg);
		{
			COLORREF colour = DBGetContactSettingDword(NULL, SRMMMOD, SRMSGSET_BKGCOLOUR, SRMSGDEFSET_BKGCOLOUR);
			SendDlgItemMessage(hwndDlg, IDC_LOG, EM_SETBKGNDCOLOR, 0, colour);
			colour = DBGetContactSettingDword(NULL, SRMMMOD, SRMSGSET_INPUTBKGCOLOUR, SRMSGDEFSET_INPUTBKGCOLOUR);
			SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETBKGNDCOLOR, 0, colour);

		}
		InvalidateRect(GetDlgItem(hwndDlg, IDC_MESSAGE), NULL, FALSE);
		{
			COLORREF colour;
			CHARFORMAT2A cf2 = {0};
			LOGFONTA lf;
			LoadMsgDlgFont(MSGFONTID_MESSAGEAREA, &lf, &colour);
			cf2.dwMask = CFM_COLOR | CFM_FACE | CFM_CHARSET | CFM_SIZE | CFM_WEIGHT | CFM_BOLD | CFM_ITALIC;
			cf2.cbSize = sizeof(cf2);
			cf2.crTextColor = colour;
			cf2.bCharSet = lf.lfCharSet;
			strncpy(cf2.szFaceName, lf.lfFaceName, LF_FACESIZE);
			cf2.dwEffects = ((lf.lfWeight >= FW_BOLD) ? CFE_BOLD : 0) | (lf.lfItalic ? CFE_ITALIC : 0);
			cf2.wWeight = (WORD)lf.lfWeight;
			cf2.bPitchAndFamily = lf.lfPitchAndFamily;
			cf2.yHeight = abs(lf.lfHeight) * 15;
			SendDlgItemMessageA(hwndDlg, IDC_MESSAGE, EM_SETCHARFORMAT, 0, (LPARAM)&cf2);
		}
		SendMessage(hwndDlg, DM_REMAKELOG, 0, 0);
		SendMessage(hwndDlg, DM_UPDATEWINICON, 0, 0);
		break;
	case DM_UPDATETITLE:
		{
			DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING *) wParam;
			if (dat->hContact) {
				if (dat->szProto) {
					CONTACTINFO ci;
					char buf[128];
					buf[0] = 0;
					ZeroMemory(&ci, sizeof(ci));
					ci.cbSize = sizeof(ci);
					ci.hContact = dat->hContact;
					ci.szProto = dat->szProto;
					ci.dwFlag = CNF_UNIQUEID;
					if (!CallService(MS_CONTACT_GETCONTACTINFO, 0, (LPARAM) & ci)) {
						switch (ci.type) {
						case CNFT_ASCIIZ:
							mir_snprintf(buf, sizeof(buf), Translate("User Menu - %s"), ci.pszVal);
							miranda_sys_free(ci.pszVal);
							break;
						case CNFT_DWORD:
							mir_snprintf(buf, sizeof(buf), Translate("User Menu - %u"), ci.dVal);
							break;
						}
					}
					SendMessage(GetDlgItem(hwndDlg, IDC_USERMENU), BUTTONADDTOOLTIP, (WPARAM) buf, 0);
		//			SetDlgItemTextA(hwndDlg, IDC_NAME, buf[0] ? buf : contactName);
			
					if (!cws || (!strcmp(cws->szModule, dat->szProto) && !strcmp(cws->szSetting, "Status"))) {
						HICON hIcon;
						DWORD dwStatus;
						dwStatus = DBGetContactSettingWord(dat->hContact, dat->szProto, "Status", ID_STATUS_OFFLINE);
						hIcon = LoadSkinnedProtoIcon(dat->szProto, dwStatus);
						SendDlgItemMessage(hwndDlg, IDC_USERMENU, BM_SETIMAGE, IMAGE_ICON, (LPARAM) hIcon);
						SendMessage(hwndDlg, DM_UPDATEWINICON, 0, 0);
					}

					// log status change
					if ((dat->wStatus != dat->wOldStatus || lParam != 0)
						&& DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_SHOWSTATUSCH, SRMSGDEFSET_SHOWSTATUSCH)) {
							DBEVENTINFO dbei;
							char buffer[450];
							HANDLE hNewEvent;
							int iLen;

							char *szOldStatus = (char *) CallService(MS_CLIST_GETSTATUSMODEDESCRIPTION, (WPARAM) dat->wOldStatus, 0);
							char *szNewStatus = (char *) CallService(MS_CLIST_GETSTATUSMODEDESCRIPTION, (WPARAM) dat->wStatus, 0);

							if (dat->wStatus == ID_STATUS_OFFLINE) {
								mir_snprintf(buffer, sizeof(buffer), Translate("signed off (was %s)"), szOldStatus);
							}
							else if (dat->wOldStatus == ID_STATUS_OFFLINE) {
								mir_snprintf(buffer, sizeof(buffer), Translate("signed on (%s)"), szNewStatus);
							}
							else {
								mir_snprintf(buffer, sizeof(buffer), Translate("is now %s (was %s)"), szNewStatus, szOldStatus);
							}
							iLen = strlen(buffer) + 1;
							MultiByteToWideChar(CP_ACP, 0, buffer, iLen, (LPWSTR) & buffer[iLen], iLen);
							dbei.cbSize = sizeof(dbei);
							dbei.pBlob = (PBYTE) buffer;
							dbei.cbBlob = (strlen(buffer) + 1) * (sizeof(TCHAR) + 1);
							dbei.eventType = EVENTTYPE_STATUSCHANGE;
							dbei.flags = 0;
							dbei.timestamp = time(NULL);
							dbei.szModule = dat->szProto;
							hNewEvent = (HANDLE) CallService(MS_DB_EVENT_ADD, (WPARAM) dat->hContact, (LPARAM) & dbei);
							if (dat->hDbEventFirst == NULL) {
								dat->hDbEventFirst = hNewEvent;
								SendMessage(hwndDlg, DM_REMAKELOG, 0, 0);
							}
						}
						dat->wOldStatus = dat->wStatus;
				}
			}
			SendMessage(dat->hwndParent, DM_UPDATETITLE, (WPARAM)hwndDlg, (LPARAM)dat);
			break;
		}
	case DM_SWITCHTOOLBAR:
		SetDialogToType(hwndDlg);
//		SendMessage(dat->hwndParent, DM_SWITCHTOOLBAR, 0, 0);
		break;
	case DM_GETCODEPAGE:
		SetWindowLong(hwndDlg, DWL_MSGRESULT, dat->codePage);
		return TRUE;
	case DM_SETCODEPAGE:
		dat->codePage = (int) lParam;
		SendMessage(hwndDlg, DM_REMAKELOG, 0, 0);
		break;
	case DM_SWITCHUNICODE:
		dat->flags ^= SMF_DISABLE_UNICODE;
		SendMessage(dat->parent->hwndStatus, SB_SETICON, 2, (LPARAM) g_dat->hIcons[(dat->flags & SMF_DISABLE_UNICODE) ? SMF_ICON_UNICODEOFF : SMF_ICON_UNICODEON]);
		SendMessage(hwndDlg, DM_REMAKELOG, 0, 0);
//		SendMessage(dat->hwndParent, DM_SWITCHSTATUSBAR, 0, 0);
		break;
	case DM_SWITCHRTL:
		{
			PARAFORMAT2 pf2;
			ZeroMemory((void *)&pf2, sizeof(pf2));
			pf2.cbSize = sizeof(pf2);
			pf2.dwMask = PFM_RTLPARA;
			dat->flags ^= SMF_RTL;
			SetDlgItemText(hwndDlg, IDC_MESSAGE, _T(""));
			if (dat->flags&SMF_RTL) {
				pf2.wEffects = PFE_RTLPARA;
				SetWindowLong(GetDlgItem(hwndDlg, IDC_MESSAGE),GWL_EXSTYLE,GetWindowLong(GetDlgItem(hwndDlg, IDC_MESSAGE),GWL_EXSTYLE) | WS_EX_RIGHT | WS_EX_RTLREADING | WS_EX_LEFTSCROLLBAR);
				SetWindowLong(GetDlgItem(hwndDlg, IDC_LOG),GWL_EXSTYLE,GetWindowLong(GetDlgItem(hwndDlg, IDC_LOG),GWL_EXSTYLE) | WS_EX_RIGHT | WS_EX_RTLREADING | WS_EX_LEFTSCROLLBAR);
			} else {
				pf2.wEffects = 0;
				SetWindowLong(GetDlgItem(hwndDlg, IDC_MESSAGE),GWL_EXSTYLE,GetWindowLong(GetDlgItem(hwndDlg, IDC_MESSAGE),GWL_EXSTYLE) &~ (WS_EX_RIGHT | WS_EX_RTLREADING | WS_EX_LEFTSCROLLBAR));
				SetWindowLong(GetDlgItem(hwndDlg, IDC_LOG),GWL_EXSTYLE,GetWindowLong(GetDlgItem(hwndDlg, IDC_LOG),GWL_EXSTYLE) &~ (WS_EX_RIGHT | WS_EX_RTLREADING | WS_EX_LEFTSCROLLBAR));
			}
			SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_SETPARAFORMAT, 0, (LPARAM)&pf2);
		}
		SendMessage(hwndDlg, DM_REMAKELOG, 0, 0);
		break;
	case DM_GETWINDOWSTATE:
		{
			UINT state = 0;
			
			state |= MSG_WINDOW_STATE_EXISTS;
			if (IsWindowVisible(hwndDlg)) 
				state |= MSG_WINDOW_STATE_VISIBLE;
			if (GetFocus()==hwndDlg) 
				state |= MSG_WINDOW_STATE_FOCUS;
			if (IsIconic(hwndDlg))
				state |= MSG_WINDOW_STATE_ICONIC;
			SetWindowLong(hwndDlg, DWL_MSGRESULT, state);
			return TRUE;

		}
	case WM_ACTIVATE:
		if (LOWORD(wParam) != WA_ACTIVE)
			break;
		//fall through
	case WM_MOUSEACTIVATE:
		if (KillTimer(hwndDlg, TIMERID_FLASHWND)) {
			dat->showUnread = 0;
//			dat->nFlash = 0;
			SendMessage(hwndDlg, DM_UPDATEWINICON, 0, 0);
		}
		break;
	case WM_LBUTTONDOWN:
		SendMessage(dat->hwndParent, WM_LBUTTONDOWN, wParam, lParam);
		return TRUE;
	case WM_SETFOCUS:
		SendMessage(dat->hwndParent, DM_ACTIVATECHILD, 0, (LPARAM) hwndDlg);
		SetFocus(GetDlgItem(hwndDlg, IDC_MESSAGE));
		return TRUE;
	case WM_GETMINMAXINFO:
		{
			MINMAXINFO *mmi = (MINMAXINFO *) lParam;
			int minBottomHeight = dat->toolbarHeight + dat->minEditBoxHeight;
			if (minBottomHeight < g_dat->limitAvatarMinH) {
			}
			mmi->ptMinTrackSize.x = 240;// + dat->avatarWidth;
			mmi->ptMinTrackSize.y = dat->minLogBoxHeight + minBottomHeight;

			return 0;
		}
	case WM_SIZE:
		{
			if (wParam==SIZE_RESTORED || wParam==SIZE_MAXIMIZED) {
				RECT rc;
				int dlgWidth, dlgHeight;
				dlgWidth = LOWORD(lParam);
				dlgHeight = HIWORD(lParam);
				/*if (dlgWidth == 0 && dlgHeight ==0) */{
					GetClientRect(hwndDlg, &rc);
					dlgWidth = rc.right - rc.left;
					dlgHeight = rc.bottom - rc.top;
				}
				MessageDialogResize(hwndDlg, dat, dlgWidth, dlgHeight);
				if ((g_dat->flags&SMF_AVATAR)&&dat->avatarPic) {
					RedrawWindow(GetDlgItem(hwndDlg, IDC_AVATAR), NULL, NULL, RDW_INVALIDATE);
				}
			}
			return TRUE;
		}
	case DM_SPLITTERMOVED:
		{
			POINT pt;
			RECT rc;
			RECT rcLog;
			if ((HWND) lParam == GetDlgItem(hwndDlg, IDC_SPLITTER)) {
				int oldSplitterY;
				GetWindowRect(GetDlgItem(hwndDlg, IDC_LOG), &rcLog);
				GetClientRect(hwndDlg, &rc);
				pt.x = 0;
				pt.y = wParam;
				ScreenToClient(hwndDlg, &pt);

				oldSplitterY = dat->splitterPos;
				dat->splitterPos = rc.bottom - pt.y;
				GetWindowRect(GetDlgItem(hwndDlg, IDC_MESSAGE), &rc);
			/*
				if (rc.bottom - rc.top + (dat->splitterPos - oldSplitterY) < dat->minEditBoxSize.cy)
					dat->splitterPos = oldSplitterY + dat->minEditBoxSize.cy - (rc.bottom - rc.top);
				if (rcLog.bottom - rcLog.top - (dat->splitterPos - oldSplitterY) < dat->minEditBoxSize.cy)
					dat->splitterPos = oldSplitterY - dat->minEditBoxSize.cy + (rcLog.bottom - rcLog.top);
					*/
				SendMessage(hwndDlg, WM_SIZE, 0, 0);
			}
			break;
		}
	case DM_REMAKELOG:
		StreamInEvents(hwndDlg, dat->hDbEventFirst, -1, 0);
		break;
	case DM_APPENDTOLOG:   //takes wParam=hDbEvent
		StreamInEvents(hwndDlg, (HANDLE) wParam, 1, 1);
		break;
	case DM_SCROLLLOGTOBOTTOM:
		{
			/*
			int	nMin, nMax;
			HWND hwndLog = GetDlgItem(hwndDlg, IDC_LOG);
			GetScrollRange(hwndLog, SB_VERT, &nMin, &nMax);
			SetScrollPos(hwndLog, SB_VERT, nMax, TRUE);
			PostMessage(hwndLog, WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, nMax), (LPARAM) NULL);
			*/
			SCROLLINFO si = { 0 };
			if ((GetWindowLong(GetDlgItem(hwndDlg, IDC_LOG), GWL_STYLE) & WS_VSCROLL) == 0)
				break;
			si.cbSize = sizeof(si);
			si.fMask = SIF_PAGE | SIF_RANGE;
			if (GetScrollInfo(GetDlgItem(hwndDlg, IDC_LOG), SB_VERT, &si)) {
				si.fMask = SIF_POS;
				si.nPos = si.nMax - si.nPage + 1;
				SetScrollInfo(GetDlgItem(hwndDlg, IDC_LOG), SB_VERT, &si, TRUE);
				PostMessage(GetDlgItem(hwndDlg, IDC_LOG), WM_VSCROLL, MAKEWPARAM(SB_BOTTOM, 0), 0);
			}
			
			break;
		}
	case HM_DBEVENTADDED:
		if ((HANDLE) wParam != dat->hContact)
			break;
		{
			DBEVENTINFO dbei = { 0 };

			dbei.cbSize = sizeof(dbei);
			dbei.cbBlob = 0;
			CallService(MS_DB_EVENT_GET, lParam, (LPARAM) & dbei);
			if (dat->hDbEventFirst == NULL)
				dat->hDbEventFirst = (HANDLE) lParam;
			if (dbei.eventType == EVENTTYPE_MESSAGE && (dbei.flags & DBEF_READ))
				break;
			if (DbEventIsShown(&dbei, dat)) {
				if (dbei.eventType == EVENTTYPE_MESSAGE && !(dbei.flags & (DBEF_SENT))) {
					dat->lastMessage = dbei.timestamp;
					SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
					if (GetForegroundWindow()==dat->hwndParent && dat->parent->hwndActive == hwndDlg)
						SkinPlaySound("RecvMsgActive");
					else SkinPlaySound("RecvMsgInactive");
				}
				if ((HANDLE) lParam != dat->hDbEventFirst && (HANDLE) CallService(MS_DB_EVENT_FINDNEXT, lParam, 0) == NULL)
					SendMessage(hwndDlg, DM_APPENDTOLOG, lParam, 0);
				else
					SendMessage(hwndDlg, DM_REMAKELOG, 0, 0);
				if (!(dbei.flags & DBEF_SENT) && dbei.eventType != EVENTTYPE_STATUSCHANGE) {
//					dat->nFlash = dat->nFlashMax;
					SendMessage(dat->hwndParent, DM_STARTFLASHING, 0, 0); 
					if (GetActiveWindow() != dat->hwndParent || GetForegroundWindow() != dat->hwndParent || dat->parent->hwndActive != hwndDlg) {
						dat->showUnread = 0;
						SetTimer(hwndDlg, TIMERID_FLASHWND, TIMEOUT_FLASHWND, NULL);
					}
				}
			}
			break;
		}
	case DM_UPDATESTATUSBAR:
		if (dat->parent->hwndActive == hwndDlg) {
			if (dat->messagesInProgress && (g_dat->flags & SMF_SHOWPROGRESS)) {
				char szBuf[256];
				mir_snprintf(szBuf, sizeof(szBuf), Translate("Sending in progress: %d message(s) left..."), dat->messagesInProgress);
				SendMessageA(dat->parent->hwndStatus, SB_SETTEXTA, 0, (LPARAM) szBuf);
				SendMessage(dat->parent->hwndStatus, SB_SETICON, 0, (LPARAM) g_dat->hIcons[SMF_ICON_DELIVERING]);
			} else if (dat->nTypeSecs) {
				char szBuf[256];
				char *szContactName = (char *) CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM) dat->hContact, 0);
				mir_snprintf(szBuf, sizeof(szBuf), Translate("%s is typing a message..."), szContactName);
				dat->nTypeSecs--;
				SendMessageA(dat->parent->hwndStatus, SB_SETTEXTA, 0, (LPARAM) szBuf);
				SendMessage(dat->parent->hwndStatus, SB_SETICON, 0, (LPARAM) g_dat->hIcons[SMF_ICON_TYPING]);
			} else if (dat->lastMessage) {
				DBTIMETOSTRING dbtts;
				char date[64], time[64], fmt[128];
				dbtts.szFormat = "d";
				dbtts.cbDest = sizeof(date);
				dbtts.szDest = date;
				CallService(MS_DB_TIME_TIMESTAMPTOSTRING, dat->lastMessage, (LPARAM) & dbtts);
				dbtts.szFormat = "t";
				dbtts.cbDest = sizeof(time);
				dbtts.szDest = time;
				CallService(MS_DB_TIME_TIMESTAMPTOSTRING, dat->lastMessage, (LPARAM) & dbtts);
				mir_snprintf(fmt, sizeof(fmt), Translate("Last message received on %s at %s."), date, time);
				SendMessageA(dat->parent->hwndStatus, SB_SETTEXTA, 0, (LPARAM) fmt);
				SendMessage(dat->parent->hwndStatus, SB_SETICON, 0, (LPARAM) NULL);
			} else {
				SendMessageA(dat->parent->hwndStatus, SB_SETTEXTA, 0, (LPARAM) "");
				SendMessage(dat->parent->hwndStatus, SB_SETICON, 0, (LPARAM) NULL);
			}
			SendMessage(dat->parent->hwndStatus, SB_SETICON, 2, (LPARAM) g_dat->hIcons[(dat->flags & SMF_DISABLE_UNICODE) ? SMF_ICON_UNICODEOFF : SMF_ICON_UNICODEON]);
			UpdateReadChars(hwndDlg, dat);
			break;
		}
		break;
	case DM_CLEARLOG:
	// IEVIew MOD Begin
		if (dat->hwndLog != NULL) {
			IEVIEWEVENT event;
			event.cbSize = sizeof(IEVIEWEVENT);
			event.iType = IEE_CLEAR_LOG;
			event.dwFlags = 0;
			event.hwnd = dat->hwndLog;
			event.hContact = dat->hContact;
			CallService(MS_IEVIEW_EVENT, 0, (LPARAM)&event);
		}
	// IEVIew MOD End
		SetDlgItemText(hwndDlg, IDC_LOG, _T(""));
		dat->hDbEventFirst = NULL;
		break;
	case WM_TIMER:
		if (wParam == TIMERID_MSGSEND) {
			int i;
			int timeout = DBGetContactSettingDword(NULL, SRMMMOD, SRMSGSET_MSGTIMEOUT, SRMSGDEFSET_MSGTIMEOUT);
			for (i = 0; i < dat->sendCount; i++) {
				if (dat->sendInfo[i].sendBuffer) {
					if (dat->sendInfo[i].timeout < timeout) {
						dat->sendInfo[i].timeout+=1000;
						if (dat->sendInfo[i].timeout >= timeout) {
							struct ErrorWindowData *ewd = (struct ErrorWindowData *) malloc(sizeof(struct ErrorWindowData));
							ewd->szName = strdup ((char *) CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM) dat->hContact, 0));
							ewd->szDescription = strdup(Translate("The message send timed out."));
							ewd->textSize = dat->sendInfo[i].sendBufferSize;
							ewd->szText = (char *)malloc(dat->sendInfo[i].sendBufferSize);
							memcpy(ewd->szText, dat->sendInfo[i].sendBuffer, dat->sendInfo[i].sendBufferSize);
							ewd->hwndParent = hwndDlg;
							if (dat->messagesInProgress>0) {
								dat->messagesInProgress--;
								if (g_dat->flags & SMF_SHOWPROGRESS) {
									SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
								}
							}
							CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_MSGSENDERROR), hwndDlg, ErrorDlgProc, (LPARAM) ewd);
							//RemoveSendBuffer(dat, i);
						}
					}
				}
			}
		}
		else if (wParam == TIMERID_FLASHWND) {
			dat->showUnread++;
			SendMessage(hwndDlg, DM_UPDATEWINICON, 0, 0);
		}
		else if (wParam == TIMERID_TYPE) {
			if (dat->nTypeMode == PROTOTYPE_SELFTYPING_ON && GetTickCount() - dat->nLastTyping > TIMEOUT_TYPEOFF) {
				NotifyTyping(dat, PROTOTYPE_SELFTYPING_OFF);
			}
			if (dat->showTyping) {
				if (dat->nTypeSecs) {
					dat->nTypeSecs--;
				}
				else {
					dat->showTyping = 0;
					SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
					if (g_dat->flags&SMF_SHOWTYPINGWIN)
						SendMessage(hwndDlg, DM_UPDATEWINICON, 0, 0);
				}
			}
			else {
				if (dat->nTypeSecs) {
					dat->showTyping = 1;
					SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
					SendMessage(hwndDlg, DM_UPDATEWINICON, 0, 0);
				}
			}
		}
		break;
	case DM_SENDMESSAGE:
			if (lParam) {
				HANDLE hSendId;
				struct MessageSendInfo *msi = (struct MessageSendInfo *)lParam;
				dat->sendCount ++;
				dat->sendInfo = (struct MessageSendInfo *) realloc(dat->sendInfo, sizeof(struct MessageSendInfo) * dat->sendCount);
				dat->sendInfo[dat->sendCount-1].sendBufferSize = msi->sendBufferSize;
				dat->sendInfo[dat->sendCount-1].sendBuffer = (char *) malloc(msi->sendBufferSize);
				dat->sendInfo[dat->sendCount-1].timeout=0;
				memcpy(dat->sendInfo[dat->sendCount-1].sendBuffer, msi->sendBuffer, dat->sendInfo[dat->sendCount-1].sendBufferSize);
				SetTimer(hwndDlg, TIMERID_MSGSEND, 1000, NULL);
				dat->messagesInProgress++;
				if (g_dat->flags & SMF_SHOWPROGRESS) {
					SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
				}
				hSendId = (HANDLE) CallContactService(dat->hContact, MsgServiceName(dat->hContact), SEND_FLAGS, (LPARAM) dat->sendInfo[dat->sendCount-1].sendBuffer);
				if (dat->sendCount>0) {
					dat->sendInfo[dat->sendCount-1].hSendId = hSendId;
				}
			}

		break;
	case DM_ERRORDECIDED:
		switch (wParam) {
		case MSGERROR_CANCEL:
			{
				SetFocus(GetDlgItem(hwndDlg, IDC_MESSAGE));
			}
			break;
		case MSGERROR_RETRY:
			if (lParam) {
				struct MessageSendInfo msi;
				struct ErrorWindowData *ewd = (struct ErrorWindowData *)lParam;
				msi.sendBufferSize = ewd->textSize;
				msi.sendBuffer = ewd->szText;
				SendMessage(hwndDlg, DM_SENDMESSAGE, 0, (LPARAM)&msi);
				/*
				HANDLE hSendId;
				struct ErrorWindowData *ewd = (struct ErrorWindowData *)lParam;
				dat->sendCount ++;
				dat->sendInfo = (struct MessageSendInfo *) realloc(dat->sendInfo, sizeof(struct MessageSendInfo) * dat->sendCount);
				dat->sendInfo[dat->sendCount-1].sendBufferSize = ewd->textSize;
				dat->sendInfo[dat->sendCount-1].sendBuffer = (char *) malloc(ewd->textSize);
				dat->sendInfo[dat->sendCount-1].timeout=0;
				memcpy(dat->sendInfo[dat->sendCount-1].sendBuffer, ewd->szText, dat->sendInfo[dat->sendCount-1].sendBufferSize);
				SetTimer(hwndDlg, TIMERID_MSGSEND, 1000, NULL);
				dat->messagesInProgress++;
				if (g_dat->flags & SMF_SHOWPROGRESS) {
					SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
				}
				hSendId = (HANDLE) CallContactService(dat->hContact, MsgServiceName(dat->hContact), SEND_FLAGS, (LPARAM) dat->sendInfo[dat->sendCount-1].sendBuffer);
				if (dat->sendCount>0) {
					dat->sendInfo[dat->sendCount-1].hSendId = hSendId;
				}
				*/
			}
			break;
		}
		break;
	case WM_MEASUREITEM:
		return CallService(MS_CLIST_MENUMEASUREITEM, wParam, lParam);
	case WM_DRAWITEM:
			{
				LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT) lParam;
				if (dis->hwndItem == GetDlgItem(hwndDlg, IDC_AVATAR) && dat->avatarPic && (g_dat->flags&SMF_AVATAR)) {
					BITMAP bminfo;
					HPEN hPen;

					GetObject(dat->avatarPic, sizeof(bminfo), &bminfo);
					{
						HDC hdcMem = CreateCompatibleDC(dis->hDC);
                        HBITMAP hbmMem = (HBITMAP)SelectObject(hdcMem, dat->avatarPic);
						{
							SetStretchBltMode(dis->hDC, HALFTONE);
                            StretchBlt(dis->hDC, 1, 1, dat->avatarWidth-2, dat->avatarHeight-2, hdcMem, 0, 0, bminfo.bmWidth, bminfo.bmHeight, SRCCOPY);
						}
						DeleteObject(hbmMem);
                        DeleteDC(hdcMem);
					}
                    hPen = CreatePen(PS_SOLID, 1, RGB(0,0,0));
                    SelectObject(dis->hDC, hPen);
					ExcludeClipRect(dis->hDC, 1, 1, dat->avatarWidth-1, dat->avatarHeight-1);
                    Rectangle(dis->hDC, 0, 0, dat->avatarWidth, dat->avatarHeight);
                    DeleteObject(hPen);
				}
				return CallService(MS_CLIST_MENUDRAWITEM, wParam, lParam);
			}
	case WM_COMMAND:
		if (CallService(MS_CLIST_MENUPROCESSCOMMAND, MAKEWPARAM(LOWORD(wParam), MPCF_CONTACTMENU), (LPARAM) dat->hContact))
			break;
		switch (LOWORD(wParam)) {
		case IDC_SENDALL:
		case IDOK:
			//this is a 'send' button
			if (!IsWindowEnabled(GetDlgItem(hwndDlg, IDOK)))
				break;
			//if(GetKeyState(VK_CTRL) & 0x8000) {    // copy user name
					//SendMessage(hwndDlg, DM_USERNAMETOCLIP, 0, 0);
			//}
			if (dat->hContact !=NULL) {
				struct MessageSendInfo msi;
			//	HANDLE hSendId;
				int bufSize;

				bufSize = GetWindowTextLengthA(GetDlgItem(hwndDlg, IDC_MESSAGE)) + 1;
				msi.sendBufferSize = bufSize * (sizeof(TCHAR) + 1);
				msi.sendBuffer = (char *) malloc(msi.sendBufferSize);
				GetDlgItemTextA(hwndDlg, IDC_MESSAGE, msi.sendBuffer, bufSize);
		#if defined( _UNICODE )
				{
					GETTEXTEX  gt;
					gt.cb = bufSize * sizeof(TCHAR);
					gt.flags = GT_USECRLF;
					gt.codepage = 1200;
					SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_GETTEXTEX, (WPARAM) &gt, (LPARAM) &msi.sendBuffer[bufSize]);
				}
		#endif
				if (msi.sendBuffer[0] == 0)
					break;
		#if defined( _UNICODE )
				dat->cmdList = tcmdlist_append(dat->cmdList, (TCHAR *) &msi.sendBuffer[bufSize]);
		#else
				dat->cmdList = tcmdlist_append(dat->cmdList, msi.sendBuffer);
		#endif
				dat->cmdListCurrent = 0;
				if (dat->nTypeMode == PROTOTYPE_SELFTYPING_ON) {
					NotifyTyping(dat, PROTOTYPE_SELFTYPING_OFF);
				}
				SetDlgItemText(hwndDlg, IDC_MESSAGE, _T(""));
				EnableWindow(GetDlgItem(hwndDlg, IDOK), FALSE);
				if (DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_AUTOMIN, SRMSGDEFSET_AUTOMIN))
					ShowWindow(dat->hwndParent, SW_MINIMIZE);
				if (LOWORD(wParam) == IDC_SENDALL) {
					SendMessage(dat->hwndParent, DM_SENDMESSAGE, 0, (LPARAM) &msi);
				} else {
					SendMessage(hwndDlg, DM_SENDMESSAGE, 0, (LPARAM) &msi);
				}
				free (msi.sendBuffer);
			}
			return TRUE;
		case IDCANCEL:
			DestroyWindow(hwndDlg);
			return TRUE;
		case IDC_USERMENU:
			{
				if(GetKeyState(VK_SHIFT) & 0x8000) {    // copy user name
						SendMessage(hwndDlg, DM_USERNAMETOCLIP, 0, 0);
				}
				else {
					RECT rc;
					HMENU hMenu = (HMENU) CallService(MS_CLIST_MENUBUILDCONTACT, (WPARAM) dat->hContact, 0);
					GetWindowRect(GetDlgItem(hwndDlg, LOWORD(wParam)), &rc);
					TrackPopupMenu(hMenu, 0, rc.left, rc.bottom, 0, hwndDlg, NULL);
					DestroyMenu(hMenu);
				}
			}
			break;
		case IDC_HISTORY:
			CallService(MS_HISTORY_SHOWCONTACTHISTORY, (WPARAM) dat->hContact, 0);
			break;
		case IDC_DETAILS:
			CallService(MS_USERINFO_SHOWDIALOG, (WPARAM) dat->hContact, 0);
			break;
		case IDC_SMILEYS:
			{
				IEVIEWSHOWSMILEYSEL smaddInfo;
				RECT rc;
				smaddInfo.cbSize = sizeof(IEVIEWSHOWSMILEYSEL);
				smaddInfo.hwndTarget = GetDlgItem(hwndDlg, IDC_MESSAGE);
				smaddInfo.targetMessage = EM_REPLACESEL;
				smaddInfo.targetWParam = TRUE;
				smaddInfo.Protocolname = dat->szProto;
				GetWindowRect(GetDlgItem(hwndDlg, IDC_SMILEYS), &rc);
				smaddInfo.Direction = 0;
				smaddInfo.xPosition = rc.left;
				smaddInfo.yPosition = rc.top + 24;
				if (dat->hwndLog != NULL) {
					CallService(MS_IEVIEW_SHOWSMILEYSELECTION, 0, (LPARAM) &smaddInfo);
				} else if (ServiceExists(MS_SMILEYADD_SHOWSELECTION)) {
					CallService(MS_SMILEYADD_SHOWSELECTION, 0, (LPARAM) &smaddInfo);
				}
			}
			break;
		case IDC_QUOTE:
			{
				DBEVENTINFO dbei = { 0 };
				SETTEXTEX  st;
				st.flags = ST_SELECTION;
#ifdef _UNICODE
				st.codepage = 1200;
#else
				st.codepage = CP_ACP;
#endif
				if (dat->hDbEventLast==NULL) break;
				if (dat->hwndLog != NULL) {
					TCHAR *buffer = GetIEViewSelection(dat);
					if (buffer!=NULL) {
						TCHAR *quotedBuffer = GetQuotedTextW(buffer);
						SendMessage(GetDlgItem(hwndDlg, IDC_MESSAGE), EM_SETTEXTEX, (WPARAM) &st, (LPARAM)quotedBuffer);
						free (quotedBuffer);
						SetFocus(GetDlgItem(hwndDlg, IDC_MESSAGE));
						break;
					}
				} else {
					TCHAR *buffer = GetRichEditSelection(hwndDlg);
					if (buffer!=NULL) {
						TCHAR *quotedBuffer = GetQuotedTextW(buffer);
						SendMessage(GetDlgItem(hwndDlg, IDC_MESSAGE), EM_SETTEXTEX, (WPARAM) &st, (LPARAM)quotedBuffer);
						free (quotedBuffer);
						free(buffer);
						SetFocus(GetDlgItem(hwndDlg, IDC_MESSAGE));
						break;
					}

				}
				dbei.cbSize = sizeof(dbei);
				dbei.cbBlob = CallService(MS_DB_EVENT_GETBLOBSIZE, (WPARAM) dat->hDbEventLast, 0);
				if (dbei.cbBlob == 0xFFFFFFFF) break;
				dbei.pBlob = (PBYTE) malloc(dbei.cbBlob);
				CallService(MS_DB_EVENT_GET, (WPARAM)  dat->hDbEventLast, (LPARAM) & dbei);
				if (dbei.eventType == EVENTTYPE_MESSAGE || dbei.eventType == EVENTTYPE_STATUSCHANGE) {
					TCHAR *buffer = NULL;
#ifdef _UNICODE
					DWORD aLen = strlen((char *)dbei.pBlob)+1;
					if (dbei.eventType == EVENTTYPE_MESSAGE) {
						if (dbei.cbBlob > aLen) {
							DWORD wlen = safe_wcslen((wchar_t *)&dbei.pBlob[aLen], (dbei.cbBlob - aLen) / 2);
							if (wlen > 0 && wlen < aLen) {
								buffer = (TCHAR *)&dbei.pBlob[aLen];
							}
						}
					}
					if (buffer == NULL) {
						buffer = (TCHAR *) malloc(sizeof(TCHAR) * aLen);
						MultiByteToWideChar(CP_ACP, 0, (char *) dbei.pBlob, -1, buffer, aLen);
						free(dbei.pBlob);
						dbei.pBlob = (char *)buffer;
					}
#else
					buffer = (TCHAR *)dbei.pBlob;
#endif
					if (buffer!=NULL) {
						TCHAR *quotedBuffer = GetQuotedTextW(buffer);
						SendMessage(GetDlgItem(hwndDlg, IDC_MESSAGE), EM_SETTEXTEX, (WPARAM) &st, (LPARAM)quotedBuffer);
						free (quotedBuffer);
					}
				}
				free(dbei.pBlob);
				SetFocus(GetDlgItem(hwndDlg, IDC_MESSAGE));
				break;
			}
		case IDC_ADD:
			{
				ADDCONTACTSTRUCT acs = { 0 };

				acs.handle = dat->hContact;
				acs.handleType = HANDLE_CONTACT;
				acs.szProto = 0;
				CallService(MS_ADDCONTACT_SHOW, (WPARAM) hwndDlg, (LPARAM) & acs);
			}
			if (!DBGetContactSettingByte(dat->hContact, "CList", "NotOnList", 0)) {
				ShowWindow(GetDlgItem(hwndDlg, IDC_ADD), FALSE);
			}
		case IDC_MESSAGE:
			if (HIWORD(wParam) == EN_CHANGE) {
				int len = GetWindowTextLength(GetDlgItem(hwndDlg, IDC_MESSAGE));
				UpdateReadChars(hwndDlg, dat);
				EnableWindow(GetDlgItem(hwndDlg, IDOK), len != 0);
				if (!(GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000)) {
					dat->nLastTyping = GetTickCount();
					if (GetWindowTextLength(GetDlgItem(hwndDlg, IDC_MESSAGE))) {
						if (dat->nTypeMode == PROTOTYPE_SELFTYPING_OFF) {
							NotifyTyping(dat, PROTOTYPE_SELFTYPING_ON);
						}
					}
					else {
						if (dat->nTypeMode == PROTOTYPE_SELFTYPING_ON) {
							NotifyTyping(dat, PROTOTYPE_SELFTYPING_OFF);
						}
					}
				}
			}
			break;
		}
		break;
	case WM_NOTIFY:
		switch (((NMHDR *) lParam)->idFrom) {
		case IDC_LOG:
			switch (((NMHDR *) lParam)->code) {
			case EN_MSGFILTER:
				switch (((MSGFILTER *) lParam)->msg) {
				case WM_LBUTTONDOWN:
					{
						HCURSOR hCur = GetCursor();
						if (hCur == LoadCursor(NULL, IDC_SIZENS) || hCur == LoadCursor(NULL, IDC_SIZEWE)
							|| hCur == LoadCursor(NULL, IDC_SIZENESW) || hCur == LoadCursor(NULL, IDC_SIZENWSE)) {
								SetWindowLong(hwndDlg, DWL_MSGRESULT, TRUE);
								return TRUE;
							}
							break;
					}
				case WM_MOUSEMOVE:
					{
						HCURSOR hCur = GetCursor();
						if (hCur == LoadCursor(NULL, IDC_SIZENS) || hCur == LoadCursor(NULL, IDC_SIZEWE)
							|| hCur == LoadCursor(NULL, IDC_SIZENESW) || hCur == LoadCursor(NULL, IDC_SIZENWSE))
							SetCursor(LoadCursor(NULL, IDC_ARROW));
						break;
					}
				case WM_RBUTTONUP:
					{
						HMENU hMenu, hSubMenu;
						POINT pt;
						CHARRANGE sel, all = { 0, -1 };

						hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_CONTEXT));
						hSubMenu = GetSubMenu(hMenu, 0);
						CallService(MS_LANGPACK_TRANSLATEMENU, (WPARAM) hSubMenu, 0);
						SendMessage(((NMHDR *) lParam)->hwndFrom, EM_EXGETSEL, 0, (LPARAM) & sel);
						if (sel.cpMin == sel.cpMax)
							EnableMenuItem(hSubMenu, IDM_COPY, MF_BYCOMMAND | MF_GRAYED);
						pt.x = (short) LOWORD(((ENLINK *) lParam)->lParam);
						pt.y = (short) HIWORD(((ENLINK *) lParam)->lParam);
						ClientToScreen(((NMHDR *) lParam)->hwndFrom, &pt);
						switch (TrackPopupMenu(hSubMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hwndDlg, NULL)) {
						case IDM_COPY:
							SendMessage(((NMHDR *) lParam)->hwndFrom, WM_COPY, 0, 0);
							break;
						case IDM_COPYALL:
							SendMessage(((NMHDR *) lParam)->hwndFrom, EM_EXSETSEL, 0, (LPARAM) & all);
							SendMessage(((NMHDR *) lParam)->hwndFrom, WM_COPY, 0, 0);
							SendMessage(((NMHDR *) lParam)->hwndFrom, EM_EXSETSEL, 0, (LPARAM) & sel);
							break;
						case IDM_SELECTALL:
							SendMessage(((NMHDR *) lParam)->hwndFrom, EM_EXSETSEL, 0, (LPARAM) & all);
							break;
						case IDM_CLEAR:
							SendMessage(hwndDlg, DM_CLEARLOG, 0, 0);
						}
						DestroyMenu(hMenu);
						SetWindowLong(hwndDlg, DWL_MSGRESULT, TRUE);
						return TRUE;
					}
				}
				break;
			case EN_LINK:
				switch (((ENLINK *) lParam)->msg) {
				case WM_SETCURSOR:
					SetCursor(hCurHyperlinkHand);
					SetWindowLong(hwndDlg, DWL_MSGRESULT, TRUE);
					return TRUE;
				case WM_RBUTTONDOWN:
				case WM_LBUTTONUP:
					{
						TEXTRANGEA tr;
						CHARRANGE sel;

						SendDlgItemMessage(hwndDlg, IDC_LOG, EM_EXGETSEL, 0, (LPARAM) & sel);
						if (sel.cpMin != sel.cpMax)
							break;
						tr.chrg = ((ENLINK *) lParam)->chrg;
						tr.lpstrText = malloc(tr.chrg.cpMax - tr.chrg.cpMin + 8);
						SendDlgItemMessageA(hwndDlg, IDC_LOG, EM_GETTEXTRANGE, 0, (LPARAM) & tr);
						if (strchr(tr.lpstrText, '@') != NULL && strchr(tr.lpstrText, ':') == NULL && strchr(tr.lpstrText, '/') == NULL) {
							MoveMemory(tr.lpstrText + 7, tr.lpstrText, tr.chrg.cpMax - tr.chrg.cpMin + 1);
							CopyMemory(tr.lpstrText, "mailto:", 7);
						}
						if (((ENLINK *) lParam)->msg == WM_RBUTTONDOWN) {
							HMENU hMenu, hSubMenu;
							POINT pt;

							hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_CONTEXT));
							hSubMenu = GetSubMenu(hMenu, 1);
							CallService(MS_LANGPACK_TRANSLATEMENU, (WPARAM) hSubMenu, 0);
							pt.x = (short) LOWORD(((ENLINK *) lParam)->lParam);
							pt.y = (short) HIWORD(((ENLINK *) lParam)->lParam);
							ClientToScreen(((NMHDR *) lParam)->hwndFrom, &pt);
							switch (TrackPopupMenu(hSubMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hwndDlg, NULL)) {
							case IDM_OPENNEW:
								CallService(MS_UTILS_OPENURL, 1, (LPARAM) tr.lpstrText);
								break;
							case IDM_OPENEXISTING:
								CallService(MS_UTILS_OPENURL, 0, (LPARAM) tr.lpstrText);
								break;
							case IDM_COPYLINK:
								{
									HGLOBAL hData;
									if (!OpenClipboard(hwndDlg))
										break;
									EmptyClipboard();
									hData = GlobalAlloc(GMEM_MOVEABLE, lstrlenA(tr.lpstrText) + 1);
									lstrcpyA(GlobalLock(hData), tr.lpstrText);
									GlobalUnlock(hData);
									SetClipboardData(CF_TEXT, hData);
									CloseClipboard();
									break;
								}
							}
							free(tr.lpstrText);
							DestroyMenu(hMenu);
							SetWindowLong(hwndDlg, DWL_MSGRESULT, TRUE);
							return TRUE;
						}
						else {
							CallService(MS_UTILS_OPENURL, 1, (LPARAM) tr.lpstrText);
							SetFocus(GetDlgItem(hwndDlg, IDC_MESSAGE));
						}

						free(tr.lpstrText);
						break;
					}
				}
				break;
			}
			break;
		case IDC_MESSAGE:
			switch (((NMHDR *) lParam)->code) {
			case EN_MSGFILTER:
				switch (((MSGFILTER *) lParam)->msg) {
				case WM_RBUTTONUP:
					{
						HMENU hMenu, hSubMenu;
						POINT pt;
						CHARRANGE sel, all = { 0, -1 };

						hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_CONTEXT));
						hSubMenu = GetSubMenu(hMenu, 2);
						CallService(MS_LANGPACK_TRANSLATEMENU, (WPARAM) hSubMenu, 0);
						SendMessage(((NMHDR *) lParam)->hwndFrom, EM_EXGETSEL, 0, (LPARAM) & sel);
						if (sel.cpMin == sel.cpMax) {
							EnableMenuItem(hSubMenu, IDM_CUT, MF_BYCOMMAND | MF_GRAYED);
							EnableMenuItem(hSubMenu, IDM_COPY, MF_BYCOMMAND | MF_GRAYED);
							EnableMenuItem(hSubMenu, IDM_DELETE, MF_BYCOMMAND | MF_GRAYED);
						}
						if (!SendMessage(((NMHDR *) lParam)->hwndFrom, EM_CANUNDO, 0, 0)) {
							EnableMenuItem(hSubMenu, IDM_UNDO, MF_BYCOMMAND | MF_GRAYED);
						}
						if (!SendMessage(((NMHDR *) lParam)->hwndFrom, EM_CANREDO, 0, 0)) {
							EnableMenuItem(hSubMenu, IDM_REDO, MF_BYCOMMAND | MF_GRAYED);
						}
						if (!SendMessage(((NMHDR *) lParam)->hwndFrom, EM_CANPASTE, 0, 0)) {
							EnableMenuItem(hSubMenu, IDM_PASTE, MF_BYCOMMAND | MF_GRAYED);
						}
						pt.x = (short) LOWORD(((ENLINK *) lParam)->lParam);
						pt.y = (short) HIWORD(((ENLINK *) lParam)->lParam);
						ClientToScreen(((NMHDR *) lParam)->hwndFrom, &pt);
						switch (TrackPopupMenu(hSubMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hwndDlg, NULL)) {
						case IDM_UNDO:
							SendMessage(((NMHDR *) lParam)->hwndFrom, WM_UNDO, 0, 0);
							break;
						case IDM_REDO:
							SendMessage(((NMHDR *) lParam)->hwndFrom, EM_REDO, 0, 0);
							break;
						case IDM_CUT:
							SendMessage(((NMHDR *) lParam)->hwndFrom, WM_CUT, 0, 0);
							break;
						case IDM_COPY:
							SendMessage(((NMHDR *) lParam)->hwndFrom, WM_COPY, 0, 0);
							break;
						case IDM_PASTE:
							SendMessage(((NMHDR *) lParam)->hwndFrom, EM_PASTESPECIAL, CF_TEXT, 0);
							break;
						case IDM_DELETE:
							SendMessage(((NMHDR *) lParam)->hwndFrom, EM_REPLACESEL, TRUE, 0);
							break;
						case IDM_SELECTALL:
							SendMessage(((NMHDR *) lParam)->hwndFrom, EM_EXSETSEL, 0, (LPARAM) & all);
							break;
						}
						DestroyMenu(hMenu);
						SetWindowLong(hwndDlg, DWL_MSGRESULT, TRUE);
						return TRUE;
					}
				}
				break;
			}
			break;
		}
		break;
	case HM_EVENTSENT:
		{
			ACKDATA *ack = (ACKDATA *) lParam;
			DBEVENTINFO dbei = { 0 };
			HANDLE hNewEvent;
			int i;

			if (ack->type != ACKTYPE_MESSAGE)
				break;
			if (dat->sendCount==0)
				break;
			for (i = 0; i < dat->sendCount; i++) {
				if (ack->hProcess == dat->sendInfo[i].hSendId && ack->hContact == dat->hContact)
					break;
			}
			if (dat->messagesInProgress>0) {
				dat->messagesInProgress--;
				if (g_dat->flags & SMF_SHOWPROGRESS) {
					SendMessage(hwndDlg, DM_UPDATESTATUSBAR, 0, 0);
				}
			}
			if (ack->result == ACKRESULT_FAILED) {
				if (i == dat->sendCount) {
					for (i = 0; i < dat->sendCount; i++) {
						if (dat->sendInfo[i].sendBuffer) {
							break;
						}
					}
				}
				if (i < dat->sendCount) {
					struct ErrorWindowData *ewd = (struct ErrorWindowData *) malloc(sizeof(struct ErrorWindowData));
					ewd->szName = strdup ((char *) CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM) dat->hContact, 0));
					ewd->szDescription = strdup((char *) ack->lParam);
					ewd->textSize = dat->sendInfo[i].sendBufferSize;
					ewd->szText = (char *)malloc(dat->sendInfo[i].sendBufferSize);
					memcpy(ewd->szText, dat->sendInfo[i].sendBuffer, dat->sendInfo[i].sendBufferSize);
					ewd->hwndParent = hwndDlg;
					CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_MSGSENDERROR), hwndDlg, ErrorDlgProc, (LPARAM) ewd);//hwndDlg
					RemoveSendBuffer(dat, i);
				}
				return 0;
			}
			if (i == dat->sendCount)
				break;
			dbei.cbSize = sizeof(dbei);
			dbei.eventType = EVENTTYPE_MESSAGE;
			dbei.flags = DBEF_SENT;
			dbei.szModule = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) dat->hContact, 0);
			dbei.timestamp = time(NULL);
			dbei.cbBlob = lstrlenA(dat->sendInfo[i].sendBuffer) + 1;
	#if defined( _UNICODE )
			dbei.cbBlob *= sizeof(TCHAR) + 1;
	#endif
			dbei.pBlob = (PBYTE) dat->sendInfo[i].sendBuffer;
			hNewEvent = (HANDLE) CallService(MS_DB_EVENT_ADD, (WPARAM) dat->hContact, (LPARAM) & dbei);
			SkinPlaySound("SendMsg");
			if (dat->hDbEventFirst == NULL) {
				dat->hDbEventFirst = hNewEvent;
				SendMessage(hwndDlg, DM_REMAKELOG, 0, 0);
			}
			RemoveSendBuffer(dat, i);
			if (dat->sendCount == 0) {
				if (DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_AUTOCLOSE, SRMSGDEFSET_AUTOCLOSE))
					DestroyWindow(hwndDlg);
			}
			break;
		}
	case WM_DESTROY:
		NotifyLocalWinEvent(dat->hContact, hwndDlg, MSG_WINDOW_EVT_CLOSING);
		if (dat->nTypeMode == PROTOTYPE_SELFTYPING_ON) {
			NotifyTyping(dat, PROTOTYPE_SELFTYPING_OFF);
		}
		if (dat->sendInfo) {
			int i;
			for (i = 0; i < dat->sendCount; i++) {
				RemoveSendBuffer(dat, i);
			}
			free(dat->sendInfo);
		}
//		if (dat->sendBuffer != NULL)
//			free(dat->sendBuffer);
		if (dat->hwndStatus)
			DestroyWindow(dat->hwndStatus);
		tcmdlist_free(dat->cmdList);
		WindowList_Remove(g_dat->hMessageWindowList, hwndDlg);
		//if (!(g_dat->flags&SMF_AVATAR)||!dat->avatarPic)
		SetWindowLong(GetDlgItem(hwndDlg, IDC_SPLITTER), GWL_WNDPROC, (LONG) OldSplitterProc);
		SendDlgItemMessage(hwndDlg, IDC_MESSAGE, EM_UNSUBCLASSED, 0, 0);
		SetWindowLong(GetDlgItem(hwndDlg, IDC_MESSAGE), GWL_WNDPROC, (LONG) OldMessageEditProc);
		{
			HFONT hFont;
			hFont = (HFONT) SendDlgItemMessage(hwndDlg, IDC_MESSAGE, WM_GETFONT, 0, 0);
			if (hFont != NULL && hFont != (HFONT) SendDlgItemMessage(hwndDlg, IDOK, WM_GETFONT, 0, 0))
				DeleteObject(hFont);
		}
		DBWriteContactSettingByte(dat->hContact, SRMMMOD, "UseRTL", (BYTE) ((dat->flags & SMF_RTL) ? 1 : 0));
		DBWriteContactSettingByte(dat->hContact, SRMMMOD, "DisableUnicode", (BYTE) ((dat->flags & SMF_DISABLE_UNICODE) ? 1 : 0));
		DBWriteContactSettingWord(dat->hContact, SRMMMOD, "CodePage", (WORD) dat->codePage);
		{
			HANDLE hContact;
			if (DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_SAVESPLITTERPERCONTACT, SRMSGDEFSET_SAVESPLITTERPERCONTACT))
				hContact = dat->hContact;
			else
				hContact = NULL;
			DBWriteContactSettingDword(hContact, SRMMMOD, "splitterPos", dat->splitterPos);
		}
		if (dat->avatarPic)
			DeleteObject(dat->avatarPic);
		NotifyLocalWinEvent(dat->hContact, hwndDlg, MSG_WINDOW_EVT_CLOSE);
		if (dat->hContact&&DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_DELTEMP, SRMSGDEFSET_DELTEMP)) {
			if (DBGetContactSettingByte(dat->hContact, "CList", "NotOnList", 0)) {
				CallService(MS_DB_CONTACT_DELETE, (WPARAM)dat->hContact, 0);
			}
		}

// IEVIew MOD Begin
		if (dat->hwndLog != NULL) {
			IEVIEWWINDOW ieWindow;
			ieWindow.cbSize = sizeof(IEVIEWWINDOW);
			ieWindow.iType = IEW_DESTROY;
			ieWindow.hwnd = dat->hwndLog;
			CallService(MS_IEVIEW_WINDOW, 0, (LPARAM)&ieWindow);
		}
// IEVIew MOD End
		SetWindowLong(hwndDlg, GWL_USERDATA, 0);
		SendMessage(dat->hwndParent, DM_REMOVECHILD, 0, (LPARAM) hwndDlg);
		free(dat);
		break;
	}
	return FALSE;
}
