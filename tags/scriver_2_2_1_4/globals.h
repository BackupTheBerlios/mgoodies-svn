#ifndef SRMM_GLOBALS_H
#define SRMM_GLOBALS_H

#define SMF_SHOWBTNS        0x00000002
#define SMF_SHOWTYPING      0x00000008
#define SMF_SHOWTYPINGWIN   0x00000010
#define SMF_SHOWTYPINGTRAY  0x00000020
#define SMF_SHOWTYPINGCLIST 0x00000040
#define SMF_SHOWICONS       0x00000080
#define SMF_SHOWTIME        0x00000100
#define SMF_AVATAR          0x00000200
#define SMF_SHOWDATE        0x00000400
#define SMF_HIDENAMES       0x00000800
#define SMF_SHOWSTATUSICON  0x00001000

#define SMF_SHOWSTATUSBAR	0x00010000
#define SMF_USETABS			0x00020000
#define SMF_TABSATBOTTOM	0x00040000

#define SMF_LONGDATE		0x00100000
#define SMF_RELATIVEDATE	0x00200000
#define SMF_SHOWSECONDS		0x00400000
#define SMF_GROUPMESSAGES	0x00800000
#define SMF_MARKFOLLOWUPS	0x01000000
#define SMF_RTL				0x02000000
#define SMF_DISABLE_UNICODE	0x04000000


#define SMF_ICON_ADD         0
#define SMF_ICON_USERDETAILS 1
#define SMF_ICON_HISTORY     2
//#define SMF_ICON_ARROW       3
#define SMF_ICON_SEND		 3
#define SMF_ICON_CANCEL		 4
#define SMF_ICON_SMILEY		 5
#define SMF_ICON_TYPING      6
#define SMF_ICON_UNICODEON	 7
#define SMF_ICON_UNICODEOFF	 8
#define SMF_ICON_DELIVERING	 9

#define SMF_ICON_INCOMING	 10
#define SMF_ICON_OUTGOING	 11
#define SMF_ICON_NOTICE		 12

struct GlobalMessageData
{
	unsigned int flags;
	HICON hIcons[16];
	HANDLE hMessageWindowList;
	HANDLE hParentWindowList;
	HWND	hParent;
	int		protoNum;
	char **	protoNames;
	HIMAGELIST hIconList;
};

void InitGlobals();
void FreeGlobals();
void ReloadGlobals();
void SetIcoLibIcons();
void LoadProtocolIcons();
void LoadGlobalIcons();
int ScriverRestoreWindowPosition(HWND hwnd,HANDLE hContact,const char *szModule,const char *szNamePrefix, int flags, int showCmd);

extern struct GlobalMessageData *g_dat;

#endif
