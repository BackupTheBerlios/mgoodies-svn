#include "headers.h"
#include "shake.h"
#include "main.h"
#include "options.h"



int NudgeOptInit(WPARAM wParam,LPARAM lParam)
{
	OPTIONSDIALOGPAGE odp = { 0 };
	odp.cbSize						= sizeof(odp);
	odp.position					= -790000000;
	odp.hInstance					= hInst;
	odp.pszTemplate					= MAKEINTRESOURCEA(IDD_OPTIONS);
	odp.pszTitle					= "Nudge";
	odp.pszGroup					= "Events";
	odp.flags						= ODPF_BOLDGROUPS;
//	odp.nIDBottomSimpleControl = IDC_STMSNGROUP;
	odp.pfnDlgProc					= OptionsDlgProc;
	CallService( MS_OPT_ADDPAGE, wParam,( LPARAM )&odp );
	return 0;
}

int InitOptions()
{
	HookEvent(ME_OPT_INITIALISE, NudgeOptInit);
	return 0;
}

static BOOL CALLBACK OptionsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static int iInit = TRUE;
   
   switch(msg)
   {
      case WM_INITDIALOG:
      {
         TCITEM tci;
         RECT rcClient;
         GetClientRect(hwnd, &rcClient);

		 iInit = TRUE;
         tci.mask = TCIF_PARAM|TCIF_TEXT;
         tci.lParam = (LPARAM)CreateDialog(hInst,MAKEINTRESOURCE(IDD_OPT_NUDGE), hwnd, DlgProcNudgeOpt);
         tci.pszText = TranslateT("Nudge");
		 TabCtrl_InsertItem(GetDlgItem(hwnd, IDC_OPTIONSTAB), 0, &tci);
         MoveWindow((HWND)tci.lParam,5,26,rcClient.right-8,rcClient.bottom-29,1);

         tci.lParam = (LPARAM)CreateDialog(hInst,MAKEINTRESOURCE(IDD_OPT_SHAKE),hwnd,DlgProcShakeOpt);
         tci.pszText = TranslateT("Window Shaking");
         TabCtrl_InsertItem(GetDlgItem(hwnd, IDC_OPTIONSTAB), 1, &tci);
         MoveWindow((HWND)tci.lParam,5,26,rcClient.right-8,rcClient.bottom-29,1);
         ShowWindow((HWND)tci.lParam, SW_HIDE);
         iInit = FALSE;
         return FALSE;
      }

      case PSM_CHANGED: // used so tabs dont have to call SendMessage(GetParent(GetParent(hwnd)), PSM_CHANGED, 0, 0);
         if(!iInit)
             SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
         break;
      case WM_NOTIFY:
         switch(((LPNMHDR)lParam)->idFrom) {
            case 0:
               switch (((LPNMHDR)lParam)->code)
               {
                  case PSN_APPLY:
                     {
                        TCITEM tci;
                        int i,count;
                        tci.mask = TCIF_PARAM;
                        count = TabCtrl_GetItemCount(GetDlgItem(hwnd,IDC_OPTIONSTAB));
                        for (i=0;i<count;i++)
                        {
                           TabCtrl_GetItem(GetDlgItem(hwnd,IDC_OPTIONSTAB),i,&tci);
                           SendMessage((HWND)tci.lParam,WM_NOTIFY,0,lParam);
                        }
						CNudgeElement *n;
						for(n = NudgeList;n != NULL; n = n->next)
						{
							n->popupTimeSec = popupTime;
							n->showPopup = bShowPopup;
							n->popupBackColor = colorBack;
							n->popupTextColor = colorText;
							n->popupWindowColor = bUseWindowColor;
						}
						
                     }
                  break;
               }
            break;
            case IDC_OPTIONSTAB:
               switch (((LPNMHDR)lParam)->code)
               {
                  case TCN_SELCHANGING:
                     {
                        TCITEM tci;
                        tci.mask = TCIF_PARAM;
                        TabCtrl_GetItem(GetDlgItem(hwnd,IDC_OPTIONSTAB),TabCtrl_GetCurSel(GetDlgItem(hwnd,IDC_OPTIONSTAB)),&tci);
                        ShowWindow((HWND)tci.lParam,SW_HIDE);                     
                     }
                  break;
                  case TCN_SELCHANGE:
                     {
                        TCITEM tci;
                        tci.mask = TCIF_PARAM;
                        TabCtrl_GetItem(GetDlgItem(hwnd,IDC_OPTIONSTAB),TabCtrl_GetCurSel(GetDlgItem(hwnd,IDC_OPTIONSTAB)),&tci);
                        ShowWindow((HWND)tci.lParam,SW_SHOW);                     
                     }
                  break;
               }
            break;

         }
      break;
   }
   return FALSE;
}

BOOL CALLBACK DlgProcShakeOpt(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch(msg)
	{
		case WM_INITDIALOG:
			char szBuf[20];
			TranslateDialogDefault(hwnd);
			_snprintf(szBuf, 10, "%d", nMoveClist);
			SetWindowTextA(GetDlgItem(hwnd, IDC_LNUMBER_CLIST), szBuf);
			_snprintf(szBuf, 10, "%d", nMoveChat);
			SetWindowTextA(GetDlgItem(hwnd, IDC_LNUMBER_CHAT), szBuf);

			_snprintf(szBuf, 10, "%d", nScaleClist);
			SetWindowTextA(GetDlgItem(hwnd, IDC_LSCALE_CLIST), szBuf);
			_snprintf(szBuf, 10, "%d", nScaleChat);
			SetWindowTextA(GetDlgItem(hwnd, IDC_LSCALE_CHAT), szBuf);

			SendDlgItemMessage(hwnd, IDC_SNUMBER_CLIST, TBM_SETRANGE, 0, (LPARAM)MAKELONG(1, 60));
			SendDlgItemMessage(hwnd, IDC_SNUMBER_CHAT, TBM_SETRANGE, 0, (LPARAM)MAKELONG(1, 60));

			SendDlgItemMessage(hwnd, IDC_SNUMBER_CLIST, TBM_SETPOS, TRUE, nMoveClist);
			SendDlgItemMessage(hwnd, IDC_SNUMBER_CHAT, TBM_SETPOS, TRUE, nMoveChat);

			SendDlgItemMessage(hwnd, IDC_SSCALE_CLIST, TBM_SETRANGE, 0, (LPARAM)MAKELONG(1, 40));
			SendDlgItemMessage(hwnd, IDC_SSCALE_CHAT, TBM_SETRANGE, 0, (LPARAM)MAKELONG(1, 40));

			SendDlgItemMessage(hwnd, IDC_SSCALE_CLIST, TBM_SETPOS, TRUE, nMoveClist);
			SendDlgItemMessage(hwnd, IDC_SSCALE_CHAT, TBM_SETPOS, TRUE, nMoveChat);

			CheckDlgButton(hwnd,IDC_CHECKCLIST,bShakeClist);
			CheckDlgButton(hwnd,IDC_CHECKCHAT,bShakeChat);
			EnableWindow(GetDlgItem(hwnd,IDC_SSCALE_CLIST),IsDlgButtonChecked(hwnd,IDC_CHECKCLIST)==BST_CHECKED);
			EnableWindow(GetDlgItem(hwnd,IDC_SSCALE_CHAT),IsDlgButtonChecked(hwnd,IDC_CHECKCHAT)==BST_CHECKED);
			EnableWindow(GetDlgItem(hwnd,IDC_SNUMBER_CLIST),(IsDlgButtonChecked(hwnd,IDC_CHECKCLIST)==BST_CHECKED));
			EnableWindow(GetDlgItem(hwnd,IDC_SNUMBER_CHAT),(IsDlgButtonChecked(hwnd,IDC_CHECKCHAT)==BST_CHECKED));
			EnableWindow(GetDlgItem(hwnd,IDC_LSCALE_CLIST),(IsDlgButtonChecked(hwnd,IDC_CHECKCLIST)==BST_CHECKED));
			EnableWindow(GetDlgItem(hwnd,IDC_LSCALE_CHAT),(IsDlgButtonChecked(hwnd,IDC_CHECKCHAT)==BST_CHECKED));
			EnableWindow(GetDlgItem(hwnd,IDC_LNUMBER_CLIST),(IsDlgButtonChecked(hwnd,IDC_CHECKCLIST)==BST_CHECKED));
			EnableWindow(GetDlgItem(hwnd,IDC_LNUMBER_CHAT),(IsDlgButtonChecked(hwnd,IDC_CHECKCHAT)==BST_CHECKED));
			break;
		case WM_COMMAND:
		{
			WORD wNotifyCode = HIWORD(wParam);
			switch(LOWORD(wParam))
			{
				case IDC_PREVIEW:
					ShakeClist(0,0);
					//SendMessage(GetParent(hwnd),PSM_CHANGED,0,0);
					break;
				case IDC_CHECKCLIST:
				case IDC_CHECKCHAT:
					EnableWindow(GetDlgItem(hwnd,IDC_SSCALE_CLIST),IsDlgButtonChecked(hwnd,IDC_CHECKCLIST)==BST_CHECKED);
					EnableWindow(GetDlgItem(hwnd,IDC_SSCALE_CHAT),IsDlgButtonChecked(hwnd,IDC_CHECKCHAT)==BST_CHECKED);
					EnableWindow(GetDlgItem(hwnd,IDC_SNUMBER_CLIST),(IsDlgButtonChecked(hwnd,IDC_CHECKCLIST)==BST_CHECKED));
					EnableWindow(GetDlgItem(hwnd,IDC_SNUMBER_CHAT),(IsDlgButtonChecked(hwnd,IDC_CHECKCHAT)==BST_CHECKED));
					EnableWindow(GetDlgItem(hwnd,IDC_LSCALE_CLIST),(IsDlgButtonChecked(hwnd,IDC_CHECKCLIST)==BST_CHECKED));
					EnableWindow(GetDlgItem(hwnd,IDC_LSCALE_CHAT),(IsDlgButtonChecked(hwnd,IDC_CHECKCHAT)==BST_CHECKED));
					EnableWindow(GetDlgItem(hwnd,IDC_LNUMBER_CLIST),(IsDlgButtonChecked(hwnd,IDC_CHECKCLIST)==BST_CHECKED));
					EnableWindow(GetDlgItem(hwnd,IDC_LNUMBER_CHAT),(IsDlgButtonChecked(hwnd,IDC_CHECKCHAT)==BST_CHECKED));
					SendMessage(GetParent(hwnd),PSM_CHANGED,0,0);
					break;
			}
			break;
		}
		case WM_HSCROLL:
            if((HWND)lParam == GetDlgItem(hwnd, IDC_SNUMBER_CLIST) || (HWND)lParam == GetDlgItem(hwnd, IDC_SNUMBER_CHAT) 
				|| (HWND)lParam == GetDlgItem(hwnd, IDC_SSCALE_CLIST) || (HWND)lParam == GetDlgItem(hwnd, IDC_SSCALE_CHAT)) 
			{
                char szBuf[20];
                DWORD dwPos = SendMessage((HWND) lParam, TBM_GETPOS, 0, 0);
                _snprintf(szBuf, 10, "%d", dwPos);
                if ((HWND)lParam == GetDlgItem(hwnd, IDC_SNUMBER_CLIST))
                    SetWindowTextA(GetDlgItem(hwnd, IDC_LNUMBER_CLIST), szBuf);
                if ((HWND)lParam == GetDlgItem(hwnd, IDC_SNUMBER_CHAT))
                    SetWindowTextA(GetDlgItem(hwnd, IDC_LNUMBER_CHAT), szBuf);
				if ((HWND)lParam == GetDlgItem(hwnd, IDC_SSCALE_CLIST))
                    SetWindowTextA(GetDlgItem(hwnd, IDC_LSCALE_CLIST), szBuf);
                if ((HWND)lParam == GetDlgItem(hwnd, IDC_SSCALE_CHAT))
                    SetWindowTextA(GetDlgItem(hwnd, IDC_LSCALE_CHAT), szBuf);
                SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
            }
			break;

		case WM_SHOWWINDOW:
			break;

		case WM_NOTIFY:
			switch(((LPNMHDR)lParam)->idFrom)
			{
				case 0:
					switch(((LPNMHDR)lParam)->code)
					{
						case PSN_APPLY:
						{
							nMoveClist = (int) SendMessage((HWND) GetDlgItem(hwnd, IDC_SNUMBER_CLIST), TBM_GETPOS, 0, 0);
							nMoveChat = (int) SendMessage((HWND) GetDlgItem(hwnd, IDC_SNUMBER_CHAT), TBM_GETPOS, 0, 0);
							nScaleClist = (int) SendMessage((HWND) GetDlgItem(hwnd, IDC_SSCALE_CLIST), TBM_GETPOS, 0, 0);
							nScaleChat = (int) SendMessage((HWND) GetDlgItem(hwnd, IDC_SSCALE_CHAT), TBM_GETPOS, 0, 0);
							bShakeClist = (IsDlgButtonChecked(hwnd,IDC_CHECKCLIST)==BST_CHECKED);
							bShakeChat = (IsDlgButtonChecked(hwnd,IDC_CHECKCHAT)==BST_CHECKED);
						}
					}
			}
			break;
	}

	return FALSE;
}

void CreateImageList(HWND hWnd)
{
	// Create and populate image list
	HIMAGELIST hImList = ImageList_Create(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),	ILC_MASK | ILC_COLOR32, nProtocol, 0);

	CNudgeElement *n;
	for(n = NudgeList;n != NULL; n = n->next)
	{
		HICON hIcon = NULL;
		hIcon=(HICON)CallProtoService(n->ProtocolName, PS_LOADICON,PLI_PROTOCOL | PLIF_SMALL, 0);
		if (hIcon == NULL || (int)hIcon == CALLSERVICE_NOTFOUND) 
		{
			hIcon=(HICON)CallProtoService(n->ProtocolName, PS_LOADICON, PLI_PROTOCOL, 0);
		}
 
		if (hIcon == NULL || (int)hIcon == CALLSERVICE_NOTFOUND) 
			hIcon = (HICON) LoadImage(hInst, MAKEINTRESOURCE(IDI_NUDGE), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);

		ImageList_AddIcon(hImList, hIcon);
		DestroyIcon(hIcon);
	}
	//ADD default Icon for nudge
	HICON hIcon = NULL;
	hIcon = (HICON) LoadImage(hInst, MAKEINTRESOURCE(IDI_NUDGE), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
	ImageList_AddIcon(hImList, hIcon);
	DestroyIcon(hIcon);

	HWND hLstView = GetDlgItem(hWnd, IDC_PROTOLIST);
	TreeView_SetImageList(hLstView, hImList, TVSIL_NORMAL);
}

void PopulateProtocolList(HWND hWnd)
{
	bool useOne = IsDlgButtonChecked(hWnd, IDC_USEBYPROTOCOL) == BST_UNCHECKED;

	HWND hLstView = GetDlgItem(hWnd, IDC_PROTOLIST);

	TreeView_DeleteAllItems(hLstView);

	TVINSERTSTRUCT tvi = {0};
	tvi.hParent = TVI_ROOT;
	tvi.hInsertAfter = TVI_LAST;
	tvi.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_STATE | TVIF_SELECTEDIMAGE;
	tvi.item.stateMask = TVIS_STATEIMAGEMASK | TVIS_SELECTED;

	CNudgeElement *n;
	int i = 0;
	if (!useOne)
	{
		for(n = NudgeList;n != NULL; n = n->next)
		{
			tvi.item.pszText = (TCHAR*)n->ProtocolName;
			tvi.item.iImage  = i;
			tvi.item.iSelectedImage = i;
			tvi.item.state = 2;	
			TreeView_InsertItem(hLstView, &tvi);
		}
	}
	else
	{
		tvi.item.pszText = Translate("Nudge");
		tvi.item.iImage  = nProtocol + 1;
		tvi.item.iSelectedImage = nProtocol + 1;
		tvi.item.state = 2;	
		TreeView_InsertItem(hLstView, &tvi);

	}
	TreeView_SelectItem(hLstView, TreeView_GetRoot(hLstView));
}


BOOL CALLBACK DlgProcNudgeOpt(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch(msg)
	{
		case WM_INITDIALOG:
			TranslateDialogDefault(hwnd);
			CreateImageList(hwnd);
			PopulateProtocolList(hwnd);
			CheckDlgButton(hwnd,IDC_CHECKPOP, (WPARAM) bShowPopup);
			CheckDlgButton(hwnd,IDC_USEWINCOLORS, (WPARAM) bUseWindowColor);
			SetDlgItemInt(hwnd,IDC_POPUPTIME, popupTime,FALSE);
			SendDlgItemMessage(hwnd,IDC_POPUPBACKCOLOR,CPM_SETCOLOUR,0,(LPARAM) colorBack);
			SendDlgItemMessage(hwnd,IDC_POPUPTEXTCOLOR,CPM_SETCOLOUR,0,(LPARAM) colorText);
			EnableWindow(GetDlgItem(hwnd,IDC_CHECKPOP),bShowPopup);
			EnableWindow(GetDlgItem(hwnd,IDC_USEWINCOLORS),bUseWindowColor);
			EnableWindow(GetDlgItem(hwnd,IDC_POPUPBACKCOLOR),bShowPopup && ! bUseWindowColor);
			EnableWindow(GetDlgItem(hwnd,IDC_POPUPTEXTCOLOR),bShowPopup && ! bUseWindowColor);
			EnableWindow(GetDlgItem(hwnd,IDC_POPUPTIME),bShowPopup);
			break;
		case WM_COMMAND:
		{
			WORD wNotifyCode = HIWORD(wParam);
			switch(LOWORD(wParam))
			{
				case IDC_PREVIEW:
					Preview();
					break;
				case IDC_POPUPTIME:
				case IDC_POPUPTEXTCOLOR:
				case IDC_POPUPBACKCOLOR:
					colorBack = SendDlgItemMessage(hwnd,IDC_POPUPBACKCOLOR,CPM_GETCOLOUR,0,0);
					colorText = SendDlgItemMessage(hwnd,IDC_POPUPTEXTCOLOR,CPM_GETCOLOUR,0,0);
					SendMessage(GetParent(hwnd),PSM_CHANGED,0,0);
					break;
				case IDC_USEWINCOLORS:
					bUseWindowColor = (IsDlgButtonChecked(hwnd,IDC_USEWINCOLORS)==BST_CHECKED);
					EnableWindow(GetDlgItem(hwnd,IDC_POPUPBACKCOLOR), bShowPopup && ! bUseWindowColor);
					EnableWindow(GetDlgItem(hwnd,IDC_POPUPTEXTCOLOR), bShowPopup && ! bUseWindowColor);
					SendMessage(GetParent(hwnd),PSM_CHANGED,0,0);
					break;
				case IDC_CHECKPOP:
					bShowPopup = (IsDlgButtonChecked(hwnd,IDC_CHECKPOP)==BST_CHECKED);
					EnableWindow(GetDlgItem(hwnd,IDC_USEWINCOLORS),bShowPopup);
					EnableWindow(GetDlgItem(hwnd,IDC_POPUPBACKCOLOR),bShowPopup && ! bUseWindowColor);
					EnableWindow(GetDlgItem(hwnd,IDC_POPUPTEXTCOLOR),bShowPopup && ! bUseWindowColor);
					EnableWindow(GetDlgItem(hwnd,IDC_POPUPTIME),bShowPopup);
					SendMessage(GetParent(hwnd),PSM_CHANGED,0,0);
					break;
				case IDC_USEBYPROTOCOL:
					PopulateProtocolList(hwnd);
					SendMessage(GetParent(hwnd),PSM_CHANGED,0,0);
					break;
			}
			break;
		}
		case WM_SHOWWINDOW:
			break;

		case WM_NOTIFY:
			switch(((LPNMHDR)lParam)->idFrom)
			{
				case 0:
					switch(((LPNMHDR)lParam)->code)
					{
						case PSN_APPLY:
						{
							BOOL Translated;
							popupTime = GetDlgItemInt(hwnd,IDC_POPUPTIME,&Translated,FALSE);
							bUseWindowColor = (IsDlgButtonChecked(hwnd,IDC_USEWINCOLORS)==BST_CHECKED);
							bShowPopup = (IsDlgButtonChecked(hwnd,IDC_CHECKPOP)==BST_CHECKED);
						}
					}
			}
			break;
	}

	return FALSE;
}
