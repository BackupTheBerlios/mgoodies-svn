/*
 * This code implements window handling (new mail)
 *
 * (c) majvan 2002-2004
 */
/* There can be problems when compiling this file, because in this file
 * we are using both unicode and no-unicode functions and compiler does not
 * like it in one file
 * When you got errors, try to comment the #define <stdio.h> and compile, then
 * put it back to uncommented and compile again :)
 */
#ifndef _WIN32_IE
	#define _WIN32_IE 0x0400
#endif
#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 0x0501
#endif

 
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
#undef UNICODE
#include <newpluginapi.h>
#include <m_utils.h>
#include <m_skin.h>
#include <m_langpack.h>
#include <m_database.h>
#include <m_clist.h>
#include "../include/m_popup.h"
#include "../include/m_kbdnotify.h"
#include "../main.h"
#include "../m_protoplugin.h"
#include "../m_account.h"
#include "../debug.h"
#include "../m_messages.h"
#include "../mails/m_mails.h"
#include "../m_yamn.h"
#include "../resources/resource.h"
#include <win2k.h>

#undef UNICODE
#include "m_browser.h"

#ifndef UNICODE
	#define UNICODE
	#define _UNICODE
	#include <commctrl.h>		//we need to have unicode commctrl.h
	#include <stdio.h>
	#undef _UNICODE
	#undef UNICODE
#else
	#include <commctrl.h>
	#undef _UNICODE
	#undef UNICODE
#endif

#define	TIMER_FLASHING 0x09061979
#define MAILBROWSER_MINXSIZE	200		//min size of mail browser window
#define MAILBROWSER_MINYSIZE	130

//- imported ---------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
extern char *ProtoName;
extern HYAMNPROTOPLUGIN POP3Plugin;

extern HANDLE hNewMailHook;
extern HANDLE WriteToFileEV;
extern YAMN_VARIABLES YAMNVar;
extern HICON hYamnIcon,hNewMailIcon,hNeutralIcon;
//From synchro.cpp
extern DWORD WINAPI WaitToWriteFcn(PSWMRG SObject,PSCOUNTER SCounter=NULL);
extern void WINAPI WriteDoneFcn(PSWMRG SObject,PSCOUNTER SCounter=NULL);
extern DWORD WINAPI WaitToReadFcn(PSWMRG SObject);
extern void WINAPI ReadDoneFcn(PSWMRG SObject);
extern DWORD WINAPI SCIncFcn(PSCOUNTER SCounter);
extern DWORD WINAPI SCDecFcn(PSCOUNTER SCounter);
//From mails.cpp
extern void WINAPI DeleteMessageFromQueueFcn(HYAMNMAIL *From,HYAMNMAIL Which,int mode);
extern void WINAPI SetRemoveFlagsInQueueFcn(HYAMNMAIL From,DWORD FlagsSet,DWORD FlagsNotSet,DWORD FlagsToSet,int mode);
//From mime.cpp
void ExtractHeader(struct CMimeItem *items,int CP,struct CHeader *head);
void ExtractShortHeader(struct CMimeItem *items,struct CShortHeader *head); 
void DeleteHeaderContent(struct CHeader *head);
//From account.cpp
void WINAPI GetStatusFcn(HACCOUNT Which,char *Value);

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
char* s_MonthNames[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
bool bDate = false,bSub=false,bSize=false,bFrom=false;
int PosX=0,PosY=0,SizeX=460,SizeY=100;
int HeadSizeX = 0x2b2, HeadSizeY = 0x0b5, HeadPosX = 100, HeadPosY = 100;
static int FromWidth=250,SubjectWidth=280,SizeWidth=50,SizeDate=205;

static WNDPROC OldListViewSubclassProc;

struct CMailNumbersSub
{
	int Total;		//any mail
	int New;			//uses YAMN_MSG_NEW flag
	int UnSeen;			//uses YAMN_MSG_UNSEEN flag
//	int Browser;		//uses YAMN_MSG_BROWSER flag
	int BrowserUC;		//uses YAMN_MSG_BROWSER flag and YAMN_MSG_UNSEEN flag
	int Display;		//uses YAMN_MSG_DISPLAY flag
	int DisplayTC;		//uses YAMN_MSG_DISPLAY flag and YAMN_MSG_DISPLAYC flag
	int DisplayUC;		//uses YAMN_MSG_DISPLAY flag and YAMN_MSG_DISPLAYC flag and YAMN_MSG_UNSEEN flag
	int PopUp;			//uses YAMN_MSG_POPUP flag
	int PopUpTC;		//uses YAMN_MSG_POPUPC flag
	int PopUpNC;		//uses YAMN_MSG_POPUPC flag and YAMN_MSG_NEW flag
	int PopUpRun;		//uses YAMN_MSG_POPUP flag and YAMN_MSG_NEW flag
	int PopUpSL2NC;		//uses YAMN_MSG_SPAML2 flag and YAMN_MSG_NEW flag
	int PopUpSL3NC;		//uses YAMN_MSG_SPAML3 flag and YAMN_MSG_NEW flag
//	int SysTray;		//uses YAMN_MSG_SYSTRAY flag
	int SysTrayUC;		//uses YAMN_MSG_SYSTRAY flag and YAMN_MSG_UNSEEN flag
//	int Sound;		//uses YAMN_MSG_SOUND flag
	int SoundNC;		//uses YAMN_MSG_SOUND flag and YAMN_MSG_NEW flag
//	int App;		//uses YAMN_MSG_APP flag
	int AppNC;		//uses YAMN_MSG_APP flag and YAMN_MSG_NEW flag
	int EventNC;		//uses YAMN_MSG_NEVENT flag and YAMN_MSG_NEW flag
};

struct CMailNumbers
{
	struct CMailNumbersSub Real;
	struct CMailNumbersSub Virtual;
};

struct CMailWinUserInfo
{
	HACCOUNT Account;
	int TrayIconState;
	BOOL UpdateMailsMessagesAccess;
	BOOL Seen;
	BOOL RunFirstTime;
};

struct CChangeContent
{
	DWORD nflags;
	DWORD nnflags;
};

struct CUpdateMails
{
	struct CChangeContent *Flags;
	BOOL Waiting;
	HANDLE Copied;
};
struct CSortList
{
	HWND hDlg;
	int	iSubItem;
};

//Retrieves HACCOUNT, whose mails are displayed in ListMails
// hLM- handle of dialog window
// returns handle of account
inline HACCOUNT GetWindowAccount(HWND hDialog);

//Looks to mail flags and increment mail counter (e.g. if mail is new, increments the new mail counter
// msgq- mail, which increments the counters
// MN- counnters structure
void IncrementMailCounters(HYAMNMAIL msgq,struct CMailNumbers *MN);

enum
{
	UPDATE_FAIL=0,		//function failed
	UPDATE_NONE,		//none update has been performed
	UPDATE_OK,		//some changes occured, update performed
};
//Just looks for mail changes in account and update the mail browser window
// hDlg- dialog handle
// ActualAccount- account handle
// nflags- flags what to do when new mail arrives
// nnflags- flags what to do when no new mail arrives
// returns one of UPDATE_XXX value(not implemented yet)
int UpdateMails(HWND hDlg,HACCOUNT ActualAccount,DWORD nflags,DWORD nnflags);

//When new mail occurs, shows window, plays sound, runs application...
// hDlg- dialog handle. Dialog of mailbrowser is already created and actions are performed over this window
// ActualAccount- handle of account, whose mails are to be notified
// MN- statistics of mails in account
// nflags- what to do or not to do (e.g. to show mailbrowser window or prohibit to show)
// nflags- flags what to do when new mail arrives
// nnflags- flags what to do when no new mail arrives
void DoMailActions(HWND hDlg,HACCOUNT ActualAccount,struct CMailNumbers *MN,DWORD nflags,DWORD nnflags);

//Looks for items in mailbrowser and if they were deleted, delete them from browser window
// hListView- handle of listview window
// ActualAccount- handle of account, whose mails are show
// MailNumbers- pointer to structure, in which function stores numbers of mails with some property
// returns one of UPDATE_XXX value (not implemented yet)
int ChangeExistingMailStatus(HWND hListView,HACCOUNT ActualAccount,struct CMailNumbers *MN);

//Adds new mails to ListView and if any new, shows multi popup (every new message is new popup window created by popup plugin)
// hListView- handle of listview window
// ActualAccount- handle of account, whose mails are show
// NewMailPopUp- pointer to prepared structure for popup plugin, can be NULL if no popup show
// MailNumbers- pointer to structure, in which function stores numbers of mails with some property
// nflags- flags what to do when new mail arrives
// returns one of UPDATE_XXX value (not implemented yet)
int AddNewMailsToListView(HWND hListView,HACCOUNT ActualAccount,struct CMailNumbers *MailNumbers,DWORD nflags);

//Window callback procedure for popup window (created by popup plugin)
LRESULT CALLBACK NewMailPopUpProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam);

//Window callback procedure for popup window (created by popup plugin)
LRESULT CALLBACK NoNewMailPopUpProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam);

//Dialog callback procedure for mail browser
BOOL CALLBACK DlgProcYAMNMailBrowser(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam);

//MailBrowser thread function creates window if needed, tray icon and plays sound
DWORD WINAPI MailBrowser(LPVOID Param);

LRESULT CALLBACK ListViewSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

//Runs mail browser in new thread
int RunMailBrowserSvc(WPARAM,LPARAM);

#define	YAMN_BROWSER_SHOWPOPUP	0x01

	//	list view items' order criteria
	#define LVORDER_NOORDER		-1
	#define LVORDER_STRING		 0
	#define LVORDER_NUMERIC		 1
	#define LVORDER_DATETIME	 2

	//	list view order direction
	#define LVORDER_ASCENDING	 1
	#define LVORDER_NONE		 0
	#define LVORDER_DESCENDING	-1

	//	list view sort type
	#define LVSORTPRIORITY_NONE -1

	//	List view column info.
	typedef struct _SAMPLELISTVIEWCOLUMN
	{
		UINT	 uCXCol;		//	index
		int		 nSortType;		//	sorting type (STRING = 0, NUMERIC, DATE, DATETIME)
		int		 nSortOrder;	//	sorting order (ASCENDING = -1, NONE, DESCENDING)
		int		 nPriority;		//	sort priority (-1 for none, 0, 1, ..., nColumns - 1 maximum)
		TCHAR lpszName[128];	//	column name
	} SAMPLELISTVIEWCOLUMN;

	//	Compare priority
	typedef struct _LVCOMPAREINFO
	{
		int	iIdx;				//	Index
		int iPriority;			//	Priority
	} LVCOMPAREINFO, *LPLVCOMPAREINFO;

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

LPARAM readItemLParam(HWND hwnd,DWORD iItem)
{
	LVITEM item;

	item.mask = LVIF_PARAM;
	item.iItem = iItem;
	item.iSubItem = 0;
	SendMessage(hwnd,LVM_GETITEM,0,(LPARAM)&item);
	return item.lParam;
}

inline HACCOUNT GetWindowAccount(HWND hDlg)
{
	struct CMailWinUserInfo *mwui;

	if(NULL==(mwui=(struct CMailWinUserInfo *)GetWindowLong(hDlg,DWL_USER)))
		return NULL;
	return mwui->Account;
}

void IncrementMailCounters(HYAMNMAIL msgq,struct CMailNumbers *MN)
{
	if(msgq->Flags & YAMN_MSG_VIRTUAL)
		MN->Virtual.Total++;
	else
		MN->Real.Total++;

	if(msgq->Flags & YAMN_MSG_NEW)
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.New++;
		else
			MN->Real.New++;
	if(msgq->Flags & YAMN_MSG_UNSEEN)
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.UnSeen++;
		else
			MN->Real.UnSeen++;
	if((msgq->Flags & (YAMN_MSG_UNSEEN | YAMN_MSG_BROWSER)) == (YAMN_MSG_UNSEEN | YAMN_MSG_BROWSER))
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.BrowserUC++;
		else
			MN->Real.BrowserUC++;
	if(msgq->Flags & YAMN_MSG_DISPLAY)
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.Display++;
		else
			MN->Real.Display++;
	if((msgq->Flags & (YAMN_MSG_DISPLAYC | YAMN_MSG_DISPLAY)) == (YAMN_MSG_DISPLAYC | YAMN_MSG_DISPLAY))
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.DisplayTC++;
		else
			MN->Real.DisplayTC++;
	if((msgq->Flags & (YAMN_MSG_UNSEEN | YAMN_MSG_DISPLAYC | YAMN_MSG_DISPLAY)) == (YAMN_MSG_UNSEEN | YAMN_MSG_DISPLAYC | YAMN_MSG_DISPLAY))
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.DisplayUC++;
		else
			MN->Real.DisplayUC++;
	if(msgq->Flags & YAMN_MSG_POPUP)
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.PopUp++;
		else
			MN->Real.PopUp++;
	if((msgq->Flags & YAMN_MSG_POPUPC) == YAMN_MSG_POPUPC)
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.PopUpTC++;
		else
			MN->Real.PopUpTC++;
	if((msgq->Flags & (YAMN_MSG_NEW | YAMN_MSG_POPUPC)) == (YAMN_MSG_NEW | YAMN_MSG_POPUPC))
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.PopUpNC++;
		else
			MN->Real.PopUpNC++;
	if((msgq->Flags & (YAMN_MSG_NEW | YAMN_MSG_POPUP)) == (YAMN_MSG_NEW | YAMN_MSG_POPUP))
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.PopUpRun++;
		else
			MN->Real.PopUpRun++;
	if((msgq->Flags & YAMN_MSG_NEW) && YAMN_MSG_SPAML(msgq->Flags,YAMN_MSG_SPAML2))
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.PopUpSL2NC++;
		else
			MN->Real.PopUpSL2NC++;
	if((msgq->Flags & YAMN_MSG_NEW) && YAMN_MSG_SPAML(msgq->Flags,YAMN_MSG_SPAML3))
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.PopUpSL3NC++;
		else
			MN->Real.PopUpSL3NC++;
/*	if(msgq->MailData->Flags & YAMN_MSG_SYSTRAY)
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.SysTray++;
		else
			MN->Real.SysTray++;
*/	if((msgq->Flags & (YAMN_MSG_UNSEEN | YAMN_MSG_SYSTRAY)) == (YAMN_MSG_UNSEEN|YAMN_MSG_SYSTRAY))
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.SysTrayUC++;
		else
			MN->Real.SysTrayUC++;
/*	if(msgq->MailData->Flags & YAMN_MSG_SOUND)
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.Sound++;
		else
			MN->Real.Sound++;
*/	if((msgq->Flags & (YAMN_MSG_NEW|YAMN_MSG_SOUND)) == (YAMN_MSG_NEW|YAMN_MSG_SOUND))
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.SoundNC++;
		else
			MN->Real.SoundNC++;
/*	if(msgq->MailData->Flags & YAMN_MSG_APP)
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.App++;
		else
			MN->Real.App++;
*/	if((msgq->Flags & (YAMN_MSG_NEW|YAMN_MSG_APP)) == (YAMN_MSG_NEW|YAMN_MSG_APP))
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.AppNC++;
		else
			MN->Real.AppNC++;
	if((msgq->Flags & (YAMN_MSG_NEW|YAMN_MSG_NEVENT)) == (YAMN_MSG_NEW|YAMN_MSG_NEVENT))
		if(msgq->Flags & YAMN_MSG_VIRTUAL)
			MN->Virtual.EventNC++;
		else
			MN->Real.EventNC++;
}

int UpdateMails(HWND hDlg,HACCOUNT ActualAccount,DWORD nflags,DWORD nnflags)
{
#define MAILBROWSERTITLE "%s - %d new mails, %d total"

	struct CMailWinUserInfo *mwui;
	struct CMailNumbers MN;

	HYAMNMAIL msgq;
	BOOL Loaded;
	BOOL RunMailBrowser,RunPopUps;

	mwui=(struct CMailWinUserInfo *)GetWindowLong(hDlg,DWL_USER);
	//now we ensure read access for account and write access for its mails
	#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"UpdateMails:ActualAccountSO-read wait\n");
	#endif
	if(WAIT_OBJECT_0!=WaitToReadFcn(ActualAccount->AccountAccessSO))
	{
		#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"UpdateMails:ActualAccountSO-read wait failed\n");
		#endif
		PostMessage(hDlg,WM_DESTROY,(WPARAM)0,(LPARAM)0);

		return UPDATE_FAIL;
	}
	#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"UpdateMails:ActualAccountSO-read enter\n");
	#endif

	#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"UpdateMails:ActualAccountMsgsSO-write wait\n");
	#endif
	if(WAIT_OBJECT_0!=WaitToWriteFcn(ActualAccount->MessagesAccessSO))
	{
		#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"UpdateMails:ActualAccountMsgsSO-write wait failed\n");
		DebugLog(SynchroFile,"UpdateMails:ActualAccountSO-read done\n");
		#endif
		ReadDoneFcn(ActualAccount->AccountAccessSO);

		PostMessage(hDlg,WM_DESTROY,(WPARAM)0,(LPARAM)0);
		return UPDATE_FAIL;
	}
	#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"UpdateMails:ActualAccountMsgsSO-write enter\n");
	#endif

	ZeroMemory(&MN,sizeof(MN));

	for(msgq=(HYAMNMAIL)ActualAccount->Mails;msgq!=NULL;msgq=msgq->Next)
	{
		if(!LoadedMailData(msgq))				//check if mail is already in memory
		{
			Loaded=false;
			if(NULL==LoadMailData(msgq))			//if we could not load mail to memory, consider this mail deleted and do not display it
				continue;
		}
		else
			Loaded=true;

		IncrementMailCounters(msgq,&MN);

		if(!Loaded)
			UnloadMailData(msgq);			//do not keep data for mail in memory
	}

	if(mwui!=NULL)
		mwui->UpdateMailsMessagesAccess=TRUE;

	//Now we are going to check if extracting data from mail headers are needed. If popups will be displayed or mailbrowser window
	if((((mwui!=NULL) && !(mwui->RunFirstTime)) && (((nnflags & YAMN_ACC_MSGP) && !(MN.Real.BrowserUC+MN.Virtual.BrowserUC)) || ((nflags & YAMN_ACC_MSGP) && (MN.Real.BrowserUC+MN.Virtual.BrowserUC)))) ||		//if mail window was displayed before and flag YAMN_ACC_MSGP is set
		((nnflags & YAMN_ACC_MSG) && !(MN.Real.BrowserUC+MN.Virtual.BrowserUC)) ||	//if needed to run mailbrowser when no unseen and no unseen mail found
		((nflags & YAMN_ACC_MSG) && (MN.Real.BrowserUC+MN.Virtual.BrowserUC)) ||	//if unseen mails found, we sure run mailbrowser
		((nflags & YAMN_ACC_ICO) && (MN.Real.SysTrayUC+MN.Virtual.SysTrayUC)))		//if needed to run systray
		RunMailBrowser=TRUE;
	else
		RunMailBrowser=FALSE;

	if(((nflags & YAMN_ACC_POP) && (ActualAccount->Flags & YAMN_ACC_POPN) && (MN.Real.PopUpNC+MN.Virtual.PopUpNC)))	//if some popups with mails are needed to show
		RunPopUps=TRUE;
	else
		RunPopUps=FALSE;

	if(RunMailBrowser)
		ChangeExistingMailStatus(GetDlgItem(hDlg,IDC_LISTMAILS),ActualAccount,&MN);
	if(RunMailBrowser || RunPopUps)
		AddNewMailsToListView(hDlg==NULL ? NULL : GetDlgItem(hDlg,IDC_LISTMAILS),ActualAccount,&MN,nflags);

	if(RunMailBrowser)
	{
		WCHAR *TitleStrW;
		char *TitleStrA;

		TitleStrA=new char[strlen(ActualAccount->Name)+strlen(Translate(MAILBROWSERTITLE))+10];	//+10 chars for numbers
		TitleStrW=new WCHAR[strlen(ActualAccount->Name)+strlen(Translate(MAILBROWSERTITLE))+10];	//+10 chars for numbers

		sprintf(TitleStrA,Translate(MAILBROWSERTITLE),ActualAccount->Name,MN.Real.DisplayUC+MN.Virtual.DisplayUC,MN.Real.Display+MN.Virtual.Display);
		MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,TitleStrA,-1,TitleStrW,strlen(TitleStrA)+1);
		SendMessageW(hDlg,WM_SETTEXT,(WPARAM)0,(LPARAM)TitleStrW);
		delete[] TitleStrA;
		delete[] TitleStrW;
	}

	#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"UpdateMails:Do mail actions\n");
	#endif
	DoMailActions(hDlg,ActualAccount,&MN,nflags,nnflags);
	#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"UpdateMails:Do mail actions done\n");
	#endif
	
	SetRemoveFlagsInQueueFcn((HYAMNMAIL)ActualAccount->Mails,YAMN_MSG_NEW,0,YAMN_MSG_NEW,YAMN_FLAG_REMOVE);				//rempve the new flag
	if(!RunMailBrowser)
		SetRemoveFlagsInQueueFcn((HYAMNMAIL)ActualAccount->Mails,YAMN_MSG_UNSEEN,YAMN_MSG_STAYUNSEEN,YAMN_MSG_UNSEEN,YAMN_FLAG_REMOVE);	//remove the unseen flag when it was not displayed and it has not "stay unseen" flag set

	if(mwui!=NULL)
	{
		mwui->UpdateMailsMessagesAccess=FALSE;
		mwui->RunFirstTime=FALSE;
	}
	#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"UpdateMails:ActualAccountMsgsSO-write done\n");
	DebugLog(SynchroFile,"UpdateMails:ActualAccountSO-read done\n");
	#endif
	WriteDoneFcn(ActualAccount->MessagesAccessSO);
	ReadDoneFcn(ActualAccount->AccountAccessSO);

	if(RunMailBrowser)
		UpdateWindow(GetDlgItem(hDlg,IDC_LISTMAILS));
	else if(hDlg!=NULL)
		DestroyWindow(hDlg);

	return 1;
}

int ChangeExistingMailStatus(HWND hListView,HACCOUNT ActualAccount,struct CMailNumbers *MN)
{
	int i,in;
	LVITEMW item;
	HYAMNMAIL mail,msgq;

	in=ListView_GetItemCount(hListView);
	item.mask=LVIF_PARAM;

	for(i=0;i<in;i++)
	{
		item.iItem=i;
		item.iSubItem=0;
		if(TRUE==ListView_GetItem(hListView,&item))
			mail=(HYAMNMAIL)item.lParam;
		else
			continue;
		for(msgq=(HYAMNMAIL)ActualAccount->Mails;(msgq!=NULL)&&(msgq!=mail);msgq=msgq->Next);	//found the same mail in account queue
		if(msgq==NULL)		//if mail was not found
			if(TRUE==ListView_DeleteItem(hListView,i))
			{
				in--;i--;
				continue;
			}
	}

	return TRUE;
}

void MimeDateToLocalizedDateTime(char *datein, WCHAR *dateout, int lendateout);
int AddNewMailsToListView(HWND hListView,HACCOUNT ActualAccount,struct CMailNumbers *MN,DWORD nflags)
{
	HYAMNMAIL msgq;
	POPUPDATAEX NewMailPopUp;

	WCHAR *FromStr;
	WCHAR SizeStr[20];
	WCHAR LocalDateStr[128];

	LVITEMW item;
	LVFINDINFO fi; 

	int foundi,lfoundi;
	struct CHeader UnicodeHeader;
	BOOL Loaded,Extracted,FromStrNew=FALSE;

	ZeroMemory(&item,sizeof(item));
	ZeroMemory(&UnicodeHeader,sizeof(UnicodeHeader));

	if(hListView!=NULL)
	{
		item.mask=LVIF_TEXT | LVIF_PARAM;
		item.iItem=0;
		ZeroMemory(&fi,sizeof(fi));
		fi.flags=LVFI_PARAM;						//let's go search item by lParam number
		lfoundi=0;
	}

	NewMailPopUp.lchContact=(ActualAccount->hContact != NULL) ? ActualAccount->hContact : ActualAccount;
	NewMailPopUp.lchIcon=hNewMailIcon;
	NewMailPopUp.colorBack=nflags & YAMN_ACC_POPC ? ActualAccount->NewMailN.PopUpB : GetSysColor(COLOR_BTNFACE);
	NewMailPopUp.colorText=nflags & YAMN_ACC_POPC ? ActualAccount->NewMailN.PopUpT : GetSysColor(COLOR_WINDOWTEXT);
	NewMailPopUp.iSeconds=ActualAccount->NewMailN.PopUpTime;

	NewMailPopUp.PluginWindowProc=(WNDPROC)NewMailPopUpProc;
	NewMailPopUp.PluginData=(void *)1;					//it's new mail popup

	for(msgq=(HYAMNMAIL)ActualAccount->Mails;msgq!=NULL;msgq=msgq->Next,lfoundi++)
	{
//		now we hide mail pointer to item's lParam member. We can later use it to retrieve mail datas

		Extracted=FALSE;FromStr=NULL;FromStrNew=FALSE;

		if(hListView!=NULL)
		{
			fi.lParam=(LPARAM)msgq;
			if(-1!=(foundi=ListView_FindItem(hListView,-1,&fi)))	//if mail is already in window
			{
				lfoundi=foundi;
				continue;					//do not insert any item
			}

			item.iItem=lfoundi;			//insert after last found item
			item.lParam=(LPARAM)msgq;
		}

		if(!LoadedMailData(msgq))				//check if mail is already in memory
		{
			Loaded=false;
			if(NULL==LoadMailData(msgq))			//if we could not load mail to memory, consider this mail deleted and do not display it
				continue;
		}
		else
			Loaded=true;

		if(((hListView!=NULL) && (msgq->Flags & YAMN_MSG_DISPLAY)) ||
			((nflags & YAMN_ACC_POP) && (ActualAccount->Flags & YAMN_ACC_POPN) && (msgq->Flags & YAMN_MSG_POPUP) && (msgq->Flags & YAMN_MSG_NEW)))
		{

			if(!Extracted) ExtractHeader(msgq->MailData->TranslatedHeader,msgq->MailData->CP,&UnicodeHeader);
			Extracted=TRUE;

			if((UnicodeHeader.From!=NULL) && (UnicodeHeader.FromNick!=NULL))
			{
				FromStr=new WCHAR[wcslen(UnicodeHeader.From)+wcslen(UnicodeHeader.FromNick)+4];
				swprintf(FromStr,L"%s <%s>",UnicodeHeader.FromNick,UnicodeHeader.From);
				FromStrNew=TRUE;
			}
			else if(UnicodeHeader.From!=NULL)
				FromStr=UnicodeHeader.From;
			else if(UnicodeHeader.FromNick!=NULL)
				FromStr=UnicodeHeader.FromNick;
			else if(UnicodeHeader.ReturnPath!=NULL)
				FromStr=UnicodeHeader.ReturnPath;

			if(NULL==FromStr)
			{
				FromStr=L"";
				FromStrNew=FALSE;
			}
		}


		if((hListView!=NULL) && (msgq->Flags & YAMN_MSG_DISPLAY))
		{
			item.iSubItem=0;
			item.pszText=FromStr;
			item.iItem=SendMessageW(hListView,LVM_INSERTITEMW,(WPARAM)0,(LPARAM)&item);

			item.iSubItem=1;
			item.pszText=(NULL!=UnicodeHeader.Subject ? UnicodeHeader.Subject : (WCHAR*)L"");
			SendMessageW(hListView,LVM_SETITEMTEXTW,(WPARAM)item.iItem,(LPARAM)&item);

			item.iSubItem=2;
			swprintf(SizeStr,L"%d kB",msgq->MailData->Size/1024);
			item.pszText=SizeStr;
			SendMessageW(hListView,LVM_SETITEMTEXTW,(WPARAM)item.iItem,(LPARAM)&item);

			item.iSubItem=3;
			item.pszText=L"";
			{	CMimeItem *heads;
				for(heads=msgq->MailData->TranslatedHeader;heads!=NULL;heads=heads->Next)	{
					if (!_stricmp(heads->name,"Date")){ 
						MimeDateToLocalizedDateTime(heads->value,LocalDateStr,128);
						item.pszText=LocalDateStr;
						break;
			}	}	}
			SendMessageW(hListView,LVM_SETITEMTEXTW,(WPARAM)item.iItem,(LPARAM)&item);
		}

		if((nflags & YAMN_ACC_POP) && (ActualAccount->Flags & YAMN_ACC_POPN) && (msgq->Flags & YAMN_MSG_POPUP) && (msgq->Flags & YAMN_MSG_NEW))
		{
			WideCharToMultiByte(CP_ACP,0,FromStr,-1,(char *)NewMailPopUp.lpzContactName,sizeof(NewMailPopUp.lpzContactName),NULL,NULL);
			if(!WideCharToMultiByte(CP_ACP,0,UnicodeHeader.Subject,-1,(char *)NewMailPopUp.lpzText,sizeof(NewMailPopUp.lpzText),NULL,NULL))
				NewMailPopUp.lpzText[0]=0;
			CallService(MS_POPUP_ADDPOPUPEX,(WPARAM)&NewMailPopUp,0);
		}

		if((msgq->Flags & YAMN_MSG_UNSEEN) && (ActualAccount->NewMailN.Flags & YAMN_ACC_KBN))
			CallService(MS_KBDNOTIFY_EVENTSOPENED,(WPARAM)1,NULL);
		
		if(FromStrNew)
			delete[] FromStr;

		if(Extracted)
		{
			DeleteHeaderContent(&UnicodeHeader);
			ZeroMemory(&UnicodeHeader,sizeof(UnicodeHeader));
		}

		if(!Loaded)
		{
			SaveMailData(msgq);
			UnloadMailData(msgq);			//do not keep data for mail in memory
		}
	}

	return TRUE;
}

void DoMailActions(HWND hDlg,HACCOUNT ActualAccount,struct CMailNumbers *MN,DWORD nflags,DWORD nnflags)
{
	TCHAR *NotIconText=Translate("- new mail(s)");
	NOTIFYICONDATA nid;

	ZeroMemory(&nid,sizeof(nid));

	if(MN->Real.EventNC+MN->Virtual.EventNC)
		NotifyEventHooks(hNewMailHook,0,0);

	if((nflags & YAMN_ACC_KBN) && (MN->Real.PopUpRun+MN->Virtual.PopUpRun))
	{
		CallService(MS_KBDNOTIFY_STARTBLINK,(WPARAM)MN->Real.PopUpNC+MN->Virtual.PopUpNC,NULL);
	}

	if((nflags & YAMN_ACC_CONT) && (MN->Real.PopUpRun+MN->Virtual.PopUpRun))
	{
		char sMsg[250];
		CLISTEVENT cEvent;
		cEvent.cbSize = sizeof(CLISTEVENT);
		cEvent.hContact = ActualAccount->hContact;
		cEvent.hIcon = hNewMailIcon;
		cEvent.hDbEvent = (HANDLE)"yamn new mail";
		cEvent.lParam = (LPARAM) ActualAccount->hContact;
		cEvent.pszService = MS_YAMN_CLISTDBLCLICK;
		cEvent.pszTooltip = new char[250];

		sprintf(cEvent.pszTooltip,Translate("%s : %d new mail(s), %d total"),ActualAccount->Name,MN->Real.PopUpNC+MN->Virtual.PopUpNC,MN->Real.PopUpTC+MN->Virtual.PopUpTC);
		CallServiceSync(MS_CLIST_ADDEVENT, 0,(LPARAM)&cEvent);
		
		sprintf(sMsg,Translate("%d new mail(s), %d total"),MN->Real.PopUpNC+MN->Virtual.PopUpNC,MN->Real.PopUpTC+MN->Virtual.PopUpTC);
		DBWriteContactSettingString(ActualAccount->hContact, "CList", "StatusMsg", sMsg);
		
		if(nflags & YAMN_ACC_CONTNICK)
		{
			DBWriteContactSettingString(ActualAccount->hContact, ProtoName, "Nick", cEvent.pszTooltip);
		}
	}

	if((nflags & YAMN_ACC_POP) && !(ActualAccount->Flags & YAMN_ACC_POPN) && (MN->Real.PopUpRun+MN->Virtual.PopUpRun))
	{
		POPUPDATAEX NewMailPopUp;

		NewMailPopUp.lchContact=(ActualAccount->hContact != NULL) ? ActualAccount->hContact : ActualAccount;
		NewMailPopUp.lchIcon=hNewMailIcon;
		NewMailPopUp.colorBack=nflags & YAMN_ACC_POPC ? ActualAccount->NewMailN.PopUpB : GetSysColor(COLOR_BTNFACE);
		NewMailPopUp.colorText=nflags & YAMN_ACC_POPC ? ActualAccount->NewMailN.PopUpT : GetSysColor(COLOR_WINDOWTEXT);
		NewMailPopUp.iSeconds=ActualAccount->NewMailN.PopUpTime;

		NewMailPopUp.PluginWindowProc=(WNDPROC)NewMailPopUpProc;
		NewMailPopUp.PluginData=(void *)1;					//it's new mail popup

		lstrcpyn(NewMailPopUp.lpzContactName,ActualAccount->Name,sizeof(NewMailPopUp.lpzContactName));
		sprintf(NewMailPopUp.lpzText,Translate("%d new mail(s), %d total"),MN->Real.PopUpNC+MN->Virtual.PopUpNC,MN->Real.PopUpTC+MN->Virtual.PopUpTC);
		CallService(MS_POPUP_ADDPOPUPEX,(WPARAM)&NewMailPopUp,0);
	}

	if((MN->Real.SysTrayUC+MN->Virtual.SysTrayUC==0) && (hDlg!=NULL))		//destroy tray icon if no new mail
	{
		nid.hWnd=hDlg;
		nid.uID=0;
		Shell_NotifyIcon(NIM_DELETE,&nid);
	}

	if((MN->Real.BrowserUC+MN->Virtual.BrowserUC==0) && (hDlg!=NULL))		
	{
		if(!IsWindowVisible(hDlg) && !(nflags & YAMN_ACC_MSG))
			PostMessage(hDlg,WM_DESTROY,(WPARAM)0,(LPARAM)0);				//destroy window if no new mail and window is not visible
		if(nnflags & YAMN_ACC_MSG)											//if no new mail and msg should be executed
		{
			SetForegroundWindow(hDlg);
			ShowWindow(hDlg,SW_SHOWNORMAL);
		}
	}
	else 
		if(hDlg!=NULL)								//else insert icon and set window if new mails
		{
			SendMessageW(GetDlgItem(hDlg,IDC_LISTMAILS),LVM_SCROLL,(WPARAM)0,(LPARAM)0x7ffffff);

			if((nflags & YAMN_ACC_ICO) && (MN->Real.SysTrayUC+MN->Virtual.SysTrayUC))
			{
				TCHAR *src,*dest;
				int i;

				for(src=ActualAccount->Name,dest=nid.szTip,i=0;(*src!=(TCHAR)0) && (i+1<sizeof(nid.szTip));*dest++=*src++);
				for(src=NotIconText;(*src!=(TCHAR)0) && (i+1<sizeof(nid.szTip));*dest++=*src++);
				*dest=(TCHAR)0;
				nid.cbSize=sizeof(NOTIFYICONDATA);
				nid.hWnd=hDlg;
				nid.hIcon=hNewMailIcon;
				nid.uID=0;
				nid.uFlags=NIF_ICON | NIF_MESSAGE | NIF_TIP;
				nid.uCallbackMessage=WM_YAMN_NOTIFYICON;
				Shell_NotifyIcon(NIM_ADD,&nid);
				SetTimer(hDlg,TIMER_FLASHING,500,NULL);
			}
			if(nflags & YAMN_ACC_MSG)											//if no new mail and msg should be executed
				ShowWindow(hDlg,SW_SHOWNORMAL);
		}

	if(MN->Real.AppNC+MN->Virtual.AppNC!=0)
	{
		if(nflags & YAMN_ACC_APP)
		{
			PROCESS_INFORMATION pi;
			STARTUPINFOW si;
			ZeroMemory(&si,sizeof(si));
			si.cb=sizeof(si);

			if(ActualAccount->NewMailN.App!=NULL)
			{
				WCHAR *Command;
				if(ActualAccount->NewMailN.AppParam!=NULL)
					Command=new WCHAR[wcslen(ActualAccount->NewMailN.App)+wcslen(ActualAccount->NewMailN.AppParam)+6];
				else
					Command=new WCHAR[wcslen(ActualAccount->NewMailN.App)+6];
		
				if(Command!=NULL)
				{
					lstrcpyW(Command,L"\"");
					lstrcatW(Command,ActualAccount->NewMailN.App);
					lstrcatW(Command,L"\" ");
					if(ActualAccount->NewMailN.AppParam!=NULL)
						lstrcatW(Command,ActualAccount->NewMailN.AppParam);
					CreateProcessW(NULL,Command,NULL,NULL,FALSE,NORMAL_PRIORITY_CLASS,NULL,NULL,&si,&pi);
					delete[] Command;
				}
			}
		}
	}

	if(MN->Real.SoundNC+MN->Virtual.SoundNC!=0)
		if(nflags & YAMN_ACC_SND)
			CallService(MS_SKIN_PLAYSOUND,0,(LPARAM)YAMN_NEWMAILSOUND);

	if((nnflags & YAMN_ACC_POP) && (MN->Real.PopUpRun+MN->Virtual.PopUpRun==0))
	{
		POPUPDATAEX NoNewMailPopUp;

		NoNewMailPopUp.lchContact=(ActualAccount->hContact != NULL) ? ActualAccount->hContact : ActualAccount;
		NoNewMailPopUp.lchIcon=hYamnIcon;
		NoNewMailPopUp.colorBack=ActualAccount->NoNewMailN.Flags & YAMN_ACC_POPC ? ActualAccount->NoNewMailN.PopUpB : GetSysColor(COLOR_BTNFACE);
		NoNewMailPopUp.colorText=ActualAccount->NoNewMailN.Flags & YAMN_ACC_POPC ? ActualAccount->NoNewMailN.PopUpT : GetSysColor(COLOR_WINDOWTEXT);
		NoNewMailPopUp.iSeconds=ActualAccount->NoNewMailN.PopUpTime;

		NoNewMailPopUp.PluginWindowProc=(WNDPROC)NoNewMailPopUpProc;
		NoNewMailPopUp.PluginData=(void *)0;					//it's not new mail popup

		lstrcpyn(NoNewMailPopUp.lpzContactName,ActualAccount->Name,sizeof(NoNewMailPopUp.lpzContactName));
		if(MN->Real.PopUpSL2NC+MN->Virtual.PopUpSL2NC)
			sprintf(NoNewMailPopUp.lpzText,Translate("No new mail, %d spam(s)"),MN->Real.PopUpSL2NC+MN->Virtual.PopUpSL2NC);
		else
			lstrcpyn(NoNewMailPopUp.lpzText,Translate("No new mail"),sizeof(NoNewMailPopUp.lpzText));
		CallService(MS_POPUP_ADDPOPUPEX,(WPARAM)&NoNewMailPopUp,0);
	}

	if((nflags & YAMN_ACC_CONT) && (MN->Real.PopUpRun+MN->Virtual.PopUpRun==0))
	{
		if(ActualAccount->hContact != NULL)
		{
			if(MN->Real.PopUpTC+MN->Virtual.PopUpTC)
			{
				char tmp[255];
				sprintf(tmp,Translate("%d new mail(s), %d total"),MN->Real.PopUpNC+MN->Virtual.PopUpNC,MN->Real.PopUpTC+MN->Virtual.PopUpTC);
				DBWriteContactSettingString(ActualAccount->hContact, "CList", "StatusMsg", tmp);
			}
			else
				DBWriteContactSettingString(ActualAccount->hContact, "CList", "StatusMsg", Translate("No new mail"));

			if(nflags & YAMN_ACC_CONTNICK)
			{
				DBWriteContactSettingString(ActualAccount->hContact, ProtoName, "Nick", ActualAccount->Name);
			}
		}
	}
	return;
}

LRESULT CALLBACK NewMailPopUpProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) 
{
	DWORD PluginParam;
	switch(msg)
	{
		case WM_COMMAND:
			if((HIWORD(wParam)==STN_CLICKED) && (msg==WM_COMMAND) && (CallService(MS_POPUP_GETPLUGINDATA,(WPARAM)hWnd,(LPARAM)&PluginParam)))	//if clicked and it's new mail popup window
			{
				HACCOUNT ActualAccount;
				HANDLE hContact;
				DBVARIANT dbv;

				hContact=(HANDLE)CallService(MS_POPUP_GETCONTACT,(WPARAM)hWnd,(LPARAM)0);

				if(!DBGetContactSetting((HANDLE) hContact,ProtoName,"Id",&dbv)) 
				{
					ActualAccount=(HACCOUNT) CallService(MS_YAMN_FINDACCOUNTBYNAME,(WPARAM)POP3Plugin,(LPARAM)dbv.pszVal);
					DBFreeVariant(&dbv);
				}
				else
					ActualAccount = (HACCOUNT) hContact;


				#ifdef DEBUG_SYNCHRO
				DebugLog(SynchroFile,"PopUpProc:LEFTCLICK:ActualAccountSO-read wait\n");
				#endif
				if(WAIT_OBJECT_0==WaitToReadFcn(ActualAccount->AccountAccessSO))
				{
					#ifdef DEBUG_SYNCHRO
					DebugLog(SynchroFile,"PopUpProc:LEFTCLICK:ActualAccountSO-read enter\n");
					#endif
					switch(msg)
					{
						case WM_COMMAND:
						{
							YAMN_MAILBROWSERPARAM Param={(HANDLE)0,ActualAccount,YAMN_ACC_MSG}; //(ActualAccount->NewMailN.Flags & ~YAMN_ACC_POP) | YAMN_ACC_MSGP,(ActualAccount->NoNewMailN.Flags & ~YAMN_ACC_POP) | YAMN_ACC_MSGP};

							RunMailBrowserSvc((WPARAM)&Param,(LPARAM)YAMN_MAILBROWSERVERSION);
						}
						break;
					}
					#ifdef DEBUG_SYNCHRO
					DebugLog(SynchroFile,"PopUpProc:LEFTCLICK:ActualAccountSO-read done\n");
					#endif
					ReadDoneFcn(ActualAccount->AccountAccessSO);
				}
				#ifdef DEBUG_SYNCHRO
				else
					DebugLog(SynchroFile,"PopUpProc:LEFTCLICK:ActualAccountSO-read enter failed\n");
				#endif
				SendMessageW(hWnd,UM_DESTROYPOPUP,0,0);
			}

			break;

		case WM_CONTEXTMENU:
			HANDLE hContact;

			hContact=(HANDLE)CallService(MS_POPUP_GETCONTACT,(WPARAM)hWnd,(LPARAM)0);
			if(CallService(MS_DB_CONTACT_IS,(WPARAM)hContact,(LPARAM)0))
			{
				CallService(MS_CLIST_REMOVEEVENT,(WPARAM)hContact,(LPARAM)"yamn new mail");
			}
			SendMessageW(hWnd,UM_DESTROYPOPUP,0,0);
			break;			
		case UM_FREEPLUGINDATA:
			//Here we'd free our own data, if we had it.
			return FALSE;
		case UM_INITPOPUP:
			//This is the equivalent to WM_INITDIALOG you'd get if you were the maker of dialog popups.
			WindowList_Add(YAMNVar.MessageWnds,hWnd,NULL);
			break;
		case UM_DESTROYPOPUP:
			WindowList_Remove(YAMNVar.MessageWnds,hWnd);
			break;
		case WM_YAMN_STOPACCOUNT:
		{
			HACCOUNT ActualAccount;
			HANDLE hContact;
			DBVARIANT dbv;

			hContact=(HANDLE)CallService(MS_POPUP_GETCONTACT,(WPARAM)hWnd,(LPARAM)0);

			if(!DBGetContactSetting((HANDLE) hContact,ProtoName,"Id",&dbv)) 
			{
				ActualAccount=(HACCOUNT) CallService(MS_YAMN_FINDACCOUNTBYNAME,(WPARAM)POP3Plugin,(LPARAM)dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			else
				ActualAccount = (HACCOUNT) hContact;

			if((HACCOUNT)wParam!=ActualAccount)
				break;
			DestroyWindow(hWnd);
			return 0;
		}
		case WM_NOTIFY:
		default:
			break;
	}
	return DefWindowProc(hWnd,msg,wParam,lParam);
}

LRESULT CALLBACK NoNewMailPopUpProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam) 
{
	switch(msg)
	{
		case WM_COMMAND:
			if((HIWORD(wParam)==STN_CLICKED) && (msg==WM_COMMAND))
			{
				HACCOUNT ActualAccount;
				HANDLE hContact;
				DBVARIANT dbv;

				hContact=(HANDLE)CallService(MS_POPUP_GETCONTACT,(WPARAM)hWnd,(LPARAM)0);

				if(!DBGetContactSetting((HANDLE) hContact,ProtoName,"Id",&dbv)) 
				{
					ActualAccount=(HACCOUNT) CallService(MS_YAMN_FINDACCOUNTBYNAME,(WPARAM)POP3Plugin,(LPARAM)dbv.pszVal);
					DBFreeVariant(&dbv);
				}
				else
					ActualAccount = (HACCOUNT) hContact;

				#ifdef DEBUG_SYNCHRO
				DebugLog(SynchroFile,"PopUpProc:LEFTCLICK:ActualAccountSO-read wait\n");
				#endif
				if(WAIT_OBJECT_0==WaitToReadFcn(ActualAccount->AccountAccessSO))
				{
					#ifdef DEBUG_SYNCHRO
					DebugLog(SynchroFile,"PopUpProc:LEFTCLICK:ActualAccountSO-read enter\n");
					#endif
					switch(msg)
					{
						case WM_COMMAND:
						{
							YAMN_MAILBROWSERPARAM Param={(HANDLE)0,ActualAccount,ActualAccount->NewMailN.Flags,ActualAccount->NoNewMailN.Flags,0};

							Param.nnflags=Param.nnflags | YAMN_ACC_MSG;			//show mails in account even no new mail in account
							Param.nnflags=Param.nnflags & ~YAMN_ACC_POP;
							
							Param.nflags=Param.nflags | YAMN_ACC_MSG;			//show mails in account even no new mail in account
							Param.nflags=Param.nflags & ~YAMN_ACC_POP;

							RunMailBrowserSvc((WPARAM)&Param,(LPARAM)YAMN_MAILBROWSERVERSION);
						}
						break;
					}
					#ifdef DEBUG_SYNCHRO
					DebugLog(SynchroFile,"PopUpProc:LEFTCLICK:ActualAccountSO-read done\n");
					#endif
					ReadDoneFcn(ActualAccount->AccountAccessSO);
				}
				#ifdef DEBUG_SYNCHRO
				else
					DebugLog(SynchroFile,"PopUpProc:LEFTCLICK:ActualAccountSO-read enter failed\n");
				#endif
				SendMessageW(hWnd,UM_DESTROYPOPUP,0,0);
			}
			break;
	
		case WM_CONTEXTMENU:
			SendMessageW(hWnd,UM_DESTROYPOPUP,0,0);
			break;

		case UM_FREEPLUGINDATA:
			//Here we'd free our own data, if we had it.
			return FALSE;
		case UM_INITPOPUP:
			//This is the equivalent to WM_INITDIALOG you'd get if you were the maker of dialog popups.
			WindowList_Add(YAMNVar.MessageWnds,hWnd,NULL);
			break;
		case UM_DESTROYPOPUP:
			WindowList_Remove(YAMNVar.MessageWnds,hWnd);
			break;
		case WM_YAMN_STOPACCOUNT:
		{
			HACCOUNT ActualAccount;
			HANDLE hContact;
			DBVARIANT dbv;

			hContact=(HANDLE)CallService(MS_POPUP_GETCONTACT,(WPARAM)hWnd,(LPARAM)0);

			if(!DBGetContactSetting((HANDLE) hContact,ProtoName,"Id",&dbv)) 
			{
				ActualAccount=(HACCOUNT) CallService(MS_YAMN_FINDACCOUNTBYNAME,(WPARAM)POP3Plugin,(LPARAM)dbv.pszVal);
				DBFreeVariant(&dbv);
			}
			else
				ActualAccount = (HACCOUNT) hContact;

			if((HACCOUNT)wParam!=ActualAccount)
				break;

			DestroyWindow(hWnd);
			return 0;
		}
		case WM_NOTIFY:
/*			switch(((LPNMHDR)lParam)->code)
			{
				case NM_CLICK:
				{
				}
			}
			break;
*/		default:
			break;
	}
	return DefWindowProc(hWnd,msg,wParam,lParam);
}

#ifdef __GNUC__
#define NUM100NANOSEC  116444736000000000ULL
#else
#define NUM100NANOSEC  116444736000000000
#endif
ULONGLONG MimeDateToFileTime(char *datein){
	char *day, *month, *year, *time, *shift;
	SYSTEMTIME st;
	ULONGLONG res=0;
	int wShiftSeconds = CallService(MS_DB_TIME_TIMESTAMPTOLOCAL,0,0);
	GetLocalTime(&st);
	//datein = "Xxx, 1 Jan 2060 5:29:1 +0530 XXX";
	if (datein){
		char tmp [64];
		strncpy(tmp,datein,63); tmp [63]=0;
		if (day = strchr(tmp,' ')){	day[0]=0; day++;}
		if (month = strchr(day,' ')){month[0]=0; month++;}
		if (year = strchr(month,' ')){year[0] = 0; year++;}
		if (time = strchr(year,' ')){time[0] = 0; time++;}
		if (shift=strchr(time,' ')){shift[0]=0; shift++;shift[5]=0;}

		if (year){
			st.wYear = atoi(year);
			if (strlen(year)<4)	if (st.wYear<70)st.wYear += 2000; else st.wYear += 1900;
		};
		if (month) for(int i=0;i<12;i++) if(strncmp(month,s_MonthNames[i],3)==0) st.wMonth = i + 1;
		if (day) st.wDay = atoi(day);
		if (time) {
			char *h, *m, *s;
			h = time;
			if (m = strchr(h,':')){m[0]=0; m++;}
			if (s = strchr(m,':')){s[0] = 0; s++;}
			st.wHour = atoi(h);
			st.wMinute = m?atoi(m):0;
			st.wSecond = s?atoi(s):0;
		} else {st.wHour=st.wMinute=st.wSecond=0;}

		if (shift){
			if (strlen(shift)<4) {
				//has only hour
				wShiftSeconds = (atoi(shift))*3600;
			} else {
				char *smin = shift + strlen(shift)-2;
				int ismin = atoi(smin);
				smin[0] = 0;
				int ishour = atoi(shift);
				wShiftSeconds = (ishour*60+(ishour<0?-1:1)*ismin)*60;
			}
		}
	} // if (datein)
	FILETIME ft;
	if (SystemTimeToFileTime(&st,&ft)){
		res = ((ULONGLONG)ft.dwHighDateTime<<32)|((ULONGLONG)ft.dwLowDateTime);
		LONGLONG w100nano = Int32x32To64((DWORD)wShiftSeconds,10000000);
		res -=  w100nano;
	}else{
		res=0;
	}
	//for testing
	/*{
		char buffA[1024] = {0};
		//res=0x7FFF35F4F06C7FFF; // -> Fri, 31 Dec 30827 23:59:59.
		res = 0xEDCBA9876FEDC9AC;
		ft.dwLowDateTime = (DWORD)res;
		ft.dwHighDateTime = (DWORD)(res >> 32);
		if (!FileTimeToSystemTime(&ft,&st)){
			int len;
			len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
				NULL, GetLastError(), 0, buffA,
				sizeof(buffA), NULL);
		} else {
			GetDateFormatA(LOCALE_USER_DEFAULT,DATE_LONGDATE,&st,NULL,buffA,sizeof(buffA));
			buffA[strlen(buffA)] = ' ';
			GetTimeFormatA(LOCALE_USER_DEFAULT,0,&st,NULL,&buffA[strlen(buffA)],sizeof(buffA));
		}
	}*/
	return res;
}
void FileTimeToLocalizedDateTime(LONGLONG filetime, WCHAR *dateout, int lendateout){
	int localeID = CallService(MS_LANGPACK_GETLOCALE,0,0);
	//int localeID = MAKELCID(LANG_URDU, SORT_DEFAULT);
	if (localeID==CALLSERVICE_NOTFOUND) localeID=LOCALE_USER_DEFAULT;
	//0x7FFF35F4F06C7FFF -> Fri, 31 Dec 30827 23:59:59.9999999 
	//The biggest time Get[Date|Time]Format can handle
	if (filetime>0x7FFF35F4F06C7FFF) filetime = 0x7FFF35F4F06C7FFF;
	else if (filetime<0) filetime=0;
	SYSTEMTIME st;
	FILETIME ft;
	ft.dwLowDateTime = (DWORD)filetime;
	ft.dwHighDateTime = (DWORD)(filetime >> 32);
	if (!FileTimeToSystemTime(&ft,&st)){
		// this should never happen
		wcsncpy(dateout,L"Incorrect FileTime",lendateout);
	} else {
		dateout[lendateout]=0;
		GetDateFormatW(localeID,DATE_LONGDATE,&st,NULL,dateout,lendateout);
		int templen = wcslen(dateout);
		if (templen<(lendateout-1)){
			dateout[templen] = ' ';
			GetTimeFormatW(localeID,0,&st,NULL,&dateout[templen+1],lendateout-templen-1);
		}
	}
}
void MimeDateToLocalizedDateTime(char *datein, WCHAR *dateout, int lendateout){
	ULONGLONG ft = MimeDateToFileTime(datein);
	FileTimeToLocalizedDateTime(ft,dateout,lendateout);
}
/*
int FormatMimeDate(char *datein,char *dateout)
{
	int i,j,l,max,m;
	char *day, *month, *year, *time, *tmp;

	j = 0;
	l = 0;
	max = strlen(datein);
	
	day = new char[2];
	month = new char[3];
	year = new char[4];
	time = new char[8];
	tmp = new char[max];
	
	for(i=0;i<max;i++,l++)
	{
		if(datein[i] == ' ')
		{
			j++;
			l=-1;
			switch(j)
			{
				case 2:
					sprintf(day,"%c%c",tmp[0],tmp[1]);
					break;
				case 3:
					sprintf(month,"%c%c%c",tmp[0],tmp[1],tmp[2]);
					break;
				case 4:
					sprintf(year,"%c%c%c%c",tmp[0],tmp[1],tmp[2],tmp[3]);
					break;
				case 5:
					sprintf(time,"%c%c%c%c%c%c%c%c",tmp[0],tmp[1],tmp[2],tmp[3],tmp[4],tmp[5],tmp[6],tmp[7]);
					break;
				default:
					break;
			}
		}
		else
			tmp[l] = datein[i];
	}

	m = 0;
	for(i=0;i<12;i++)
	{
		if(strcmp(month,s_MonthNames[i])==0)
		{
			m = i + 1;
		}
	}

	sprintf(dateout,"date = %s-%2d-%s %s",year,m,day,time);
	delete[] day;
	delete[] month;
	delete[] year ;
	delete[] time;
	delete[] tmp;
	return 0;
}
*/
int CALLBACK ListViewCompareProc(LPARAM lParam1, LPARAM lParam2,LPARAM lParamSort )
{

	int					nResult;
	HYAMNMAIL			email1;
	HYAMNMAIL			email2;
	struct CShortHeader	Header1;
	struct CShortHeader	Header2;	
	char			*str1;
	char			*str2;

	email1 = (HYAMNMAIL)lParam1;
	email2 = (HYAMNMAIL)lParam2;

	ZeroMemory(&Header1,sizeof(Header1));
	ZeroMemory(&Header2,sizeof(Header2));

	if(lParam1 == NULL)
		return 0;

	if(lParam2 == NULL)
		return 0;

	nResult = 0;

	try
	{

		ExtractShortHeader(email1->MailData->TranslatedHeader,&Header1);
		ExtractShortHeader(email2->MailData->TranslatedHeader,&Header2);	

		switch((int)lParamSort)
		{
			case 0:
				if(Header1.FromNick == NULL) str1 = Header1.From;
				else str1 = Header1.FromNick;

				if(Header2.FromNick == NULL) str2 = Header2.From;
				else str2 = Header2.FromNick;

				nResult = strcmp(str1, str2);

				if(bFrom) nResult = -nResult;
				break;
			case 1:
				if(Header1.Subject == NULL) str1 = " ";
				else str1 = Header1.Subject;

				if(Header2.Subject == NULL) str2 = " ";
				else str2 = Header2.Subject;

				nResult = strcmp(str1, str2);

				if(bSub) nResult = -nResult;
				break;
			case 2:
				if(email1->MailData->Size == email2->MailData->Size) nResult = 0;
				if(email1->MailData->Size > email2->MailData->Size) nResult = 1;
				if(email1->MailData->Size < email2->MailData->Size) nResult = -1;

				if(bSize) nResult = -nResult;
				break;

			case 3:
				{
				ULONGLONG ts1 = 0, ts2 = 0;
				ts1 = MimeDateToFileTime(Header1.Date);
				ts2 = MimeDateToFileTime(Header2.Date);
				if(ts1 > ts2) nResult = 1;
				else if (ts1 < ts2) nResult = -1;
				else nResult = 0;
				}
				if(bDate) nResult = -nResult;
				break;

			default:
				if(Header1.Subject == NULL) str1 = " ";
				else str1 = Header1.Subject;

				if(Header2.Subject == NULL) str2 = " ";
				else str2 = Header2.Subject;

				nResult = strcmp(str1, str2);
				break;
		}
		//MessageBox(NULL,str1,str2,0);
	}
	catch( ... )
	{
	}
	return nResult;

} 

void ConvertCodedStringToUnicode(char *stream,WCHAR **storeto,DWORD cp,int mode);
BOOL CALLBACK DlgProcYAMNShowMessage(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam)
{
	HYAMNMAIL ActualMail = (HYAMNMAIL)lParam;
	switch(msg)
	{
		case WM_INITDIALOG:
		{
//			HIMAGELIST hIcons;
			WCHAR *iHeaderW=NULL;
			WCHAR *iValueW=NULL;
			int StrLen;
			HWND hListView = GetDlgItem(hDlg,IDC_LISTHEADERS);
			SendMessageW(hDlg,WM_SETICON,(WPARAM)ICON_BIG,(LPARAM)hNewMailIcon);
			SendMessageW(hDlg,WM_SETICON,(WPARAM)ICON_SMALL,(LPARAM)hNewMailIcon);

			ListView_SetUnicodeFormat(hListView,TRUE);
			ListView_SetExtendedListViewStyle(hListView,LVS_EX_FULLROWSELECT);

			StrLen=MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Header"),-1,NULL,0);
			iHeaderW=new WCHAR[StrLen+1];
			MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Header"),-1,iHeaderW,StrLen);

			StrLen=MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Value"),-1,NULL,0);
			iValueW=new WCHAR[StrLen+1];
			MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Value"),-1,iValueW,StrLen);

			LVCOLUMNW lvc0={LVCF_FMT | LVCF_TEXT | LVCF_WIDTH,LVCFMT_LEFT,130,iHeaderW,0,0};
			LVCOLUMNW lvc1={LVCF_FMT | LVCF_TEXT | LVCF_WIDTH,LVCFMT_LEFT,400,iValueW,0,0};
			SendMessageW(hListView,LVM_INSERTCOLUMNW,(WPARAM)0,(LPARAM)&lvc0);
			SendMessageW(hListView,LVM_INSERTCOLUMNW,(WPARAM)1,(LPARAM)&lvc1);
			if(NULL!=iHeaderW)
				delete[] iHeaderW;
			if(NULL!=iValueW)
				delete[] iValueW;

			//WindowList_Add(YAMNVar.MessageWnds,hDlg,NULL);
			//WindowList_Add(YAMNVar.NewMailAccountWnd,hDlg,ActualAccount);
			struct CMimeItem *Header;
			LVITEM item;
			item.mask=LVIF_TEXT | LVIF_PARAM;
			WCHAR *From=0,*Subj=0;
			for(Header=ActualMail->MailData->TranslatedHeader;Header!=NULL;Header=Header->Next)
			{
				WCHAR *str1 = 0;
				WCHAR *str2 = 0;

				ConvertCodedStringToUnicode(Header->name,&str1,ActualMail->MailData->CP,1); 
				ConvertCodedStringToUnicode(Header->value,&str2,ActualMail->MailData->CP,1); 
				if (!From) if (!strcmp(Header->name,"From")) {
					From =new WCHAR[wcslen(str2)+1];
					wcscpy(From,str2);
				}
				if (!Subj) if (!strcmp(Header->name,"Subject")) {
					Subj =new WCHAR[wcslen(str2)+1];
					wcscpy(Subj,str2);
				}
				int count = 0; WCHAR **split=0;
				if (str2){
					int ofs = 0;
					while (str2[ofs]) {
						if ((str2[ofs]==0x266A)||(str2[ofs]==0x25D9)||(str2[ofs]==0x25CB)||
							(str2[ofs]==0x09)||(str2[ofs]==0x0A)||(str2[ofs]==0x0D))count++;
						ofs++;
					}
					split=new WCHAR*[count+1];
					count=0; ofs=0;
					split[0]=str2;
					while (str2[ofs]){
						if ((str2[ofs]==0x266A)||(str2[ofs]==0x25D9)||(str2[ofs]==0x25CB)||
							(str2[ofs]==0x09)||(str2[ofs]==0x0A)||(str2[ofs]==0x0D)) {
							if (str2[ofs-1]){
								count++;
							}
							split[count]=(WCHAR *)(str2+ofs+1);
							str2[ofs]=0;
						}
						ofs++;
					};
				}
				if (!strcmp(Header->name,"From")||!strcmp(Header->name,"To")||!strcmp(Header->name,"Date")||!strcmp(Header->name,"Subject")) 
					item.iItem = 0;
				else 
					item.iItem = 999;
				for (int i=0;i<=count;i++){
					item.iSubItem=0;
					if (i==0){
						item.pszText=str1;
					} else {
						item.iItem++;
						item.pszText=0;
					}
					item.iItem=SendMessageW(hListView,LVM_INSERTITEMW,(WPARAM)0,(LPARAM)&item);
					item.iSubItem=1;
					item.pszText=str2?split[i]:0;
					SendMessageW(hListView,LVM_SETITEMTEXTW,(WPARAM)item.iItem,(LPARAM)&item);
				} 
				if (split)delete[] split;

				if (str1) free(str1);
				if (str2) free(str2);
			}
			WCHAR *title=0;
			title = new WCHAR[(From?wcslen(From):0)+(Subj?wcslen(Subj):0)+4];
			if (From&&Subj) wsprintfW(title,L"%s (%s)",Subj,From);
			else if (From)wsprintfW(title,L"%s",From);
			else if (Subj)wsprintfW(title,L"%s",Subj);
			else wsprintfW(title,L"none");
			if (Subj) delete[] Subj;
			if (From) delete[] From;
			SendMessageW(hDlg,WM_SETTEXT,(WPARAM)0,(LPARAM)title);
			delete[] title;
			MoveWindow(hDlg,HeadPosX,HeadPosY,HeadSizeX,HeadSizeY,0);
			ShowWindow(hDlg,SW_SHOWNORMAL);
			break;
		}
		case WM_DESTROY:
		{
			RECT coord;
			if(!YAMNVar.Shutdown && GetWindowRect(hDlg,&coord))	//the YAMNVar.Shutdown testing is because M<iranda strange functionality at shutdown phase, when call to DBWriteContactSetting freezes calling thread
			{
				HeadPosX=coord.left;
				HeadSizeX=coord.right-coord.left;
				HeadPosY=coord.top;
				HeadSizeY=coord.bottom-coord.top;
				DBWriteContactSettingDword(NULL,YAMN_DBMODULE,YAMN_DBMSGPOSX,HeadPosX);
				DBWriteContactSettingDword(NULL,YAMN_DBMODULE,YAMN_DBMSGPOSY,HeadPosY);
				DBWriteContactSettingDword(NULL,YAMN_DBMODULE,YAMN_DBMSGSIZEX,HeadSizeX);
				DBWriteContactSettingDword(NULL,YAMN_DBMODULE,YAMN_DBMSGSIZEY,HeadSizeY);
			}
		}
		break;
		case WM_SYSCOMMAND:
		{
			switch(wParam)
			{
				case SC_CLOSE:
					DestroyWindow(hDlg);
					break;
			}
		}
		break;
		case WM_MOVE:
				HeadPosX=LOWORD(lParam);	//((LPRECT)lParam)->right-((LPRECT)lParam)->left;
				HeadPosY=HIWORD(lParam);	//((LPRECT)lParam)->bottom-((LPRECT)lParam)->top;
				return 0;
		case WM_SIZE:
			if(wParam==SIZE_RESTORED)
			{
				HWND hList = GetDlgItem(hDlg,IDC_LISTHEADERS);
				BOOL changeX = LOWORD(lParam)!=HeadSizeX;
				HeadSizeX=LOWORD(lParam);	//((LPRECT)lParam)->right-((LPRECT)lParam)->left;
				HeadSizeY=HIWORD(lParam);	//((LPRECT)lParam)->bottom-((LPRECT)lParam)->top;
				int localSizeX;
				RECT coord;
				MoveWindow(hList,  5         ,5     ,HeadSizeX-10    ,HeadSizeY-10,TRUE);	//where to put list mail window while resizing
				if (GetClientRect(hList,&coord)){
					localSizeX=coord.right-coord.left;
				} else localSizeX=HeadSizeX;
				LONG iNameWidth =  ListView_GetColumnWidth(hList,0);
				ListView_SetColumnWidth(hList,1,(localSizeX<=iNameWidth)?0:(localSizeX-iNameWidth));
			}
//			break;
			return 0;
	}
	return 0;
}

BOOL CALLBACK DlgProcYAMNMailBrowser(HWND hDlg,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch(msg)
	{
		case WM_INITDIALOG:
		{
//			HIMAGELIST hIcons;
			HACCOUNT ActualAccount;
			WCHAR *iFromW=NULL;
			WCHAR *iSubjectW=NULL;
			WCHAR *iSizeW=NULL;
			WCHAR *iDateW=NULL;
			WCHAR *iRunAppW=NULL;
			WCHAR *iDeleteMailsW=NULL;
			int StrLen;
			struct MailBrowserWinParam *MyParam=(struct MailBrowserWinParam *)lParam;
			struct CMailWinUserInfo *mwui;

			ListView_SetUnicodeFormat(GetDlgItem(hDlg,IDC_LISTMAILS),TRUE);
			ListView_SetExtendedListViewStyle(GetDlgItem(hDlg,IDC_LISTMAILS),LVS_EX_FULLROWSELECT);

			ActualAccount=MyParam->account;
			mwui=new struct CMailWinUserInfo;
			mwui->Account=ActualAccount;
			mwui->TrayIconState=0;
			mwui->UpdateMailsMessagesAccess=FALSE;
			mwui->Seen=FALSE;
			mwui->RunFirstTime=TRUE;

			SetWindowLong(hDlg,DWL_USER,(LONG)mwui);
			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"MailBrowser:INIT:ActualAccountSO-read wait\n");
			#endif
			if(WAIT_OBJECT_0!=WaitToReadFcn(ActualAccount->AccountAccessSO))
			{
				#ifdef DEBUG_SYNCHRO
				DebugLog(SynchroFile,"MailBrowser:INIT:ActualAccountSO-read enter failed\n");
				#endif
				DestroyWindow(hDlg);
				return FALSE;
			}
			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"MailBrowser:INIT:ActualAccountSO-read enter\n");
			#endif

			StrLen=MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("From"),-1,NULL,0);
			iFromW=new WCHAR[StrLen+1];
			MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("From"),-1,iFromW,StrLen);

			StrLen=MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Subject"),-1,NULL,0);
			iSubjectW=new WCHAR[StrLen+1];
			MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Subject"),-1,iSubjectW,StrLen);

			StrLen=MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Size"),-1,NULL,0);
			iSizeW=new WCHAR[StrLen+1];
			MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Size"),-1,iSizeW,StrLen);

			StrLen=MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Date"),-1,NULL,0);
			iDateW=new WCHAR[StrLen+1];
			MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Date"),-1,iDateW,StrLen);

			StrLen=MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Run application"),-1,NULL,0);
			iRunAppW=new WCHAR[StrLen+1];
			MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Run application"),-1,iRunAppW,StrLen);

			StrLen=MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Delete selected"),-1,NULL,0);
			iDeleteMailsW=new WCHAR[StrLen+1];
			MultiByteToWideChar(CP_ACP,MB_USEGLYPHCHARS,Translate("Delete selected"),-1,iDeleteMailsW,StrLen);

			SendMessageW(GetDlgItem(hDlg,IDC_BTNAPP),WM_SETTEXT,(WPARAM)0,(LPARAM)iRunAppW);
			SendMessageW(GetDlgItem(hDlg,IDC_BTNDEL),WM_SETTEXT,(WPARAM)0,(LPARAM)iDeleteMailsW);

			LVCOLUMNW lvc0={LVCF_FMT | LVCF_TEXT | LVCF_WIDTH,LVCFMT_LEFT,FromWidth,iFromW,0,0};
			LVCOLUMNW lvc1={LVCF_FMT | LVCF_TEXT | LVCF_WIDTH,LVCFMT_LEFT,SubjectWidth,iSubjectW,0,0};
			LVCOLUMNW lvc2={LVCF_FMT | LVCF_TEXT | LVCF_WIDTH,LVCFMT_LEFT,SizeWidth,iSizeW,0,0};
			LVCOLUMNW lvc3={LVCF_FMT | LVCF_TEXT | LVCF_WIDTH,LVCFMT_LEFT,SizeDate,iDateW,0,0};
			SendMessageW(GetDlgItem(hDlg,IDC_LISTMAILS),LVM_INSERTCOLUMNW,(WPARAM)0,(LPARAM)&lvc0);
			SendMessageW(GetDlgItem(hDlg,IDC_LISTMAILS),LVM_INSERTCOLUMNW,(WPARAM)1,(LPARAM)&lvc1);
			SendMessageW(GetDlgItem(hDlg,IDC_LISTMAILS),LVM_INSERTCOLUMNW,(WPARAM)2,(LPARAM)&lvc2);
			SendMessageW(GetDlgItem(hDlg,IDC_LISTMAILS),LVM_INSERTCOLUMNW,(WPARAM)3,(LPARAM)&lvc3);
			if(NULL!=iFromW)
				delete[] iFromW;
			if(NULL!=iSubjectW)
				delete[] iSubjectW;
			if(NULL!=iSizeW)
				delete[] iSizeW;
			if(NULL!=iDateW)
				delete[] iDateW;
			if(NULL!=iRunAppW)
				delete[] iRunAppW;
			if(NULL!=iDeleteMailsW)
				delete[] iDeleteMailsW;

			if((ActualAccount->NewMailN.App!=NULL) && (wcslen(ActualAccount->NewMailN.App)))
				EnableWindow(GetDlgItem(hDlg,IDC_BTNAPP),(WPARAM)TRUE);
			else
				EnableWindow(GetDlgItem(hDlg,IDC_BTNAPP),(WPARAM)FALSE);
			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"MailBrowser:INIT:ActualAccountSO-read done\n");
			#endif
			ReadDoneFcn(ActualAccount->AccountAccessSO);

			WindowList_Add(YAMNVar.MessageWnds,hDlg,NULL);
			WindowList_Add(YAMNVar.NewMailAccountWnd,hDlg,ActualAccount);

			{
				char accstatus[512];

				GetStatusFcn(ActualAccount,accstatus);
				SetDlgItemTextA(hDlg,IDC_STSTATUS,accstatus);
			}
			SetTimer(hDlg,TIMER_FLASHING,500,NULL);

			if(ActualAccount->hContact != NULL)
			{
				CallService(MS_CLIST_REMOVEEVENT,(WPARAM)ActualAccount->hContact,(LPARAM)"yamn new mail");
			}

			OldListViewSubclassProc = (WNDPROC) SetWindowLong(GetDlgItem(hDlg, IDC_LISTMAILS), GWL_WNDPROC, (LONG) ListViewSubclassProc);

			break;
		}
		case WM_DESTROY:
		{
			HACCOUNT ActualAccount;
			RECT coord;
			LVCOLUMNW ColInfo;
			NOTIFYICONDATA nid;
			HYAMNMAIL Parser;
			struct CMailWinUserInfo *mwui;

			mwui=(struct CMailWinUserInfo *)GetWindowLong(hDlg,DWL_USER);
			if(NULL==(ActualAccount=GetWindowAccount(hDlg)))
				break;
			ColInfo.mask=LVCF_WIDTH;
			if(ListView_GetColumn(GetDlgItem(hDlg,IDC_LISTMAILS),0,&ColInfo))
				FromWidth=ColInfo.cx;
			if(ListView_GetColumn(GetDlgItem(hDlg,IDC_LISTMAILS),1,&ColInfo))
				SubjectWidth=ColInfo.cx;
			if(ListView_GetColumn(GetDlgItem(hDlg,IDC_LISTMAILS),2,&ColInfo))
				SizeWidth=ColInfo.cx;
			if(ListView_GetColumn(GetDlgItem(hDlg,IDC_LISTMAILS),3,&ColInfo))
				SizeDate=ColInfo.cx;

			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"MailBrowser:DESTROY:save window position\n");
			#endif
			if(!YAMNVar.Shutdown && GetWindowRect(hDlg,&coord))	//the YAMNVar.Shutdown testing is because M<iranda strange functionality at shutdown phase, when call to DBWriteContactSetting freezes calling thread
			{
				PosX=coord.left;
				SizeX=coord.right-coord.left;
				PosY=coord.top;
				SizeY=coord.bottom-coord.top;
				DBWriteContactSettingDword(NULL,YAMN_DBMODULE,YAMN_DBPOSX,PosX);
				DBWriteContactSettingDword(NULL,YAMN_DBMODULE,YAMN_DBPOSY,PosY);
				DBWriteContactSettingDword(NULL,YAMN_DBMODULE,YAMN_DBSIZEX,SizeX);
				DBWriteContactSettingDword(NULL,YAMN_DBMODULE,YAMN_DBSIZEY,SizeY);
			}
			KillTimer(hDlg,TIMER_FLASHING);

			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"MailBrowser:DESTROY:remove window from list\n");
			#endif
			WindowList_Remove(YAMNVar.NewMailAccountWnd,hDlg);
			WindowList_Remove(YAMNVar.MessageWnds,hDlg);

			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"MailBrowser:DESTROY:ActualAccountMsgsSO-write wait\n");
			#endif
			if(WAIT_OBJECT_0!=WaitToWriteFcn(ActualAccount->MessagesAccessSO))
			{
				#ifdef DEBUG_SYNCHRO
				DebugLog(SynchroFile,"MailBrowser:DESTROY:ActualAccountMsgsSO-write wait failed\n");
				#endif
				break;
			}
			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"MailBrowser:DESTROY:ActualAccountMsgsSO-write enter\n");
			#endif
			//delete mails from queue, which are deleted from server (spam level 3 mails e.g.)
			for(Parser=(HYAMNMAIL)ActualAccount->Mails;Parser!=NULL;Parser=Parser->Next)
			{
				if((Parser->Flags & YAMN_MSG_DELETED) && YAMN_MSG_SPAML(Parser->Flags,YAMN_MSG_SPAML3) && mwui->Seen)		//if spaml3 was already deleted and user knows about it
				{
					DeleteMessageFromQueueFcn((HYAMNMAIL *)&ActualAccount->Mails,Parser,1);
					CallService(MS_YAMN_DELETEACCOUNTMAIL,(WPARAM)ActualAccount->Plugin,(LPARAM)Parser);
				}
			}

			//mark mails as read (remove "new" and "unseen" flags)
			if(mwui->Seen)
				SetRemoveFlagsInQueueFcn((HYAMNMAIL)ActualAccount->Mails,YAMN_MSG_DISPLAY,0,YAMN_MSG_NEW | YAMN_MSG_UNSEEN,0);
			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"MailBrowser:DESTROY:ActualAccountMsgsSO-write done\n");
			#endif
			WriteDoneFcn(ActualAccount->MessagesAccessSO);

			ZeroMemory(&nid,sizeof(NOTIFYICONDATA));

			delete mwui;
			SetWindowLong(hDlg,DWL_USER,(LONG)NULL);

			nid.cbSize=sizeof(NOTIFYICONDATA);
			nid.hWnd=hDlg;
			nid.uID=0;
			Shell_NotifyIcon(NIM_DELETE,&nid);
			PostQuitMessage(0);
		}
			break;
		case WM_SHOWWINDOW:
		{
			struct CMailWinUserInfo *mwui;

			if(NULL==(mwui=(struct CMailWinUserInfo *)GetWindowLong(hDlg,DWL_USER)))
				return 0;
			mwui->Seen=TRUE;
		}
		case WM_YAMN_CHANGESTATUS:
		{
			HACCOUNT ActualAccount;
			char accstatus[512];

			if(NULL==(ActualAccount=GetWindowAccount(hDlg)))
				break;
			if((HACCOUNT)wParam!=ActualAccount)
				break;
			GetStatusFcn(ActualAccount,accstatus);
			SetDlgItemTextA(hDlg,IDC_STSTATUS,accstatus);
		}
			return 1;
		case WM_YAMN_CHANGECONTENT:
		{
			struct CUpdateMails UpdateParams;
			BOOL ThisThreadWindow=(GetCurrentThreadId()==GetWindowThreadProcessId(hDlg,NULL));

			if(NULL==(UpdateParams.Copied=CreateEvent(NULL,FALSE,FALSE,NULL)))
			{
				DestroyWindow(hDlg);
				return 0;
			}
			UpdateParams.Flags=(struct CChangeContent *)lParam;
			UpdateParams.Waiting=!ThisThreadWindow;

			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"MailBrowser:CHANGECONTENT:posting UPDATEMAILS\n");
			#endif
			if(ThisThreadWindow)
			{
				if(!UpdateMails(hDlg,(HACCOUNT)wParam,UpdateParams.Flags->nflags,UpdateParams.Flags->nnflags))
					DestroyWindow(hDlg);
			}
			else if(PostMessage(hDlg,WM_YAMN_UPDATEMAILS,wParam,(LPARAM)&UpdateParams))	//this ensures UpdateMails will execute the thread who created the browser window
			{
				if(!ThisThreadWindow)
				{
					#ifdef DEBUG_SYNCHRO
					DebugLog(SynchroFile,"MailBrowser:CHANGECONTENT:waiting for event\n");
					#endif
					WaitForSingleObject(UpdateParams.Copied,INFINITE);
					#ifdef DEBUG_SYNCHRO
					DebugLog(SynchroFile,"MailBrowser:CHANGECONTENT:event signaled\n");
					#endif
				}
			}

			CloseHandle(UpdateParams.Copied);
		}
			return 1;
		case WM_YAMN_UPDATEMAILS:
		{
			HACCOUNT ActualAccount;

			struct CUpdateMails *um=(struct CUpdateMails *)lParam;
			DWORD nflags,nnflags;

			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"MailBrowser:UPDATEMAILS\n");
			#endif

			if(NULL==(ActualAccount=GetWindowAccount(hDlg)))
				return 0;
			if((HACCOUNT)wParam!=ActualAccount)
				return 0;

			nflags=um->Flags->nflags;
			nnflags=um->Flags->nnflags;

			if(um->Waiting)
				SetEvent(um->Copied);

			if(!UpdateMails(hDlg,ActualAccount,nflags,nnflags))
				DestroyWindow(hDlg);
		}
			return 1;
		case WM_YAMN_STOPACCOUNT:
		{
			HACCOUNT ActualAccount;

			if(NULL==(ActualAccount=GetWindowAccount(hDlg)))
				break;
			if((HACCOUNT)wParam!=ActualAccount)
				break;
			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"MailBrowser:STOPACCOUNT:sending destroy msg\n");
			#endif
			PostQuitMessage(0);
		}
			return 1;
		case WM_YAMN_NOTIFYICON:
		{
			HACCOUNT ActualAccount;
			if(NULL==(ActualAccount=GetWindowAccount(hDlg)))
				break;

			switch(lParam)
			{
				case WM_LBUTTONDBLCLK:
					#ifdef DEBUG_SYNCHRO
					DebugLog(SynchroFile,"MailBrowser:DBLCLICKICON:ActualAccountSO-read wait\n");
					#endif
					if(WAIT_OBJECT_0!=WaitToReadFcn(ActualAccount->AccountAccessSO))
					{
						#ifdef DEBUG_SYNCHRO
						DebugLog(SynchroFile,"MailBrowser:DBLCLICKICON:ActualAccountSO-read wait failed\n");
						#endif
						return 0;
					}
					#ifdef DEBUG_SYNCHRO
					DebugLog(SynchroFile,"MailBrowser:DBLCLICKICON:ActualAccountSO-read enter\n");
					#endif
					if(ActualAccount->AbilityFlags & YAMN_ACC_BROWSE)
					{
						ShowWindow(hDlg,SW_SHOWNORMAL);
						SetForegroundWindow(hDlg);
					}
					else
						DestroyWindow(hDlg);
					#ifdef DEBUG_SYNCHRO
					DebugLog(SynchroFile,"MailBrowser:DBLCLICKICON:ActualAccountSO-read done\n");
					#endif
					ReadDoneFcn(ActualAccount->AccountAccessSO);
					break;
			}
			break;
		}
		case WM_SYSCOMMAND:
		{
			HACCOUNT ActualAccount;

			if(NULL==(ActualAccount=GetWindowAccount(hDlg)))
				break;
			switch(wParam)
			{
				case SC_CLOSE:
					DestroyWindow(hDlg);
					break;
			}
		}
			break;

		case WM_COMMAND:
		{
			HACCOUNT ActualAccount;
			int Items;

			if(NULL==(ActualAccount=GetWindowAccount(hDlg)))
				break;

			switch(LOWORD(wParam))
			{
				case IDC_BTNCHECKALL:
					ListView_SetItemState(GetDlgItem(hDlg,IDC_LISTMAILS), -1, 0, LVIS_SELECTED); // deselect all items
					ListView_SetItemState(GetDlgItem(hDlg,IDC_LISTMAILS),-1, LVIS_SELECTED ,LVIS_SELECTED);
					Items = ListView_GetItemCount(GetDlgItem(hDlg,IDC_LISTMAILS));
					ListView_RedrawItems(GetDlgItem(hDlg,IDC_LISTMAILS), 0, Items);
					UpdateWindow(GetDlgItem(hDlg,IDC_LISTMAILS));
					SetFocus(GetDlgItem(hDlg,IDC_LISTMAILS));
					break;

				case IDC_BTNOK:
					DestroyWindow(hDlg);
					break;

				case IDC_BTNAPP:
				{
					PROCESS_INFORMATION pi;
					STARTUPINFOW si;

					ZeroMemory(&si,sizeof(si));
					si.cb=sizeof(si);

					#ifdef DEBUG_SYNCHRO
					DebugLog(SynchroFile,"MailBrowser:BTNAPP:ActualAccountSO-read wait\n");
					#endif
					if(WAIT_OBJECT_0==WaitToReadFcn(ActualAccount->AccountAccessSO))
					{
						#ifdef DEBUG_SYNCHRO
						DebugLog(SynchroFile,"MailBrowser:BTNAPP:ActualAccountSO-read enter\n");
						#endif
						if(ActualAccount->NewMailN.App!=NULL)
						{
							WCHAR *Command;
							if(ActualAccount->NewMailN.AppParam!=NULL)
								Command=new WCHAR[wcslen(ActualAccount->NewMailN.App)+wcslen(ActualAccount->NewMailN.AppParam)+6];
							else
								Command=new WCHAR[wcslen(ActualAccount->NewMailN.App)+6];
						
							if(Command!=NULL)
							{
								lstrcpyW(Command,L"\"");
								lstrcatW(Command,ActualAccount->NewMailN.App);
								lstrcatW(Command,L"\" ");
								if(ActualAccount->NewMailN.AppParam!=NULL)
									lstrcatW(Command,ActualAccount->NewMailN.AppParam);
								CreateProcessW(NULL,Command,NULL,NULL,FALSE,NORMAL_PRIORITY_CLASS,NULL,NULL,&si,&pi);
								delete[] Command;
							}
						}

						#ifdef DEBUG_SYNCHRO
						DebugLog(SynchroFile,"MailBrowser:BTNAPP:ActualAccountSO-read done\n");
						#endif
						ReadDoneFcn(ActualAccount->AccountAccessSO);
					}
					#ifdef DEBUG_SYNCHRO
					else
						DebugLog(SynchroFile,"MailBrowser:BTNAPP:ActualAccountSO-read enter failed\n");
					#endif
					if(!(GetKeyState(VK_SHIFT) & 0x8000) && !(GetKeyState(VK_CONTROL) & 0x8000))
						DestroyWindow(hDlg);

				}
					break;
				case IDC_BTNDEL:
				{
					LVITEMW item;
					HYAMNMAIL FirstMail=NULL,ActualMail;
					HANDLE ThreadRunningEV;
					DWORD tid,Total=0;

					//	we use event to signal, that running thread has all needed stack parameters copied
					if(NULL==(ThreadRunningEV=CreateEvent(NULL,FALSE,FALSE,NULL)))
						break;
					int Items=ListView_GetItemCount(GetDlgItem(hDlg,IDC_LISTMAILS));

					item.stateMask=0xFFFFFFFF;
					#ifdef DEBUG_SYNCHRO
					DebugLog(SynchroFile,"MailBrowser:BTNDEL:ActualAccountMsgsSO-write wait\n");
					#endif
					if(WAIT_OBJECT_0==WaitToWriteFcn(ActualAccount->MessagesAccessSO))
					{
						#ifdef DEBUG_SYNCHRO
						DebugLog(SynchroFile,"MailBrowser:BTNDEL:ActualAccountMsgsSO-write enter\n");
						#endif
						for(int i=0;i<Items;i++)
						{
							item.iItem=i;
							item.iSubItem=0;
							item.mask=LVIF_PARAM | LVIF_STATE;
							item.stateMask=0xFFFFFFFF;
							ListView_GetItem(GetDlgItem(hDlg,IDC_LISTMAILS),&item);
							ActualMail=(HYAMNMAIL)item.lParam;
							if(NULL==ActualMail)
								break;
							if(item.state & LVIS_SELECTED)
							{
								ActualMail->Flags|=YAMN_MSG_USERDELETE;	//set to mail we are going to delete it
								Total++;
							}
						}

						// Enable write-access to mails
						#ifdef DEBUG_SYNCHRO
						DebugLog(SynchroFile,"MailBrowser:BTNDEL:ActualAccountMsgsSO-write done\n");
						#endif
						WriteDoneFcn(ActualAccount->MessagesAccessSO);

						if(Total)
						{
							char DeleteMsg[1024];

							sprintf(DeleteMsg,Translate("Do you really want to delete %d selected mails?"),Total);
							if(IDOK==MessageBox(hDlg,DeleteMsg,Translate("Delete confirmation"),MB_OKCANCEL | MB_ICONWARNING))
							{
								struct DeleteParam ParamToDeleteMails={YAMN_DELETEVERSION,ThreadRunningEV,ActualAccount,NULL};

								// Find if there's mail marked to delete, which was deleted before
								#ifdef DEBUG_SYNCHRO
								DebugLog(SynchroFile,"MailBrowser:BTNDEL:ActualAccountMsgsSO-write wait\n");
								#endif
								if(WAIT_OBJECT_0==WaitToWriteFcn(ActualAccount->MessagesAccessSO))
								{
									#ifdef DEBUG_SYNCHRO
									DebugLog(SynchroFile,"MailBrowser:BTNDEL:ActualAccountMsgsSO-write enter\n");
									#endif
									for(ActualMail=(HYAMNMAIL)ActualAccount->Mails;ActualMail!=NULL;ActualMail=ActualMail->Next)
									{
										if((ActualMail->Flags & YAMN_MSG_DELETED) && ((ActualMail->Flags & YAMN_MSG_USERDELETE)))	//if selected mail was already deleted
										{
											DeleteMessageFromQueueFcn((HYAMNMAIL *)&ActualAccount->Mails,ActualMail,1);
											CallService(MS_YAMN_DELETEACCOUNTMAIL,(WPARAM)ActualAccount->Plugin,(LPARAM)ActualMail);	//delete it from memory
											continue;
										}
									}
									// Set flag to marked mails that they can be deleted
									SetRemoveFlagsInQueueFcn((HYAMNMAIL)ActualAccount->Mails,YAMN_MSG_DISPLAY | YAMN_MSG_USERDELETE,0,YAMN_MSG_DELETEOK,1);
									// Create new thread which deletes marked mails.
									HANDLE NewThread;

									if(NULL!=(NewThread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)ActualAccount->Plugin->Fcn->DeleteMailsFcnPtr,(LPVOID)&ParamToDeleteMails,0,&tid)))
									{
										WaitForSingleObject(ThreadRunningEV,INFINITE);
										CloseHandle(NewThread);
									}
									// Enable write-access to mails
									#ifdef DEBUG_SYNCHRO
									DebugLog(SynchroFile,"MailBrowser:BTNDEL:ActualAccountMsgsSO-write done\n");
									#endif
									WriteDoneFcn(ActualAccount->MessagesAccessSO);
								}
							}
							else
								//else mark messages that they are not to be deleted
								SetRemoveFlagsInQueueFcn((HYAMNMAIL)ActualAccount->Mails,YAMN_MSG_DISPLAY | YAMN_MSG_USERDELETE,0,YAMN_MSG_USERDELETE,0);
						}
					}
					CloseHandle(ThreadRunningEV);
				}
				break;
			}
		}
			break;
		case WM_SIZE:
			if(wParam==SIZE_RESTORED)
			{
				LONG x=LOWORD(lParam);	//((LPRECT)lParam)->right-((LPRECT)lParam)->left;
				LONG y=HIWORD(lParam);	//((LPRECT)lParam)->bottom-((LPRECT)lParam)->top;
				MoveWindow(GetDlgItem(hDlg,IDC_BTNDEL),     5            ,y-5-25,(x-20)/3,25,TRUE);	//where to put DELETE button while resizing
				MoveWindow(GetDlgItem(hDlg,IDC_BTNCHECKALL),10+  (x-20)/3,y-5-25,(x-20)/6,25,TRUE);	//where to put CHECK ALL button while resizing				
				MoveWindow(GetDlgItem(hDlg,IDC_BTNAPP),     15+  (x-20)/3 + (x-20)/6,y-5-25,(x-20)/3,25,TRUE);	//where to put RUN APP button while resizing
				MoveWindow(GetDlgItem(hDlg,IDC_BTNOK),      20+2*(x-20)/3 + (x-20)/6 ,y-5-25,(x-20)/6,25,TRUE);	//where to put OK button while resizing
				MoveWindow(GetDlgItem(hDlg,IDC_LISTMAILS),  5         ,5     ,x-10    ,y-55,TRUE);	//where to put list mail window while resizing
				MoveWindow(GetDlgItem(hDlg,IDC_STSTATUS),   5         ,y-5-45     ,x-10    ,15,TRUE);	//where to put account status text while resizing
			}
//			break;
			return 0;
		case WM_GETMINMAXINFO:
			((LPMINMAXINFO)lParam)->ptMinTrackSize.x=MAILBROWSER_MINXSIZE;
			((LPMINMAXINFO)lParam)->ptMinTrackSize.y=MAILBROWSER_MINYSIZE;
			return 0;
		case WM_TIMER:
		{
			NOTIFYICONDATA nid;
			struct CMailWinUserInfo *mwui=(struct CMailWinUserInfo *)GetWindowLong(hDlg,DWL_USER);

			ZeroMemory(&nid,sizeof(nid));
			nid.cbSize=sizeof(NOTIFYICONDATA);
			nid.hWnd=hDlg;
			nid.uID=0;
			nid.uFlags=NIF_ICON;
			if(mwui->TrayIconState==0)
				nid.hIcon=hNeutralIcon;
			else
				nid.hIcon=hNewMailIcon;
			Shell_NotifyIcon(NIM_MODIFY,&nid);
			mwui->TrayIconState=!mwui->TrayIconState;
//			UpdateWindow(hDlg);
		}
			break;
		case WM_NOTIFY:
			switch(((LPNMHDR)lParam)->idFrom)
			{
				
				case IDC_LISTMAILS:
				{
					NM_LISTVIEW* pNMListView;

					switch(((LPNMHDR)lParam)->code)
					{
						case NM_DBLCLK:
							pNMListView = (NM_LISTVIEW*)lParam;
							int iSelect;
							iSelect=SendMessage(GetDlgItem(hDlg,IDC_LISTMAILS),LVM_GETNEXTITEM,-1,MAKELPARAM((UINT)LVNI_FOCUSED,0)); // return item selected

							if(iSelect!=-1) 
							{
								LV_ITEMW item;
								HYAMNMAIL ActualMail;

								item.iItem=iSelect;
								item.iSubItem=0;
								item.mask=LVIF_PARAM | LVIF_STATE;
								item.stateMask=0xFFFFFFFF;
								ListView_GetItem(GetDlgItem(hDlg,IDC_LISTMAILS),&item);
								ActualMail=(HYAMNMAIL)item.lParam;
								if(NULL!=ActualMail)
								{
									//MessageBox(NULL,ActualMail->MailData->Body,Translate("Mail source"),0);
									CreateDialogParamW(YAMNVar.hInst,MAKEINTRESOURCEW(IDD_DLGSHOWMESSAGE),NULL,(DLGPROC)DlgProcYAMNShowMessage,(LPARAM)ActualMail);
									//MoveWindow(hShowMessage,PosX,PosY,SizeX,SizeY,TRUE);
								}

							}
							break;
						

						case LVN_COLUMNCLICK:
							HACCOUNT ActualAccount;
							if(NULL==(ActualAccount=GetWindowAccount(hDlg)))
								break;

							pNMListView = (NM_LISTVIEW*)lParam;
							if(WAIT_OBJECT_0==WaitToReadFcn(ActualAccount->AccountAccessSO))
							{
								#ifdef DEBUG_SYNCHRO
								DebugLog(SynchroFile,"MailBrowser:COLUMNCLICK:ActualAccountSO-read enter\n");
								#endif
								switch((int)pNMListView->iSubItem)
								{
									case 0:
										bFrom = !bFrom;
										break;
									case 1:
										bSub = !bSub;
										break;
									case 2:
										bSize = !bSize;
										break;
									case 3:
										bDate = !bDate;
										break;
									default:
										break;
								}
								ListView_SortItems(pNMListView->hdr.hwndFrom,ListViewCompareProc,pNMListView->iSubItem);
								#ifdef DEBUG_SYNCHRO
								DebugLog(SynchroFile,"MailBrowser:BTNAPP:ActualAccountSO-read done\n");
								#endif
								ReadDoneFcn(ActualAccount->AccountAccessSO);
							}
							break;

						case NM_CUSTOMDRAW:
						{
							HACCOUNT ActualAccount;
							LPNMLVCUSTOMDRAW cd=(LPNMLVCUSTOMDRAW)lParam;
							DWORD PaintCode;

							if(NULL==(ActualAccount=GetWindowAccount(hDlg)))
								break;

							switch(cd->nmcd.dwDrawStage)
							{
								case CDDS_PREPAINT:
									PaintCode=CDRF_NOTIFYITEMDRAW;
									break;
								case CDDS_ITEMPREPAINT:
									PaintCode=CDRF_NOTIFYSUBITEMDRAW;
									break;
								case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
								{
//									COLORREF crText, crBkgnd;
//									crText= RGB(128,128,255);
									HYAMNMAIL ActualMail;
									BOOL umma;

									{
										struct CMailWinUserInfo *mwui;
										mwui=(struct CMailWinUserInfo *)GetWindowLong(hDlg,DWL_USER);
										umma= mwui->UpdateMailsMessagesAccess;
									}
									ActualMail=(HYAMNMAIL)cd->nmcd.lItemlParam;
									if(!ActualMail) 
										ActualMail=(HYAMNMAIL)readItemLParam(cd->nmcd.hdr.hwndFrom,cd->nmcd.dwItemSpec);
									#ifdef DEBUG_SYNCHRO
									DebugLog(SynchroFile,"MailBrowser:DRAWITEM:ActualAccountMsgsSO-read wait\n");
									#endif
									if(!umma)
										if(WAIT_OBJECT_0!=WaitToReadFcn(ActualAccount->MessagesAccessSO))
										{
											#ifdef DEBUG_SYNCHRO
											DebugLog(SynchroFile,"MailBrowser:DRAWITEM:ActualAccountMsgsSO-read wait failed\n");
											#endif
											return 0;
										}
									#ifdef DEBUG_SYNCHRO
									DebugLog(SynchroFile,"MailBrowser:DRAWITEM:ActualAccountMsgsSO-read enter\n");
									#endif
									switch(ActualMail->Flags & YAMN_MSG_SPAMMASK)
									{
										case YAMN_MSG_SPAML1:
										case YAMN_MSG_SPAML2:
											cd->clrText=RGB(150,150,150);
											break;
										case YAMN_MSG_SPAML3:
											cd->clrText=RGB(200,200,200);
											cd->clrTextBk=RGB(160,160,160);
											break;
										case 0:
											if(cd->nmcd.dwItemSpec & 1)
												cd->clrTextBk=RGB(230,230,230);
											break;
										default:
											break;
									}
									if(ActualMail->Flags & YAMN_MSG_UNSEEN)
										cd->clrTextBk=RGB(220,235,250);
									PaintCode=CDRF_DODEFAULT;

									if(!umma)
									{
										#ifdef DEBUG_SYNCHRO
										DebugLog(SynchroFile,"MailBrowser:DRAWITEM:ActualAccountMsgsSO-read done\n");
										#endif
										ReadDoneFcn(ActualAccount->MessagesAccessSO);
									}

									break;
								}
							}
							SetWindowLong(hDlg,DWL_MSGRESULT,PaintCode);
								return 1;
						}
					}
				}
			}
			break;
		default:
			return 0;
	}
//	return DefWindowProc(hDlg,msg,wParam,lParam);
	return 0;
}

LRESULT CALLBACK ListViewSubclassProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND hwndParent = GetParent(hDlg);
	
	int Items;

	switch(msg) {
		case WM_CHAR: case WM_SYSCHAR: case WM_KEYDOWN:
        {
			
            BOOL isCtrl = GetKeyState(VK_CONTROL) & 0x8000;
            BOOL isShift = GetKeyState(VK_SHIFT) & 0x8000;
            BOOL isAlt = GetKeyState(VK_MENU) & 0x8000;

			switch (wParam) 
			{
				case 1:
					if(!isAlt && !isShift && isCtrl) 
					{
						ListView_SetItemState(hDlg, -1, 0, LVIS_SELECTED); // deselect all items
						ListView_SetItemState(hDlg,-1, LVIS_SELECTED ,LVIS_SELECTED);
						Items = ListView_GetItemCount(hDlg);
						ListView_RedrawItems(hDlg, 0, Items);
						UpdateWindow(hDlg);
						SetFocus(hDlg);
					}
					break;

				case 46:
					HACCOUNT ActualAccount;
					if(NULL==(ActualAccount=GetWindowAccount(hwndParent)))
						break;
					LVITEMW item;
					HYAMNMAIL FirstMail=NULL,ActualMail;
					HANDLE ThreadRunningEV;
					DWORD tid,Total=0;

					//	we use event to signal, that running thread has all needed stack parameters copied
					if(NULL==(ThreadRunningEV=CreateEvent(NULL,FALSE,FALSE,NULL)))
						break;
					int Items=ListView_GetItemCount(hDlg);

					item.stateMask=0xFFFFFFFF;
					#ifdef DEBUG_SYNCHRO
					DebugLog(SynchroFile,"MailBrowser:BTNDEL:ActualAccountMsgsSO-write wait\n");
					#endif
					if(WAIT_OBJECT_0==WaitToWriteFcn(ActualAccount->MessagesAccessSO))
					{
						#ifdef DEBUG_SYNCHRO
						DebugLog(SynchroFile,"MailBrowser:BTNDEL:ActualAccountMsgsSO-write enter\n");
						#endif
						for(int i=0;i<Items;i++)
						{
							item.iItem=i;
							item.iSubItem=0;
							item.mask=LVIF_PARAM | LVIF_STATE;
							item.stateMask=0xFFFFFFFF;
							ListView_GetItem(hDlg,&item);
							ActualMail=(HYAMNMAIL)item.lParam;
							if(NULL==ActualMail)
								break;
							if(item.state & LVIS_SELECTED)
							{
								ActualMail->Flags|=YAMN_MSG_USERDELETE;	//set to mail we are going to delete it
								Total++;
							}
						}

						// Enable write-access to mails
						#ifdef DEBUG_SYNCHRO
						DebugLog(SynchroFile,"MailBrowser:BTNDEL:ActualAccountMsgsSO-write done\n");
						#endif
						WriteDoneFcn(ActualAccount->MessagesAccessSO);

						if(Total)
						{
							char DeleteMsg[1024];

							sprintf(DeleteMsg,Translate("Do you really want to delete %d selected mails?"),Total);
							if(IDOK==MessageBox(hDlg,DeleteMsg,Translate("Delete confirmation"),MB_OKCANCEL | MB_ICONWARNING))
							{
								struct DeleteParam ParamToDeleteMails={YAMN_DELETEVERSION,ThreadRunningEV,ActualAccount,NULL};

								// Find if there's mail marked to delete, which was deleted before
								#ifdef DEBUG_SYNCHRO
								DebugLog(SynchroFile,"MailBrowser:BTNDEL:ActualAccountMsgsSO-write wait\n");
								#endif
								if(WAIT_OBJECT_0==WaitToWriteFcn(ActualAccount->MessagesAccessSO))
								{
									#ifdef DEBUG_SYNCHRO
									DebugLog(SynchroFile,"MailBrowser:BTNDEL:ActualAccountMsgsSO-write enter\n");
									#endif
									for(ActualMail=(HYAMNMAIL)ActualAccount->Mails;ActualMail!=NULL;ActualMail=ActualMail->Next)
									{
										if((ActualMail->Flags & YAMN_MSG_DELETED) && ((ActualMail->Flags & YAMN_MSG_USERDELETE)))	//if selected mail was already deleted
										{
											DeleteMessageFromQueueFcn((HYAMNMAIL *)&ActualAccount->Mails,ActualMail,1);
											CallService(MS_YAMN_DELETEACCOUNTMAIL,(WPARAM)ActualAccount->Plugin,(LPARAM)ActualMail);	//delete it from memory
											continue;
										}
									}
									// Set flag to marked mails that they can be deleted
									SetRemoveFlagsInQueueFcn((HYAMNMAIL)ActualAccount->Mails,YAMN_MSG_DISPLAY | YAMN_MSG_USERDELETE,0,YAMN_MSG_DELETEOK,1);
									// Create new thread which deletes marked mails.
									HANDLE NewThread;

									if(NULL!=(NewThread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)ActualAccount->Plugin->Fcn->DeleteMailsFcnPtr,(LPVOID)&ParamToDeleteMails,0,&tid)))
									{
										WaitForSingleObject(ThreadRunningEV,INFINITE);
										CloseHandle(NewThread);
									}
									// Enable write-access to mails
									#ifdef DEBUG_SYNCHRO
									DebugLog(SynchroFile,"MailBrowser:BTNDEL:ActualAccountMsgsSO-write done\n");
									#endif
									WriteDoneFcn(ActualAccount->MessagesAccessSO);
								}
							}
							else
								//else mark messages that they are not to be deleted
								SetRemoveFlagsInQueueFcn((HYAMNMAIL)ActualAccount->Mails,YAMN_MSG_DISPLAY | YAMN_MSG_USERDELETE,0,YAMN_MSG_USERDELETE,0);
						}
					}
					CloseHandle(ThreadRunningEV);
					break;
			}
			
			break;

		}
	}
	return CallWindowProc(OldListViewSubclassProc, hDlg, msg, wParam, lParam);
}

DWORD WINAPI MailBrowser(LPVOID Param)
{
	MSG msg;

	HWND hMailBrowser;
	BOOL WndFound=FALSE;
	HACCOUNT ActualAccount;
	struct MailBrowserWinParam MyParam;

	MyParam=*(struct MailBrowserWinParam *)Param;
	ActualAccount=MyParam.account;	
	#ifdef DEBUG_SYNCHRO
	DebugLog(SynchroFile,"MailBrowser:Incrementing \"using threads\" %x (account %x)\n",ActualAccount->UsingThreads,ActualAccount);
	#endif
	SCIncFcn(ActualAccount->UsingThreads);

//	we will not use params in stack anymore
	SetEvent(MyParam.ThreadRunningEV);

	__try
	{
		#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"MailBrowser:ActualAccountSO-read wait\n");
		#endif
		if(WAIT_OBJECT_0!=WaitToReadFcn(ActualAccount->AccountAccessSO))
		{
			#ifdef DEBUG_SYNCHRO
			DebugLog(SynchroFile,"MailBrowser:ActualAccountSO-read wait failed\n");
			#endif
			return 0;
		}
		#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"MailBrowser:ActualAccountSO-read enter\n");
		#endif
		if(!(ActualAccount->AbilityFlags & YAMN_ACC_BROWSE))
		{
			MyParam.nflags=MyParam.nflags & ~YAMN_ACC_MSG;
			MyParam.nnflags=MyParam.nnflags & ~YAMN_ACC_MSG;
		}
		if(!(ActualAccount->AbilityFlags & YAMN_ACC_POPUP))
			MyParam.nflags=MyParam.nflags & ~YAMN_ACC_POP;
		#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"MailBrowser:ActualAccountSO-read done\n");
		#endif
		ReadDoneFcn(ActualAccount->AccountAccessSO);

		if(NULL!=(hMailBrowser=WindowList_Find(YAMNVar.NewMailAccountWnd,ActualAccount)))
			WndFound=TRUE;
		if((hMailBrowser==NULL) && ((MyParam.nflags & YAMN_ACC_MSG) || (MyParam.nflags & YAMN_ACC_ICO) || (MyParam.nnflags & YAMN_ACC_MSG)))
		{
			hMailBrowser=CreateDialogParamW(YAMNVar.hInst,MAKEINTRESOURCEW(IDD_DLGVIEWMESSAGES),NULL,(DLGPROC)DlgProcYAMNMailBrowser,(LPARAM)&MyParam);
			SendMessageW(hMailBrowser,WM_SETICON,(WPARAM)ICON_BIG,(LPARAM)hNewMailIcon);
			SendMessageW(hMailBrowser,WM_SETICON,(WPARAM)ICON_SMALL,(LPARAM)hNewMailIcon);
			MoveWindow(hMailBrowser,PosX,PosY,SizeX,SizeY,TRUE);
		}

		if(hMailBrowser!=NULL)
		{
			struct CChangeContent Params={MyParam.nflags,MyParam.nnflags};	//if this thread created window, just post message to update mails

			SendMessageW(hMailBrowser,WM_YAMN_CHANGECONTENT,(WPARAM)ActualAccount,(LPARAM)&Params);	//we ensure this will do the thread who created the browser window
		}
		else
			UpdateMails(NULL,ActualAccount,MyParam.nflags,MyParam.nnflags);	//update mails without displaying or refreshing any window

		if((hMailBrowser!=NULL) && !WndFound)		//we process message loop only for thread that created window
		{
			while(GetMessage(&msg,NULL,0,0))
			{
				TranslateMessage(&msg); 
				DispatchMessage(&msg);  
			}
		}

		if((!WndFound) && (ActualAccount->Plugin->Fcn!=NULL) && (ActualAccount->Plugin->Fcn->WriteAccountsFcnPtr!=NULL) && ActualAccount->AbleToWork)
			ActualAccount->Plugin->Fcn->WriteAccountsFcnPtr();
	}
	__finally
	{
		#ifdef DEBUG_SYNCHRO
		DebugLog(SynchroFile,"MailBrowser:Decrementing \"using threads\" %x (account %x)\n",ActualAccount->UsingThreads,ActualAccount);
		#endif
		SCDecFcn(ActualAccount->UsingThreads);
	}
	return 1;
}

int RunMailBrowserSvc(WPARAM wParam,LPARAM lParam)
{
	DWORD tid;
	//an event for successfull copy parameters to which point a pointer in stack for new thread
	HANDLE ThreadRunningEV;
	PYAMN_MAILBROWSERPARAM Param=(PYAMN_MAILBROWSERPARAM)wParam;

	if((DWORD)lParam!=YAMN_MAILBROWSERVERSION)
		return 0;

	if(NULL!=(ThreadRunningEV=CreateEvent(NULL,FALSE,FALSE,NULL)))
	{
		HANDLE NewThread;

		Param->ThreadRunningEV=ThreadRunningEV;
		if(NULL!=(NewThread=CreateThread(NULL,0,MailBrowser,Param,0,&tid)))
		{
			WaitForSingleObject(ThreadRunningEV,INFINITE);
			CloseHandle(NewThread);
		}
		CloseHandle(ThreadRunningEV);
		return 1;
	}
	return 0;
}
