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


#include "commons.h"


// Prototypes ///////////////////////////////////////////////////////////////////////////


PLUGININFOEX pluginInfo={
	sizeof(PLUGININFOEX),
	"Status Message Retriever",
	PLUGIN_MAKE_VERSION(1,0,0,7),
	"Retrieve status message based on timer / status change",
	"Ricardo Pescuma Domenecci, Tomasz Słotwiński",
	"",
	"© 2007 Ricardo Pescuma Domenecci, Tomasz Słotwiński",
	"http://pescuma.mirandaim.ru/miranda/smr",
	0, 
	0,		//doesn't replace anything built-in
	{ 0x800a5c24, 0x737b, 0x499f, { 0x96, 0xa2, 0x40, 0x46, 0xda, 0xa8, 0x41, 0xb1 } } // {800A5C24-737B-499f-96A2-4046DAA841B1}
};


HINSTANCE hInst;
PLUGINLINK *pluginLink;

HANDLE hEnableMenu = NULL; 
HANDLE hDisableMenu = NULL; 
HANDLE hModulesLoaded = NULL;
HANDLE hPreBuildCMenu = NULL;

int ModulesLoaded(WPARAM wParam, LPARAM lParam);

int PreBuildContactMenu(WPARAM wParam,LPARAM lParam);

int EnableContactMsgRetrieval(WPARAM wParam,LPARAM lParam);
int DisableContactMsgRetrieval(WPARAM wParam,LPARAM lParam);
int MsgRetrievalEnabledForUser(WPARAM wParam, LPARAM lParam);
int MsgRetrievalEnabledForProtocol(WPARAM wParam, LPARAM lParam);


// Functions ////////////////////////////////////////////////////////////////////////////


extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) 
{
	hInst = hinstDLL;
	return TRUE;
}


extern "C" __declspec(dllexport) PLUGININFO* MirandaPluginInfo(DWORD mirandaVersion) 
{
	pluginInfo.cbSize = sizeof(PLUGININFO);
	return (PLUGININFO*) &pluginInfo;
}


extern "C" __declspec(dllexport) PLUGININFOEX* MirandaPluginInfoEx(DWORD mirandaVersion)
{
	pluginInfo.cbSize = sizeof(PLUGININFOEX);
	return &pluginInfo;
}


static const MUUID interfaces[] = { MIID_STATUS_MESSAGE_RETRIEVER, MIID_LAST };
extern "C" __declspec(dllexport) const MUUID* MirandaPluginInterfaces(void)
{
	return interfaces;
}


extern "C" int __declspec(dllexport) Load(PLUGINLINK *link) {
	pluginLink = link;

	init_mir_malloc();
	init_list_interface();

	CreateServiceFunction(MS_SMR_DISABLE_CONTACT, DisableContactMsgRetrieval);
	CreateServiceFunction(MS_SMR_ENABLE_CONTACT, EnableContactMsgRetrieval);
	CreateServiceFunction(MS_SMR_ENABLED_FOR_PROTOCOL, MsgRetrievalEnabledForProtocol);
	CreateServiceFunction(MS_SMR_ENABLED_FOR_CONTACT, MsgRetrievalEnabledForUser);

	// Add menu item to enable/disable status message check
	CLISTMENUITEM mi = {0};
	mi.cbSize = sizeof(mi);
	mi.flags = 0;
	mi.pszPopupName = NULL;

	mi.position = 1000100020;
	mi.ptszName = TranslateT("Disable Status Message Check");
	mi.pszService = MS_SMR_DISABLE_CONTACT;
	hDisableMenu = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM)&mi);
	
	mi.position = 1000100020;
	mi.ptszName = TranslateT("Enable Status Message Check");
	mi.pszService = MS_SMR_ENABLE_CONTACT;
	hEnableMenu = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM)&mi);

	// hooks
	hModulesLoaded = HookEvent(ME_SYSTEM_MODULESLOADED, ModulesLoaded);

	// prebuild contact menu
	hPreBuildCMenu = HookEvent(ME_CLIST_PREBUILDCONTACTMENU, PreBuildContactMenu);

	return 0;
}

extern "C" int __declspec(dllexport) Unload(void) 
{
	FreeStatus();
	FreeStatusMsgs();
	FreePoll();
	DeInitOptions();

	UnhookEvent(hModulesLoaded);
	UnhookEvent(hPreBuildCMenu);
	return 0;
}


// Called when all the modules are loaded
int ModulesLoaded(WPARAM wParam, LPARAM lParam) 
{
	InitPoll();
	InitStatusMsgs();
	InitStatus();
	InitOptions();

	// add our modules to the KnownModules list
	CallService("DBEditorpp/RegisterSingleModule", (WPARAM) MODULE_NAME, 0);

    // updater plugin support
    if(ServiceExists(MS_UPDATE_REGISTER))
	{
		Update upd = {0};
		char szCurrentVersion[30];

		upd.cbSize = sizeof(upd);
		upd.szComponentName = pluginInfo.shortName;

		upd.szUpdateURL = UPDATER_AUTOREGISTER;

		upd.szBetaVersionURL = "http://pescuma.org/miranda/smr_version.txt";
		upd.szBetaChangelogURL = "http://pescuma.org/miranda/smr#Changelog";
		upd.pbBetaVersionPrefix = (BYTE *)"Status Message Retriever ";
		upd.cpbBetaVersionPrefix = strlen((char *)upd.pbBetaVersionPrefix);
		upd.szBetaUpdateURL = "http://pescuma.org/miranda/smr.zip";

		upd.pbVersion = (BYTE *)CreateVersionStringPlugin((PLUGININFO*) &pluginInfo, szCurrentVersion);
		upd.cpbVersion = strlen((char *)upd.pbVersion);

        CallService(MS_UPDATE_REGISTER, 0, (LPARAM)&upd);
	}

	return 0;
}


int EnableContactMsgRetrieval(WPARAM wParam,LPARAM lParam) 
{
	HANDLE hContact = (HANDLE) wParam;

	if (hContact != NULL)
		DBWriteContactSettingByte(hContact, MODULE_NAME, OPT_CONTACT_GETMSG, TRUE);

	return 0;
}


int DisableContactMsgRetrieval(WPARAM wParam,LPARAM lParam) 
{
	HANDLE hContact = (HANDLE) wParam;

	if (hContact != NULL)
		DBWriteContactSettingByte(hContact, MODULE_NAME, OPT_CONTACT_GETMSG, FALSE);

	return 0;
}


int PreBuildContactMenu(WPARAM wParam,LPARAM lParam) 
{
	HANDLE hContact = (HANDLE) wParam;
	char *proto = (char*)CallService(MS_PROTO_GETCONTACTBASEPROTO, wParam, 0);

	if (proto == NULL || !PollCheckProtocol(proto))
	{
		// Hide both

		CLISTMENUITEM clmi = {0};
		clmi.cbSize = sizeof(clmi);
		clmi.flags = CMIM_FLAGS | CMIF_HIDDEN;

		CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hEnableMenu, (LPARAM)&clmi);
		CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hDisableMenu, (LPARAM) &clmi);
	}
	else
	{
		// See what to show

		CLISTMENUITEM clmi = {0};
		clmi.cbSize = sizeof(clmi);

		if (DBGetContactSettingByte(hContact, MODULE_NAME, OPT_CONTACT_GETMSG, TRUE))
		{
			clmi.flags = CMIM_FLAGS | CMIF_HIDDEN;
			CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hEnableMenu, (LPARAM) &clmi);

			clmi.flags = CMIM_FLAGS | CMIM_ICON;
			clmi.hIcon = LoadSkinnedProtoIcon(proto, ID_STATUS_OFFLINE);
			CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hDisableMenu, (LPARAM) &clmi);
		}
		else
		{
			clmi.flags = CMIM_FLAGS | CMIF_HIDDEN;
			CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hDisableMenu, (LPARAM) &clmi);

			clmi.flags = CMIM_FLAGS | CMIM_ICON;
			clmi.hIcon = LoadSkinnedProtoIcon(proto, ID_STATUS_ONLINE);
			CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hEnableMenu, (LPARAM) &clmi);
		}
	}

	return 0;
}


/*
Return TRUE is smr is enabled for this contact and its protocol (smr can be disabled per user,
if protocol is enabled)
If is enabled, status message is kept under CList\StatusMsg db key in user data

wParam: hContact
lParam: ignored
*/
int MsgRetrievalEnabledForUser(WPARAM wParam, LPARAM lParam) 
{
	char *proto = (char*) CallService(MS_PROTO_GETCONTACTBASEPROTO, wParam, 0);

	return proto != NULL && PollCheckProtocol(proto) && PollCheckContact((HANDLE) wParam);
}


/*
Return TRUE is smr is enabled for this protocol
If is enabled, status message is kept under CList\StatusMsg db key in user data

wParam: protocol name
lParam: ignored
*/
int MsgRetrievalEnabledForProtocol(WPARAM wParam, LPARAM lParam) 
{
	char *proto = (char *) wParam;

	return proto != NULL && PollCheckProtocol(proto);
}

