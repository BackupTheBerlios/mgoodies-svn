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
#pragma hdrstop
#include <ctype.h>
#include <malloc.h>
#include <mbstring.h>

#define MIRANDA_0_5

#define LOGICON_MSG_IN      0
#define LOGICON_MSG_OUT     1
#define LOGICON_MSG_NOTICE  2

extern HINSTANCE g_hInst;
static int logPixelSY;
static PBYTE pLogIconBmpBits[3];
static int logIconBmpSize[sizeof(pLogIconBmpBits) / sizeof(pLogIconBmpBits[0])];
static HIMAGELIST g_hImageList;

#define STREAMSTAGE_HEADER  0
#define STREAMSTAGE_EVENTS  1
#define STREAMSTAGE_TAIL    2
#define STREAMSTAGE_STOP    3
struct LogStreamData {
	int stage;
	HANDLE hContact;
	HANDLE hDbEvent, hDbEventLast;
	char *buffer;
	int bufferOffset, bufferLen;
	int eventsToInsert;
	int isFirst;
	int isEmpty;
	struct MessageWindowData *dlgDat;
};

struct EventData {
	int cbSize;
	DWORD timestamp;
	DWORD flags;
	DWORD eventType;
	HANDLE hContact;
	char *text;
	wchar_t* wtext;
	char *szModule;
};


int DbEventIsShown(DBEVENTINFO * dbei, struct MessageWindowData *dat)
{
	switch (dbei->eventType) {
		case EVENTTYPE_MESSAGE:
			return 1;
		case EVENTTYPE_STATUSCHANGE:
			if (dbei->flags & DBEF_READ)
				return 0;
			return 1;
		case EVENTTYPE_FILE:
		case EVENTTYPE_URL:
//			if (dat->hwndLog != NULL)
				return 1;
	}
	return 0;
}

TCHAR *strToWcs(const char *text, int textlen, int cp) {
#if defined ( _UNICODE )
	wchar_t *wtext;
	if (textlen == -1) {
		textlen = strlen(text) + 1;
	}
	wtext = (wchar_t *) malloc(sizeof(wchar_t) * textlen);
	MultiByteToWideChar(cp, 0, text, -1, wtext, textlen);
	return wtext;
#else
	return _tcsdup(text);
#endif
}

struct EventData *getEventFromDB(struct MessageWindowData *dat, HANDLE hContact, HANDLE hDbEvent) {
	DBEVENTINFO dbei = { 0 };
	struct EventData *event;
	dbei.cbSize = sizeof(dbei);
	dbei.cbBlob = CallService(MS_DB_EVENT_GETBLOBSIZE, (WPARAM) hDbEvent, 0);
	if (dbei.cbBlob == -1) return NULL;
	dbei.pBlob = (PBYTE) malloc(dbei.cbBlob);
	CallService(MS_DB_EVENT_GET, (WPARAM) hDbEvent, (LPARAM) & dbei);
	if (!DbEventIsShown(&dbei, dat)) {
		free(dbei.pBlob);
		return NULL;
	}
	if (!(dbei.flags & DBEF_SENT) && (dbei.eventType == EVENTTYPE_MESSAGE || dbei.eventType == EVENTTYPE_URL)) {
		CallService(MS_DB_EVENT_MARKREAD, (WPARAM) hContact, (LPARAM) hDbEvent);
		CallService(MS_CLIST_REMOVEEVENT, (WPARAM) hContact, (LPARAM) hDbEvent);
	}
	else if (dbei.eventType == EVENTTYPE_STATUSCHANGE) {
		CallService(MS_DB_EVENT_MARKREAD, (WPARAM) hContact, (LPARAM) hDbEvent);
	}
	event = (struct EventData *) malloc(sizeof(struct EventData));
	memset(event, 0, sizeof(struct EventData));
	event->hContact = hContact;
	event->eventType = dbei.eventType;
	event->szModule = strdup(dbei.szModule);
	event->flags = dbei.flags;
	event->timestamp = dbei.timestamp;
#if defined( _UNICODE )
	if (event->eventType == EVENTTYPE_FILE) {
		event->text = strdup(((char *) dbei.pBlob) + sizeof(DWORD));
	} else if (event->eventType == EVENTTYPE_MESSAGE) {
		int msglen = strlen((char *) dbei.pBlob) + 1;
		event->text = strdup((char *) dbei.pBlob);
		if (msglen != (int) dbei.cbBlob && !(dat->flags & SMF_DISABLE_UNICODE)) {
			int wlen;
			wlen = safe_wcslen((wchar_t*) &dbei.pBlob[msglen], (dbei.cbBlob - msglen) / 2);
			if (wlen > 0 && wlen < msglen) {
				event->wtext = wcsdup((wchar_t*) &dbei.pBlob[msglen]);
			} else {
				event->wtext = strToWcs((char *) dbei.pBlob, msglen, dat->codePage);
			}
		} else {
			event->wtext = strToWcs((char *) dbei.pBlob, msglen, dat->codePage);
		}
	} else {
		event->text = strdup((char *) dbei.pBlob);
	}
#else
	if (event->eventType == EVENTTYPE_FILE) {
		event->text = strdup(((char *) dbei.pBlob) + sizeof(DWORD));
	} else {
		event->text = strdup((char *) dbei.pBlob);
	}
#endif
	free(dbei.pBlob);
	return event;
}

static void freeEvent(struct EventData *event) {
	if (event->text != NULL) free (event->text);
	if (event->wtext != NULL) free (event->wtext);
	if (event->szModule != NULL) free (event->szModule);
	free(event);
}

int safe_wcslen(wchar_t *msg, int maxLen) {
    int i;
	for (i = 0; i < maxLen; i++) {
		if (msg[i] == (wchar_t)0)
			return i;
	}
	return 0;
}


static int mimFlags = 0;

enum MIMFLAGS {
	MIM_CHECKED = 1,
	MIM_UNICODE = 2
};


static int IsUnicodeMIM() {
	if (!(mimFlags & MIM_CHECKED)) {
		char str[512];
		mimFlags = MIM_CHECKED;
		CallService(MS_SYSTEM_GETVERSIONTEXT, (WPARAM)500, (LPARAM)(char*)str);
		if(strstr(str, "Unicode")) {
			mimFlags |= MIM_UNICODE;
		}
	}
	return (mimFlags & MIM_UNICODE) != 0;
}

TCHAR *GetNickname(HANDLE hContact, const char* szProto) {
	char * szBaseNick;
	TCHAR *szName;
	CONTACTINFO ci;
	ZeroMemory(&ci, sizeof(ci));
	ci.cbSize = sizeof(ci);
	ci.hContact = hContact;
    ci.szProto = (char *)szProto;
	ci.dwFlag = CNF_DISPLAY;
#if defined ( _UNICODE )
	if(IsUnicodeMIM()) {
		ci.dwFlag |= CNF_UNICODE;
    }
#endif
	if (!CallService(MS_CONTACT_GETCONTACTINFO, 0, (LPARAM) & ci)) {
		if (ci.type == CNFT_ASCIIZ) {
			if (ci.pszVal) {
#if defined ( _UNICODE )
				if(IsUnicodeMIM()) {
					szName = _tcsdup((TCHAR *)ci.pszVal);
				} else {
					szName = strToWcs((char *)ci.pszVal, -1, CP_ACP);
				}
#else 
				szName = _tcsdup((TCHAR *)ci.pszVal);
#endif
				miranda_sys_free(ci.pszVal);
				return szName;
			}
		}
	}
	szBaseNick = (char *)CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM)hContact, 0);
	if (szBaseNick != NULL) {
#if defined ( _UNICODE )
		int len;
		len = strlen(szBaseNick) + 1;
		szName = (TCHAR *) malloc(len * 2);
	    MultiByteToWideChar(CP_ACP, 0, szBaseNick, -1, szName, len);
		szName[len - 1] = 0;
	    return szName;
#else
	    return _tcsdup(szBaseNick);
#endif
	}
    return _tcsdup(TranslateT("Unknown Contact"));
}

static void AppendToBuffer(char **buffer, int *cbBufferEnd, int *cbBufferAlloced, const char *fmt, ...)
{
	va_list va;
	int charsDone;

	va_start(va, fmt);
	for (;;) {
		charsDone = _vsnprintf(*buffer + *cbBufferEnd, *cbBufferAlloced - *cbBufferEnd, fmt, va);
		if (charsDone >= 0)
			break;
		*cbBufferAlloced += 1024;
		*buffer = (char *) realloc(*buffer, *cbBufferAlloced);
	}
	va_end(va);
	*cbBufferEnd += charsDone;
}

#if defined( _UNICODE )
static int AppendUnicodeToBuffer(char **buffer, int *cbBufferEnd, int *cbBufferAlloced, TCHAR * line)
{
	DWORD textCharsCount = 0;
	char *d;

	int lineLen = wcslen(line) * 9 + 8;
	if (*cbBufferEnd + lineLen > *cbBufferAlloced) {
		cbBufferAlloced[0] += (lineLen + 1024 - lineLen % 1024);
		*buffer = (char *) realloc(*buffer, *cbBufferAlloced);
	}

	d = *buffer + *cbBufferEnd;
	strcpy(d, "{\\uc1 ");
	d += 6;

	for (; *line; line++, textCharsCount++) {
		if (*line == '\r' && line[1] == '\n') {
			CopyMemory(d, "\\par ", 5);
			line++;
			d += 5;
		}
		else if (*line == '\n') {
			CopyMemory(d, "\\par ", 5);
			d += 5;
		}
		else if (*line == '\t') {
			CopyMemory(d, "\\tab ", 5);
			d += 5;
		}
		else if (*line == '\\' || *line == '{' || *line == '}') {
			*d++ = '\\';
			*d++ = (char) *line;
		}
		else if (*line < 128) {
			*d++ = (char) *line;
		}
		else
			d += sprintf(d, "\\u%d ?", *line);
	}

	strcpy(d, "}");
	d++;

	*cbBufferEnd = (int) (d - *buffer);
	return textCharsCount;
}
#endif

//same as above but does "\r\n"->"\\par " and "\t"->"\\tab " too
static int AppendToBufferWithRTF(char **buffer, int *cbBufferEnd, int *cbBufferAlloced, const char *fmt, ...)
{
	va_list va;
	int charsDone, i;

	va_start(va, fmt);
	for (;;) {
		charsDone = _vsnprintf(*buffer + *cbBufferEnd, *cbBufferAlloced - *cbBufferEnd, fmt, va);
		if (charsDone >= 0)
			break;
		*cbBufferAlloced += 1024;
		*buffer = (char *) realloc(*buffer, *cbBufferAlloced);
	}
	va_end(va);
	*cbBufferEnd += charsDone;
	for (i = *cbBufferEnd - charsDone; (*buffer)[i]; i++) {
		if ((*buffer)[i] == '\r' && (*buffer)[i + 1] == '\n') {
			if (*cbBufferEnd + 4 > *cbBufferAlloced) {
				*cbBufferAlloced += 1024;
				*buffer = (char *) realloc(*buffer, *cbBufferAlloced);
			}
			MoveMemory(*buffer + i + 5, *buffer + i + 2, *cbBufferEnd - i - 1);
			CopyMemory(*buffer + i, "\\par ", 5);
			*cbBufferEnd += 3;
		}
		else if ((*buffer)[i] == '\n') {
			if (*cbBufferEnd + 5 > *cbBufferAlloced) {
				*cbBufferAlloced += 1024;
				*buffer = (char *) realloc(*buffer, *cbBufferAlloced);
			}
			MoveMemory(*buffer + i + 5, *buffer + i + 1, *cbBufferEnd - i);
			CopyMemory(*buffer + i, "\\par ", 5);
			*cbBufferEnd += 4;
		}
		else if ((*buffer)[i] == '\t') {
			if (*cbBufferEnd + 5 > *cbBufferAlloced) {
				*cbBufferAlloced += 1024;
				*buffer = (char *) realloc(*buffer, *cbBufferAlloced);
			}
			MoveMemory(*buffer + i + 5, *buffer + i + 1, *cbBufferEnd - i);
			CopyMemory(*buffer + i, "\\tab ", 5);
			*cbBufferEnd += 4;
		}
		else if ((*buffer)[i] == '\\' || (*buffer)[i] == '{' || (*buffer)[i] == '}') {
			if (*cbBufferEnd + 2 > *cbBufferAlloced) {
				*cbBufferAlloced += 1024;
				*buffer = (char *) realloc(*buffer, *cbBufferAlloced);
			}
			MoveMemory(*buffer + i + 1, *buffer + i, *cbBufferEnd - i + 1);
			(*buffer)[i] = '\\';
			++*cbBufferEnd;
			i++;
		}
	}
	return _mbslen(*buffer + *cbBufferEnd);
}

//free() the return value
static char *CreateRTFHeader(struct MessageWindowData *dat)
{
	char *buffer;
	int bufferAlloced, bufferEnd;
	int i;
	LOGFONTA lf;
	COLORREF colour;
	HDC hdc;

	hdc = GetDC(NULL);
	logPixelSY = GetDeviceCaps(hdc, LOGPIXELSY);
	ReleaseDC(NULL, hdc);
	bufferEnd = 0;
	bufferAlloced = 1024;
	buffer = (char *) malloc(bufferAlloced);
	buffer[0] = '\0';
	if (dat->flags & SMF_RTL)
		AppendToBuffer(&buffer,&bufferEnd,&bufferAlloced,"{\\rtf1\\ansi\\deff0\\rtldoc{\\fonttbl");
	else
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "{\\rtf1\\ansi\\deff0{\\fonttbl");
	for (i = 0; i < msgDlgFontCount; i++) {
		LoadMsgDlgFont(i, &lf, NULL);
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "{\\f%u\\fnil\\fcharset%u %s;}", i, lf.lfCharSet, lf.lfFaceName);
	}
	AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "}{\\colortbl ");
	for (i = 0; i < msgDlgFontCount; i++) {
		LoadMsgDlgFont(i, NULL, &colour);
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\red%u\\green%u\\blue%u;", GetRValue(colour), GetGValue(colour), GetBValue(colour));
	}
	if (GetSysColorBrush(COLOR_HOTLIGHT) == NULL)
		colour = RGB(0, 0, 255);
	else
		colour = GetSysColor(COLOR_HOTLIGHT);
	AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\red%u\\green%u\\blue%u;", GetRValue(colour), GetGValue(colour), GetBValue(colour));
	colour = DBGetContactSettingDword(NULL, SRMMMOD, "BkgColour", RGB(224,224,224));
	AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\red%u\\green%u\\blue%u;", GetRValue(colour), GetGValue(colour), GetBValue(colour));
	colour = DBGetContactSettingDword(NULL, SRMMMOD, "IncomingBkgColour", RGB(224,224,224));
    AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\red%u\\green%u\\blue%u;", GetRValue(colour), GetGValue(colour), GetBValue(colour));
    colour = DBGetContactSettingDword(NULL, SRMMMOD, "OutgoingBkgColour", RGB(224,224,224));
    AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\red%u\\green%u\\blue%u;", GetRValue(colour), GetGValue(colour), GetBValue(colour));
    colour = DBGetContactSettingDword(NULL, SRMMMOD, "LineColour", RGB(224,224,224));
    AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\red%u\\green%u\\blue%u;", GetRValue(colour), GetGValue(colour), GetBValue(colour));
	if (dat->flags & SMF_RTL)
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "}\\rtlpar");
	else
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "}\\pard");
	AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\li30\\ri30\\fi0\\tx0");
	return buffer;
}

//free() the return value
static char *CreateRTFTail(struct MessageWindowData *dat)
{
	char *buffer;
	int bufferAlloced, bufferEnd;

	bufferEnd = 0;
	bufferAlloced = 1024;
	buffer = (char *) malloc(bufferAlloced);
	buffer[0] = '\0';
	AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "}");
	return buffer;
}

//return value is static
static char *SetToStyle(int style)
{
	static char szStyle[128];
	LOGFONTA lf;

	LoadMsgDlgFont(style, &lf, NULL);
	wsprintfA(szStyle, "\\f%u\\cf%u\\b%d\\i%d\\fs%u", style, style, lf.lfWeight >= FW_BOLD ? 1 : 0, lf.lfItalic, 2 * abs(lf.lfHeight) * 74 / logPixelSY);
	return szStyle;
}

char *TimestampToString(DWORD dwFlags, time_t check, int groupStart)
{
    static char szResult[512];
    char str[80];

    DBTIMETOSTRING dbtts;

    dbtts.cbDest = 70;;
    dbtts.szDest = str;

    if(!groupStart || !(dwFlags & SMF_SHOWDATE)) {
        dbtts.szFormat = (dwFlags & SMF_SHOWSECONDS) ? (char *)"s" : (char *)"t";
        szResult[0] = '\0';
    }
    else {
		struct tm tm_now, tm_today;
		time_t now = time(NULL);
		time_t today;
        tm_now = *localtime(&now);
        tm_today = tm_now;
        tm_today.tm_hour = tm_today.tm_min = tm_today.tm_sec = 0;
        today = mktime(&tm_today);

        if(dwFlags & SMF_RELATIVEDATE && check >= today) {
            dbtts.szFormat = (dwFlags & SMF_SHOWSECONDS) ? (char *)"s" : (char *)"t";
            strcpy(szResult, Translate("Today"));
	        strcat(szResult, ", ");
        }
        else if(dwFlags & SMF_RELATIVEDATE && check > (today - 86400)) {
            dbtts.szFormat = (dwFlags & SMF_SHOWSECONDS) ? (char *)"s" : (char *)"t";
            strcpy(szResult, Translate("Yesterday"));
	        strcat(szResult, ", ");
        }
        else {
            if(dwFlags & SMF_LONGDATE)
                dbtts.szFormat = (dwFlags & SMF_SHOWSECONDS) ? (char *)"D s" : (char *)"D t";
            else
                dbtts.szFormat = (dwFlags & SMF_SHOWSECONDS) ? (char *)"d s" : (char *)"d t";
            szResult[0] = '\0';
        }
    }
	CallService(MS_DB_TIME_TIMESTAMPTOSTRING, check, (LPARAM) & dbtts);
    strncat(szResult, str, 500);
    return szResult;
}

int isSameDate(DWORD time1, DWORD time2)
{
    struct tm tm_t1, tm_t2;
    tm_t1 = *localtime((time_t *)(&time1));
    tm_t2 = *localtime((time_t *)(&time2));
    if (tm_t1.tm_year == tm_t2.tm_year && tm_t1.tm_mon == tm_t2.tm_mon
		&& tm_t1.tm_mday == tm_t2.tm_mday) {
		return 1;
	}
	return 0;
}

//free() the return value
static char *CreateRTFFromDbEvent2(struct MessageWindowData *dat, struct EventData *event, int prefixParaBreak, int firstEvent, struct LogStreamData *streamData)
{
	char *buffer;
	int bufferAlloced, bufferEnd;
	int showColon = 0;
	int isGroupBreak = TRUE;
	bufferEnd = 0;
	bufferAlloced = 1024;
	buffer = (char *) malloc(bufferAlloced);
	buffer[0] = '\0';
	if (prefixParaBreak) {
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\par");
	}
 	if ((g_dat->flags & SMF_GROUPMESSAGES) && event->flags == LOWORD(dat->lastEventType)
	  && event->eventType == EVENTTYPE_MESSAGE && HIWORD(dat->lastEventType) == EVENTTYPE_MESSAGE
	  && (isSameDate(event->timestamp, dat->lastEventTime))
//	  && ((dbei.timestamp - dat->lastEventTime) < 86400)
	  && ((((int)event->timestamp < dat->startTime) == (dat->lastEventTime < dat->startTime)) || !(event->flags & DBEF_READ))) {
		isGroupBreak = FALSE;
	}
	if (!firstEvent && isGroupBreak && (g_dat->flags & SMF_DRAWLINES)) {
//		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\sl-1\\highlight%d\\line\\sl0", msgDlgFontCount + 3);
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\sl-1\\slmult0\\highlight%d\\par\\sl0", msgDlgFontCount + 4);
	}
	if (event->eventType == EVENTTYPE_MESSAGE) {
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\highlight%d", msgDlgFontCount + 2 + ((event->flags & DBEF_SENT) ? 1 : 0));
	} else {
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\highlight%d", msgDlgFontCount + 1);
	}

	if (g_dat->flags&SMF_SHOWICONS && isGroupBreak) {
		int i = LOGICON_MSG_NOTICE;

		switch (event->eventType) {
			case EVENTTYPE_MESSAGE:
				if (event->flags & DBEF_SENT) {
					i = LOGICON_MSG_OUT;
				}
				else {
					i = LOGICON_MSG_IN;
				}
				break;
			case EVENTTYPE_STATUSCHANGE:
			case EVENTTYPE_URL:
			case EVENTTYPE_FILE:
				i = LOGICON_MSG_NOTICE;
				break;
		}
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\f0\\fs14\\-");
		while (bufferAlloced - bufferEnd < logIconBmpSize[i])
			bufferAlloced += 1024;
		buffer = (char *) realloc(buffer, bufferAlloced);
		CopyMemory(buffer + bufferEnd, pLogIconBmpBits[i], logIconBmpSize[i]);
		bufferEnd += logIconBmpSize[i];
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, " ");
	}

	if (g_dat->flags&SMF_SHOWTIME &&
		(event->eventType != EVENTTYPE_MESSAGE ||
		!(g_dat->flags & SMF_GROUPMESSAGES) ||
		(isGroupBreak && !(g_dat->flags & SMF_MARKFOLLOWUPS)) ||  (!isGroupBreak && (g_dat->flags & SMF_MARKFOLLOWUPS))))
	{
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "%s ", SetToStyle(event->flags & DBEF_SENT ? MSGFONTID_MYTIME : MSGFONTID_YOURTIME));
		AppendToBufferWithRTF(&buffer, &bufferEnd, &bufferAlloced, "%s", TimestampToString(g_dat->flags, event->timestamp, isGroupBreak));
		if (event->eventType != EVENTTYPE_MESSAGE) {
			AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "%s : ", SetToStyle(event->flags & DBEF_SENT ? MSGFONTID_MYCOLON : MSGFONTID_YOURCOLON));
		}
		showColon = 1;
	}
	if ((!(g_dat->flags&SMF_HIDENAMES) && event->eventType == EVENTTYPE_MESSAGE && isGroupBreak) || event->eventType == EVENTTYPE_STATUSCHANGE) {
		if (event->eventType == EVENTTYPE_MESSAGE) {
			if (showColon) {
				AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, " %s ", SetToStyle(event->flags & DBEF_SENT ? MSGFONTID_MYNAME : MSGFONTID_YOURNAME));
			} else {
				AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "%s ", SetToStyle(event->flags & DBEF_SENT ? MSGFONTID_MYNAME : MSGFONTID_YOURNAME));
			}
		} else {
			AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "%s ", SetToStyle(MSGFONTID_NOTICE));
		}
		{
			TCHAR *szName;
			if (event->flags & DBEF_SENT) {
				szName = GetNickname(NULL, event->szModule);
			} else {
				szName = GetNickname(event->hContact, event->szModule);
			}
#if defined( _UNICODE )
			AppendUnicodeToBuffer(&buffer, &bufferEnd, &bufferAlloced, szName);
#else
			AppendToBufferWithRTF(&buffer, &bufferEnd, &bufferAlloced, "%s", szName);
#endif
			free(szName);
		}
		showColon = 1;
		if (event->eventType == EVENTTYPE_MESSAGE && g_dat->flags & SMF_GROUPMESSAGES) {
			AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\par");
			showColon = 0;
		}
	}
	if (g_dat->flags&SMF_SHOWTIME && g_dat->flags & SMF_GROUPMESSAGES && g_dat->flags & SMF_MARKFOLLOWUPS
		&& event->eventType == EVENTTYPE_MESSAGE && isGroupBreak) {
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, " %s ", SetToStyle(event->flags & DBEF_SENT ? MSGFONTID_MYTIME : MSGFONTID_YOURTIME));
		AppendToBufferWithRTF(&buffer, &bufferEnd, &bufferAlloced, "%s", TimestampToString(g_dat->flags, event->timestamp, isGroupBreak));
		showColon = 1;
	}
	if (showColon && event->eventType == EVENTTYPE_MESSAGE) {
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "%s : ", SetToStyle(event->flags & DBEF_SENT ? MSGFONTID_MYCOLON : MSGFONTID_YOURCOLON));
	}
	switch (event->eventType) {
		case EVENTTYPE_MESSAGE:
		if (g_dat->flags & SMF_MSGONNEWLINE && showColon) {
			AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\par");
		}
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "%s ", SetToStyle(event->flags & DBEF_SENT ? MSGFONTID_MYMSG : MSGFONTID_YOURMSG));

#if defined( _UNICODE )
		if (event->wtext != NULL) {
			AppendUnicodeToBuffer(&buffer, &bufferEnd, &bufferAlloced, event->wtext);
		} else {
			AppendToBufferWithRTF(&buffer, &bufferEnd, &bufferAlloced, "%s", event->text);
		}
#else
		AppendToBufferWithRTF(&buffer, &bufferEnd, &bufferAlloced, "%s", event->text);
#endif
		break;
		case EVENTTYPE_STATUSCHANGE:
		case EVENTTYPE_URL:
		case EVENTTYPE_FILE:
		{
			AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "%s ", SetToStyle(MSGFONTID_NOTICE));
			if (event->eventType == EVENTTYPE_FILE) {
				if (event->flags & DBEF_SENT) {
					AppendToBufferWithRTF(&buffer, &bufferEnd, &bufferAlloced, "%s: %s", Translate("File sent"), event->text);
				} else {
					AppendToBufferWithRTF(&buffer, &bufferEnd, &bufferAlloced, "%s: %s", Translate("File received"), event->text);
				}
			} else if (event->eventType == EVENTTYPE_URL) {
				if (event->flags & DBEF_SENT) {
					AppendToBufferWithRTF(&buffer, &bufferEnd, &bufferAlloced, "%s: %s", Translate("URL sent"), event->text);
				} else {
					AppendToBufferWithRTF(&buffer, &bufferEnd, &bufferAlloced, "%s: %s", Translate("URL received"), event->text);
				}
			} else {
				AppendToBufferWithRTF(&buffer, &bufferEnd, &bufferAlloced, " %s", event->text);
			}
			break;
		}
	}
	if (!prefixParaBreak) {
		AppendToBuffer(&buffer, &bufferEnd, &bufferAlloced, "\\par");
	}
	dat->lastEventTime = event->timestamp;
	dat->lastEventType = MAKELONG(event->flags, event->eventType);
	dat->lastEventContact = event->hContact;
	return buffer;
}

static DWORD CALLBACK LogStreamInEvents(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG * pcb)
{
	struct LogStreamData *dat = (struct LogStreamData *) dwCookie;

	if (dat->buffer == NULL) {
		dat->bufferOffset = 0;
		switch (dat->stage) {
			case STREAMSTAGE_HEADER:
				dat->buffer = CreateRTFHeader(dat->dlgDat);
				dat->stage = STREAMSTAGE_EVENTS;
				break;
			case STREAMSTAGE_EVENTS:
				if (dat->eventsToInsert) {
					do {
#ifdef MIRANDA_0_5
						struct EventData *event = getEventFromDB(dat->dlgDat, dat->hContact, dat->hDbEvent);
						dat->buffer = NULL;
						if (event != NULL) {
							dat->buffer = CreateRTFFromDbEvent2(dat->dlgDat, event, !dat->isFirst, dat->isEmpty, dat);
							freeEvent(event);
						}
#else
						dat->buffer = CreateRTFFromDbEvent(dat->dlgDat, dat->hContact, dat->hDbEvent, !dat->isFirst, dat->isEmpty, dat);
#endif
						if (dat->buffer)
							dat->hDbEventLast = dat->hDbEvent;
						dat->hDbEvent = (HANDLE) CallService(MS_DB_EVENT_FINDNEXT, (WPARAM) dat->hDbEvent, 0);
						if (--dat->eventsToInsert == 0)
							break;
					} while (dat->buffer == NULL && dat->hDbEvent);
					if (dat->buffer) {
						dat->isEmpty = 0;
						break;
					}
				}
				dat->stage = STREAMSTAGE_TAIL;
				//fall through
			case STREAMSTAGE_TAIL:
				dat->buffer = CreateRTFTail(dat->dlgDat);
				dat->stage = STREAMSTAGE_STOP;
				break;
			case STREAMSTAGE_STOP:
				*pcb = 0;
				return 0;
		}
		dat->bufferLen = lstrlenA(dat->buffer);
	}
	*pcb = min(cb, dat->bufferLen - dat->bufferOffset);
	CopyMemory(pbBuff, dat->buffer + dat->bufferOffset, *pcb);
	dat->bufferOffset += *pcb;
	if (dat->bufferOffset == dat->bufferLen) {
		free(dat->buffer);
		dat->buffer = NULL;
	}
	return 0;
}

void StreamInEvents(HWND hwndDlg, HANDLE hDbEventFirst, int count, int fAppend)
{
	FINDTEXTEXA fi;
	EDITSTREAM stream = { 0 };
	struct LogStreamData streamData = { 0 };
	struct MessageWindowData *dat = (struct MessageWindowData *) GetWindowLong(hwndDlg, GWL_USERDATA);
	CHARRANGE oldSel, sel;

// IEVIew MOD Begin
	if (dat->hwndLog != NULL) {
		IEVIEWEVENT event;
		IEVIEWWINDOW ieWindow;
		event.cbSize = sizeof(IEVIEWEVENT);
		event.dwFlags = ((dat->flags & SMF_RTL) ? IEEF_RTL : 0) | ((dat->flags & SMF_DISABLE_UNICODE) ? IEEF_NO_UNICODE : 0);
		event.hwnd = dat->hwndLog;
		event.hContact = dat->hContact;
		if (!fAppend) {
			event.iType = IEE_CLEAR_LOG;
			CallService(MS_IEVIEW_EVENT, 0, (LPARAM)&event);
		}
		event.iType = IEE_LOG_EVENTS;
		event.codepage = dat->codePage;
		event.hDbEventFirst = hDbEventFirst;
		event.count = count;
		CallService(MS_IEVIEW_EVENT, 0, (LPARAM)&event);
		dat->hDbEventLast = event.hDbEventFirst != NULL ? event.hDbEventFirst : dat->hDbEventLast;

		ieWindow.cbSize = sizeof(IEVIEWWINDOW);
		ieWindow.iType = IEW_SCROLLBOTTOM;
		ieWindow.hwnd = dat->hwndLog;
		CallService(MS_IEVIEW_WINDOW, 0, (LPARAM)&ieWindow);
		return;
	}
// IEVIew MOD End

	SendDlgItemMessage(hwndDlg, IDC_LOG, EM_HIDESELECTION, TRUE, 0);
	SendDlgItemMessage(hwndDlg, IDC_LOG, EM_EXGETSEL, 0, (LPARAM) & oldSel);
	streamData.hContact = dat->hContact;
	streamData.hDbEvent = hDbEventFirst;
	streamData.hDbEventLast = dat->hDbEventLast;
	streamData.dlgDat = dat;
	streamData.eventsToInsert = count;
	streamData.isEmpty = fAppend ? GetWindowTextLength(GetDlgItem(hwndDlg, IDC_LOG)) == 0 : 1;
	streamData.isFirst = streamData.isEmpty;
	stream.pfnCallback = LogStreamInEvents;
	stream.dwCookie = (DWORD_PTR) & streamData;
	sel.cpMin = 0;
	if (fAppend) {
        GETTEXTLENGTHEX gtxl = {0};
#if defined( _UNICODE )
        gtxl.codepage = 1200;
        gtxl.flags = GTL_DEFAULT | GTL_PRECISE | GTL_NUMCHARS;
#else
        gtxl.codepage = CP_ACP;
        gtxl.flags = GTL_DEFAULT | GTL_PRECISE;
#endif
        sel.cpMin = sel.cpMax = GetWindowTextLength(GetDlgItem(hwndDlg, IDC_LOG));
        SendDlgItemMessage(hwndDlg, IDC_LOG, EM_EXSETSEL, 0, (LPARAM) & sel);
        fi.chrg.cpMin = SendDlgItemMessage(hwndDlg, IDC_LOG, EM_GETTEXTLENGTHEX, (WPARAM)&gtxl, 0);
    } else
        fi.chrg.cpMin = 0;

	SendDlgItemMessage(hwndDlg, IDC_LOG, EM_STREAMIN, fAppend ? SFF_SELECTION | SF_RTF : SF_RTF, (LPARAM) & stream);
	SendDlgItemMessage(hwndDlg, IDC_LOG, EM_EXSETSEL, 0, (LPARAM) & oldSel);
	SendDlgItemMessage(hwndDlg, IDC_LOG, EM_HIDESELECTION, FALSE, 0);
	if (ServiceExists(MS_SMILEYADD_REPLACESMILEYS)) {
		SMADD_RICHEDIT2 smre;
		smre.cbSize = sizeof(SMADD_RICHEDIT2);
		smre.hwndRichEditControl = GetDlgItem(hwndDlg, IDC_LOG);
		smre.Protocolname = dat->szProto;
        if (dat->szProto!=NULL && strcmp(dat->szProto,"MetaContacts")==0) {
            HANDLE hContact = (HANDLE) CallService(MS_MC_GETMOSTONLINECONTACT, (WPARAM) dat->hContact, 0);
            if (hContact!=NULL) {
                smre.Protocolname = (char*) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM)hContact, 0);
            }
        }
		if (fi.chrg.cpMin > 0) {
			sel.cpMin = fi.chrg.cpMin;
			sel.cpMax = -1;
			smre.rangeToReplace = &sel;
		} else {
			smre.rangeToReplace = NULL;
		}
		smre.rangeToReplace = NULL;
		smre.useSounds = FALSE;
		smre.disableRedraw = TRUE;
		CallService(MS_SMILEYADD_REPLACESMILEYS, 0, (LPARAM) &smre);
	}
//	if (GetWindowLong(GetDlgItem(hwndDlg, IDC_LOG), GWL_STYLE) & WS_VSCROLL)
	{
		int len;
		len = GetWindowTextLengthA(GetDlgItem(hwndDlg, IDC_LOG));
		SendDlgItemMessage(hwndDlg, IDC_LOG, EM_SETSEL, len - 1, len - 1);
	}
	dat->hDbEventLast = streamData.hDbEventLast;
	PostMessage(hwndDlg, DM_SCROLLLOGTOBOTTOM, 0, 0);
}

#define RTFPICTHEADERMAXSIZE   78
void LoadMsgLogIcons(void)
{
	HICON hIcon = NULL;
	HBITMAP hBmp, hoBmp;
	HDC hdc, hdcMem;
	BITMAPINFOHEADER bih = { 0 };
	int widthBytes, i;
	RECT rc;
	HBRUSH hBrush;
	HBRUSH hBkgBrush;
	HBRUSH hInBkgBrush;
	HBRUSH hOutBkgBrush;
	int rtfHeaderSize;
	PBYTE pBmpBits;

	g_hImageList = ImageList_Create(10, 10, IsWinVerXPPlus()? ILC_COLOR32 | ILC_MASK : ILC_COLOR8 | ILC_MASK, sizeof(pLogIconBmpBits) / sizeof(pLogIconBmpBits[0]), 0);
	hBkgBrush = CreateSolidBrush(DBGetContactSettingDword(NULL, SRMMMOD, SRMSGSET_BKGCOLOUR, SRMSGDEFSET_BKGCOLOUR));
	hInBkgBrush = CreateSolidBrush(DBGetContactSettingDword(NULL, SRMMMOD, SRMSGSET_INCOMINGBKGCOLOUR, SRMSGDEFSET_INCOMINGBKGCOLOUR));
	hOutBkgBrush = CreateSolidBrush(DBGetContactSettingDword(NULL, SRMMMOD, SRMSGSET_OUTGOINGBKGCOLOUR, SRMSGDEFSET_OUTGOINGBKGCOLOUR));
	bih.biSize = sizeof(bih);
	bih.biBitCount = 24;
	bih.biCompression = BI_RGB;
	bih.biHeight = 10;
	bih.biPlanes = 1;
	bih.biWidth = 10;
	widthBytes = ((bih.biWidth * bih.biBitCount + 31) >> 5) * 4;
	rc.top = rc.left = 0;
	rc.right = bih.biWidth;
	rc.bottom = bih.biHeight;
	hdc = GetDC(NULL);
	hBmp = CreateCompatibleBitmap(hdc, bih.biWidth, bih.biHeight);
	hdcMem = CreateCompatibleDC(hdc);
	pBmpBits = (PBYTE) malloc(widthBytes * bih.biHeight);
	hBrush = hBkgBrush;
	for (i = 0; i < sizeof(pLogIconBmpBits) / sizeof(pLogIconBmpBits[0]); i++) {
		switch (i) {
			case LOGICON_MSG_IN:
				ImageList_AddIcon(g_hImageList, g_dat->hIcons[SMF_ICON_INCOMING]);
				hIcon = ImageList_GetIcon(g_hImageList, LOGICON_MSG_IN, ILD_NORMAL);
				hBrush = hInBkgBrush;
				break;
			case LOGICON_MSG_OUT:
				ImageList_AddIcon(g_hImageList, g_dat->hIcons[SMF_ICON_OUTGOING]);
				hIcon = ImageList_GetIcon(g_hImageList, LOGICON_MSG_OUT, ILD_NORMAL);
				hBrush = hOutBkgBrush;
				break;
			case LOGICON_MSG_NOTICE:
				ImageList_AddIcon(g_hImageList, g_dat->hIcons[SMF_ICON_NOTICE]);
				hIcon = ImageList_GetIcon(g_hImageList, LOGICON_MSG_NOTICE, ILD_NORMAL);
				//hBrush = hInBkgBrush;
				hBrush = hBkgBrush;
				break;
		}
		pLogIconBmpBits[i] = (PBYTE) malloc(RTFPICTHEADERMAXSIZE + (bih.biSize + widthBytes * bih.biHeight) * 2);
		//I can't seem to get binary mode working. No matter.
		rtfHeaderSize = sprintf(pLogIconBmpBits[i], "{\\pict\\dibitmap0\\wbmbitspixel%u\\wbmplanes1\\wbmwidthbytes%u\\picw%u\\pich%u ", bih.biBitCount, widthBytes, (UINT) bih.biWidth, (UINT)bih.biHeight);
		hoBmp = (HBITMAP) SelectObject(hdcMem, hBmp);
		FillRect(hdcMem, &rc, hBrush);
		DrawIconEx(hdcMem, 0, 0, hIcon, bih.biWidth, bih.biHeight, 0, NULL, DI_NORMAL);
		SelectObject(hdcMem, hoBmp);
		GetDIBits(hdc, hBmp, 0, bih.biHeight, pBmpBits, (BITMAPINFO *) & bih, DIB_RGB_COLORS);
		DestroyIcon(hIcon);
		{
			int n;
			for (n = 0; n < sizeof(BITMAPINFOHEADER); n++)
				sprintf(pLogIconBmpBits[i] + rtfHeaderSize + n * 2, "%02X", ((PBYTE) & bih)[n]);
			for (n = 0; n < widthBytes * bih.biHeight; n += 4)
				sprintf(pLogIconBmpBits[i] + rtfHeaderSize + (bih.biSize + n) * 2, "%02X%02X%02X%02X", pBmpBits[n], pBmpBits[n + 1], pBmpBits[n + 2], pBmpBits[n + 3]);
		}
		logIconBmpSize[i] = rtfHeaderSize + (bih.biSize + widthBytes * bih.biHeight) * 2 + 1;
		pLogIconBmpBits[i][logIconBmpSize[i] - 1] = '}';
	}
	free(pBmpBits);
	DeleteDC(hdcMem);
	DeleteObject(hBmp);
	ReleaseDC(NULL, hdc);
	DeleteObject(hBkgBrush);
	DeleteObject(hInBkgBrush);
	DeleteObject(hOutBkgBrush);
}

void FreeMsgLogIcons(void)
{
	int i;
	for (i = 0; i < sizeof(pLogIconBmpBits) / sizeof(pLogIconBmpBits[0]); i++)
		free(pLogIconBmpBits[i]);
	ImageList_RemoveAll(g_hImageList);
	ImageList_Destroy(g_hImageList);
}
