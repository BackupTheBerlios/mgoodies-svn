/* 
Copyright (C) 2006 Ricardo Pescuma Domenecci

This is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this file; see the file license.txt.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  
*/


#ifndef __M_HISTORYEVENTS_H__
# define __M_HISTORYEVENTS_H__


#define MIID_HISTORYEVENTS { 0xc8be8543, 0x6618, 0x4030, { 0x85, 0xcf, 0x90, 0x82, 0xc7, 0xde, 0x7f, 0xf7 } }


#define HISTORYEVENTS_FORMAT_CHAR			1
#define HISTORYEVENTS_FORMAT_WCHAR			2
#define HISTORYEVENTS_FORMAT_RICH_TEXT		4
#define HISTORYEVENTS_FORMAT_HTML			8

#define HISTORYEVENTS_FLAG_DEFAULT						(1 << 0)
#define HISTORYEVENTS_FLAG_SHOW_IM_SRMM					(1 << 1)
#define HISTORYEVENTS_FLAG_USE_SENT_FLAG				(1 << 2) // Means that it can be a sent or received and uses DBEF_SENT to mark that
#define HISTORYEVENTS_FLAG_EXPECT_CONTACT_NAME_BEFORE	(1 << 3) // Means that who is drawing this should draw the contact name before the text
#define HISTORYEVENTS_FLAG_KEEP_ONE_MONTH 				(1 << 8) // By default store in db for 1 month
#define HISTORYEVENTS_FLAG_KEEP_ONE_WEEK 				(2 << 8) // By default store in db for 1 week
#define HISTORYEVENTS_FLAG_KEEP_FOR_SRMM 				(3 << 8) // By default store in db only enought for message log
#define HISTORYEVENTS_FLAG_KEEP_DONT					(4 << 8) // By default don't store in db (aka ignore it)


// This function must be implemented by subscribers. It must return a pointer or NULL
// to say it can't handle the text
typedef void * (*fGetHistoryEventText)(HANDLE hContact, HANDLE hDbEvent, DBEVENTINFO *dbe, int format);

typedef struct {
	int cbSize;
	char *module;
	char *name;				// Internal event name
	char *description;		// Will be translated
	WORD eventType;			// The event type it can handle
	union {
		HICON defaultIcon;			// Optional
		char * defaultIconName;		// Don't use - internal use only
	};
	int supports;			// What kind of return is supported - or of HISTORYEVENTS_FORMAT_*
	int flags;				// or of HISTORYEVENTS_FLAG_*
	fGetHistoryEventText pfGetHistoryEventText;	// NULL to use default get text (similar to message, without extra format)

	// Aditional data if wants to use add to history services
	char **templates; // Each entry is: "Name\nDefault\n%var%\tDescription\n%var%\tDescription\n%var%\tDescription"
	int numTemplates;

} HISTORY_EVENT_HANDLER;


/*
Register a plugin that can handle an event type

wParam: HISTORY_EVENT_HANDLER *
lParam: ignored
Return: 0 for success
*/
#define MS_HISTORYEVENTS_REGISTER		"HistoryEvents/Register"


typedef struct {
	int cbSize;
	HANDLE hDbEvent;
	DBEVENTINFO *dbe;	// Optional
	int format;			// one of HISTORYEVENTS_FORMAT_*

} HISTORY_EVENT_PARAM;

/*
Check if an event can be handled by any subscribers

wParam: WORD - event type
lParam: ignored
Return: BOOL 
*/
#define MS_HISTORYEVENTS_CAN_HANDLE		"HistoryEvents/CanHandle"

/*
Get the icon for a history event type

wParam: WORD - event type
lParam: ignored
Return: HICON - after use free with MS_HISTORYEVENTS_RELEASE_ICON
*/
#define MS_HISTORYEVENTS_GET_ICON		"HistoryEvents/GetIcon"

/*
Get the flags for a history event type

wParam: WORD - event type
lParam: ignored
Return: int - or of HISTORYEVENTS_FLAG_* or -1 if error
*/
#define MS_HISTORYEVENTS_GET_FLAGS		"HistoryEvents/GetFlags"

/*
Release the icon for a history event type. This is really just a forward to icolib

wParam: HICON
lParam: ignored
*/
#define MS_HISTORYEVENTS_RELEASE_ICON	"Skin2/Icons/ReleaseIcon"

/*
Get the text for a history event type

wParam: HISTORY_EVENT_PARAM *
lParam: ignored
Return: char * or wchar * depending on sent flags. Free with mir_free or MS_HISTORYEVENTS_RELEASE_TEXT
*/
#define MS_HISTORYEVENTS_GET_TEXT		"HistoryEvents/GetText"

/*
Release the text for a history event type. Internally is just a call to mir_free

wParam: char * or wchar * 
lParam: ignored
*/
#define MS_HISTORYEVENTS_RELEASE_TEXT	"HistoryEvents/ReleaseText"



typedef struct {
	int cbSize;
	HANDLE hContact;
	WORD eventType;
	int templateNum;
	TCHAR **variables;
	int numVariables;
	PBYTE additionalData;
	int additionalDataSize;
	int flags;
} HISTORY_EVENT_ADD;

/*
Add an registered event to the history. This is a helper service

wParam: HISTORY_EVENT_ADD
lParam: ignored
Return: HANDLE to the db event
*/
#define MS_HISTORYEVENTS_ADD_TO_HISTORY	"HistoryEvents/AddToHistory"



// Helper functions //////////////////////////////////////////////////////////////////////////////




static int HistoryEvents_Register(char *module, char *name, char *description, int eventType, HICON defaultIcon, 
								  int supports, int flags, fGetHistoryEventText pfGetHistoryEventText)
{
	HISTORY_EVENT_HANDLER heh = {0};

	if (!ServiceExists(MS_HISTORYEVENTS_REGISTER))
		return 1;

	heh.cbSize = sizeof(heh);
	heh.module = module;
	heh.name = name;
	heh.description = description;
	heh.eventType = eventType;
	heh.defaultIcon = defaultIcon;
	heh.supports = supports;
	heh.flags = flags;
	heh.pfGetHistoryEventText = pfGetHistoryEventText;
	return CallService(MS_HISTORYEVENTS_REGISTER, (WPARAM) &heh, 0);
}

static int HistoryEvents_RegisterWithTemplates(char *module, char *name, char *description, int eventType, HICON defaultIcon, 
								  int supports, int flags, fGetHistoryEventText pfGetHistoryEventText,
								  char **templates, int numTemplates)
{
	HISTORY_EVENT_HANDLER heh = {0};

	if (!ServiceExists(MS_HISTORYEVENTS_REGISTER))
		return 1;

	heh.cbSize = sizeof(heh);
	heh.module = module;
	heh.name = name;
	heh.description = description;
	heh.eventType = eventType;
	heh.defaultIcon = defaultIcon;
	heh.supports = supports;
	heh.flags = flags;
	heh.pfGetHistoryEventText = pfGetHistoryEventText;
	heh.templates = templates;
	heh.numTemplates = numTemplates;
	return CallService(MS_HISTORYEVENTS_REGISTER, (WPARAM) &heh, 0);
}

static int HistoryEvents_RegisterMessageStyle(char *module, char *name, char *description, int eventType, HICON defaultIcon, 
										int flags, char **templates, int numTemplates)
{
	HISTORY_EVENT_HANDLER heh = {0};

	if (!ServiceExists(MS_HISTORYEVENTS_REGISTER))
		return 1;

	heh.cbSize = sizeof(heh);
	heh.module = module;
	heh.name = name;
	heh.description = description;
	heh.eventType = eventType;
	heh.defaultIcon = defaultIcon;
	heh.flags = flags;
	heh.templates = templates;
	heh.numTemplates = numTemplates;
	return CallService(MS_HISTORYEVENTS_REGISTER, (WPARAM) &heh, 0);
}

static BOOL HistoryEvents_CanHandle(WORD eventType)
{
	if (!ServiceExists(MS_HISTORYEVENTS_CAN_HANDLE))
		return FALSE;

	return (BOOL) CallService(MS_HISTORYEVENTS_CAN_HANDLE, (WPARAM) eventType, 0);
}

static HICON HistoryEvents_GetIcon(WORD eventType)
{
	if (!ServiceExists(MS_HISTORYEVENTS_GET_ICON))
		return NULL;

	return (HICON) CallService(MS_HISTORYEVENTS_GET_ICON, (WPARAM) eventType, 0);
}

static int HistoryEvents_GetFlags(WORD eventType)
{
	if (!ServiceExists(MS_HISTORYEVENTS_GET_FLAGS))
		return -1;

	return (int) CallService(MS_HISTORYEVENTS_GET_FLAGS, (WPARAM) eventType, 0);
}

static void HistoryEvents_ReleaseIcon(HICON icon)
{
	CallService(MS_HISTORYEVENTS_RELEASE_ICON, (WPARAM) icon, 0);
}

static char * HistoryEvents_GetTextA(HANDLE hDbEvent, DBEVENTINFO *dbe)
{
	HISTORY_EVENT_PARAM hep = {0};

	if (!ServiceExists(MS_HISTORYEVENTS_GET_TEXT))
		return NULL;

	hep.cbSize = sizeof(hep);
	hep.hDbEvent = hDbEvent;
	hep.dbe = dbe;
	hep.format = HISTORYEVENTS_FORMAT_CHAR;
	return (char *) CallService(MS_HISTORYEVENTS_GET_TEXT, (WPARAM) &hep, 0);
}

static wchar_t * HistoryEvents_GetTextW(HANDLE hDbEvent, DBEVENTINFO *dbe)
{
	HISTORY_EVENT_PARAM hep = {0};

	if (!ServiceExists(MS_HISTORYEVENTS_GET_TEXT))
		return NULL;

	hep.cbSize = sizeof(hep);
	hep.hDbEvent = hDbEvent;
	hep.dbe = dbe;
	hep.format = HISTORYEVENTS_FORMAT_WCHAR;
	return (wchar_t *) CallService(MS_HISTORYEVENTS_GET_TEXT, (WPARAM) &hep, 0);
}

static char * HistoryEvents_GetRichText(HANDLE hDbEvent, DBEVENTINFO *dbe)
{
	HISTORY_EVENT_PARAM hep = {0};

	if (!ServiceExists(MS_HISTORYEVENTS_GET_TEXT))
		return NULL;

	hep.cbSize = sizeof(hep);
	hep.hDbEvent = hDbEvent;
	hep.dbe = dbe;
	hep.format = HISTORYEVENTS_FORMAT_RICH_TEXT;
	return (char *) CallService(MS_HISTORYEVENTS_GET_TEXT, (WPARAM) &hep, 0);
}

#define HistoryEvents_ReleaseText mir_free
//static void HistoryEvents_ReleaseText(void *str)
//{
//	if (!ServiceExists(MS_HISTORYEVENTS_RELEASE_TEXT))
//		return;
//
//	CallService(MS_HISTORYEVENTS_RELEASE_TEXT, (WPARAM) str, 0);
//}

static HANDLE HistoryEvents_AddToHistory(HANDLE hContact, WORD eventType, int templateNum, 
										 TCHAR **variables, int numVariables, 
										 PBYTE additionalData, int additionalDataSize,
										 int flags)
{
	HISTORY_EVENT_ADD hea = {0};

	if (!ServiceExists(MS_HISTORYEVENTS_ADD_TO_HISTORY))
		return NULL;

	hea.cbSize = sizeof(hea);
	hea.hContact = hContact;
	hea.eventType = eventType;
	hea.templateNum = templateNum;
	hea.numVariables = numVariables;
	hea.variables = variables;
	hea.additionalData = additionalData;
	hea.additionalDataSize = additionalDataSize;
	hea.flags = flags;

	return (HANDLE) CallService(MS_HISTORYEVENTS_ADD_TO_HISTORY, (WPARAM) &hea, 0);
}

static HANDLE HistoryEvents_AddToHistorySimple(HANDLE hContact, WORD eventType, int templateNum, int flags)
{
	HISTORY_EVENT_ADD hea = {0};

	if (!ServiceExists(MS_HISTORYEVENTS_ADD_TO_HISTORY))
		return NULL;

	hea.cbSize = sizeof(hea);
	hea.hContact = hContact;
	hea.eventType = eventType;
	hea.templateNum = templateNum;
	hea.flags = flags;

	return (HANDLE) CallService(MS_HISTORYEVENTS_ADD_TO_HISTORY, (WPARAM) &hea, 0);
}


#ifdef UNICODE

#define HistoryEvents_GetTextT HistoryEvents_GetTextW

#else

#define HistoryEvents_GetTextT HistoryEvents_GetTextA

#endif



#endif // __M_HISTORYEVENTS_H__