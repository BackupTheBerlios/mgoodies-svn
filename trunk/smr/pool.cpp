#include "pool.h"


// Prototypes ///////////////////////////////////////////////////////////////////////////

static CRITICAL_SECTION update_cs;

typedef struct
{
	SortedList *queue;
	HANDLE hThread;
	DWORD dwThreadID;
	BOOL bThreadRunning;

} ThreadQueue;

ThreadQueue statusQueue;


struct QueueItem
{
	HANDLE hContact;
	DWORD check_time;
	BOOL priority;
};


UINT_PTR hTimer = 0;


void QueueAdd(HANDLE hContact, DWORD check_time, BOOL prio);
void QueueRemove(HANDLE hContact);
int QueueCompareItems(void *i1, void *i2);
int QueueSortItems(void *i1, void *i2);


BOOL ProtocolStatusAllowMsgs(HANDLE hContact, const char *protocol);
VOID CALLBACK PoolTimerAddContacts(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime);


DWORD WINAPI UpdateThread(LPVOID vParam);





// Functions ////////////////////////////////////////////////////////////////////////////


void InitPool() 
{
	int queuesize = CallService(MS_DB_CONTACT_GETCOUNT, 0, 0);

	// Init queue
	ZeroMemory(&statusQueue, sizeof(statusQueue));
	statusQueue.queue = List_Create(0, queuesize + 10);
	statusQueue.queue->sortFunc = QueueSortItems;
	statusQueue.queue->compareFunc = QueueCompareItems;
	statusQueue.bThreadRunning = TRUE;
	statusQueue.hThread = CreateThread(NULL, 16000, UpdateThread, NULL, 0, &statusQueue.dwThreadID);

	InitializeCriticalSection(&update_cs);
}


void FreePool()
{
	DWORD dwExitcode;
	int steps;

	// Stop queue
	steps = 0;
	statusQueue.bThreadRunning = FALSE;
	ResumeThread(statusQueue.hThread);
	do {
		Sleep(100);
		GetExitCodeThread(statusQueue.hThread, &dwExitcode);
		steps++;
	} while ( dwExitcode == STILL_ACTIVE && steps < 20 );
	if (statusQueue.hThread)
		CloseHandle(statusQueue.hThread);

	// Delete cs
	DeleteCriticalSection(&update_cs);

	// Free lists
	List_DestroyFreeContents(statusQueue.queue);
	mir_free(statusQueue.queue);
}


// Return true if this protocol has to be checked
BOOL PoolCheckProtocol(const char *protocol)
{
	char setting[256];

	if (protocol == NULL)
		return FALSE;

	mir_snprintf(setting, sizeof(setting), OPT_PROTOCOL_GETMSG, protocol);

	return (BOOL) DBGetContactSettingByte(NULL, MODULE_NAME, setting, FALSE);
}


// Return true if this contact has to be checked
BOOL PoolCheckContact(HANDLE hContact)
{
	return !DBGetContactSettingByte(hContact,"CList","Hidden",0) && 
		!DBGetContactSettingByte(hContact,"CList","NotOnList",0) &&
		DBGetContactSettingByte(hContact, MODULE_NAME, OPT_CONTACT_GETMSG, TRUE);
}


// Add a contact to the pool when the status of the contact has changed
void PoolStatusChangeAddContact(HANDLE hContact)
{
	char *proto = (char*) CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM) hContact, 0);

log(MODULE_NAME, "PoolStatusChangeAddContact", "[%d] [%s] Status changed", hContact, proto);

	if (proto != NULL && PoolCheckProtocol(proto) && PoolCheckContact(hContact))
	{
		// Delete some messages now (don't add to list...)
		if (ProtocolStatusAllowMsgs(hContact, proto))
		{
			if (opts.pool_clear_on_status_change)
				ClearStatusMessage(hContact);

			QueueAdd(hContact, GetTickCount() + opts.pool_timer_status * 1000, TRUE);
		}
		else
		{
			PoolRemoveContact(hContact);
			ClearStatusMessage(hContact);
		}
	}
}

void PoolAddAllContacts(int timer, const char *protocol)
{

log(MODULE_NAME, "PoolAddAllContacts", "Start");

	EnterCriticalSection(&update_cs);

	// Make list for next timer ...
	HANDLE hContact = (HANDLE)CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
	while (hContact)
	{
		char *proto = (char*)CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM)hContact, 0);

		if (proto != NULL && (protocol == NULL || strcmp(proto, protocol) == 0))
		{
			QueueAdd(hContact, GetTickCount() + timer, FALSE);

			// Delete if possible
			if (PoolCheckProtocol(proto) && PoolCheckContact(hContact) && !ProtocolStatusAllowMsgs(hContact, proto))
			{
				ClearStatusMessage(hContact);
			}
		}

		hContact = (HANDLE)CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM)hContact, 0);
	}

	LeaveCriticalSection(&update_cs);

log(MODULE_NAME, "PoolAddAllContacts", "End");

}

// Add a contact to the pool when the status of the contact has changed
VOID CALLBACK PoolTimerAddContacts(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
{
	if (hTimer != NULL) {
		KillTimer(NULL, hTimer);
		hTimer = NULL;
	}

	PoolAddAllContacts(opts.pool_timer_check * 60000, NULL);

	PoolSetTimer();
}


void PoolSetTimer(void)
{
	if (hTimer != NULL)
		KillTimer(NULL, hTimer);

	if (opts.pool_check_on_timer)
		hTimer = SetTimer(NULL, 0, opts.pool_check_on_timer * 60000, PoolTimerAddContacts);
}


// Remove a contact from the current pool
void PoolRemoveContact(HANDLE hContact)
{
	QueueRemove(hContact);
}


void QueueRemove(HANDLE hContact)
{
log(MODULE_NAME, "QueueRemove", "[%d]", hContact);

	// Add this to thread...
	QueueItem *item = (QueueItem *) mir_alloc(sizeof(QueueItem));

	if (item == NULL)
		return;

	EnterCriticalSection(&update_cs);

	item->hContact = hContact;

	List_RemoveByValueFreeContents(statusQueue.queue, item);

	LeaveCriticalSection(&update_cs);

	mir_free(item);
}


// Add an contact to the pool queue
void QueueAdd(HANDLE hContact, DWORD check_time, BOOL prio)
{
log(MODULE_NAME, "QueueAdd", "[%d]", hContact);

	// Add this to thread...
	QueueItem *item = (QueueItem *) mir_alloc(sizeof(QueueItem));

	if (item == NULL)
		return;

	item->hContact = hContact;
	item->check_time = check_time;
	item->priority = prio;


	EnterCriticalSection(&update_cs);

	if (prio)
	{
		// Remove from timer
		List_RemoveByValueFreeContents(statusQueue.queue, item);

		List_InsertOrdered(statusQueue.queue, item);
	}
	else
	{
		if (List_IndexOf(statusQueue.queue, item) == -1)
			List_InsertOrdered(statusQueue.queue, item);
		else
			mir_free(item);
	}

	LeaveCriticalSection(&update_cs);

	ResumeThread(statusQueue.hThread);
}

int QueueCompareItems(void *i1, void *i2)
{
	return (int)((QueueItem*)i1)->hContact - (int)((QueueItem*)i2)->hContact;
}

// Itens with higher priority at end
int QueueSortItems(void *i1, void *i2)
{
	if (((QueueItem*)i1)->priority != ((QueueItem*)i2)->priority)
		return ((QueueItem*)i1)->priority - ((QueueItem*)i2)->priority;
	else
		return ((QueueItem*)i2)->check_time - ((QueueItem*)i1)->check_time;
}


BOOL ProtocolStatusAllowMsgs(HANDLE hContact, const char *protocol)
{
	int status = CallProtoService(protocol, PS_GETSTATUS, 0, 0);

	// Exclude offline, connecting and invisible
	if (status >= ID_STATUS_ONLINE && status <= ID_STATUS_OUTTOLUNCH && status != ID_STATUS_INVISIBLE) 
	{
		int contact_status = DBGetContactSettingWord(hContact, protocol, "Status", 0);

		DWORD protoStatusFlags = CallProtoService(protocol, PS_GETCAPS, PFLAGNUM_3, 0);
		if (protoStatusFlags & Proto_Status2Flag(contact_status))
		{
			return TRUE;// here you know the proto named protocol supports status i
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		return FALSE;
	}
}


DWORD WINAPI UpdateThread(LPVOID vParam)
{
	// Initial timer
	Sleep(INITIAL_TIMER);

log(MODULE_NAME, "UpdateThread", "Start");

	while (statusQueue.bThreadRunning)
	{
		EnterCriticalSection(&update_cs);

		if (!List_HasItens(statusQueue.queue))
		{
			LeaveCriticalSection(&update_cs);

log(MODULE_NAME, "UpdateThread", "Has no contacts in list");

			// Stop this one...
			if (statusQueue.bThreadRunning)
				SuspendThread(statusQueue.hThread);
		}
		else
		{
			// Get next job...

			/*
			 * the thread is awake, processing the update queue one by one entry with a given delay
			 * of UPDATE_DELAY seconds. The delay ensures that no protocol will kick us because of
			 * flood protection(s)
			 */
			QueueItem *qi = (QueueItem *) List_Peek(statusQueue.queue);

			if (qi->check_time > GetTickCount()) 
			{
				LeaveCriticalSection(&update_cs);

log(MODULE_NAME, "UpdateThread", "No time to check contact yet");

				if (statusQueue.bThreadRunning)
					Sleep(UPDATE_DELAY);
			}
			else
			{
				qi = (QueueItem *) List_Pop(statusQueue.queue);

				char *proto = (char *)CallService(MS_PROTO_GETCONTACTBASEPROTO, (WPARAM)qi->hContact, 0);

log(MODULE_NAME, "UpdateThread", "[%d] [%s] Checking", qi->hContact, proto);

				if (proto && PoolCheckProtocol(proto) && PoolCheckContact(qi->hContact))
				{
					if (ProtocolStatusAllowMsgs(qi->hContact, proto))
					{
						CallContactService(qi->hContact,PSS_GETAWAYMSG,0,0);

						LeaveCriticalSection(&update_cs);

log(MODULE_NAME, "UpdateThread", "[%d] [%s] Checked", qi->hContact, proto);

						if (statusQueue.bThreadRunning)
							Sleep(UPDATE_DELAY);
					}
					else
					{
						ClearStatusMessage(qi->hContact);

						LeaveCriticalSection(&update_cs);
					}
				}
				else
				{
					LeaveCriticalSection(&update_cs);
				}
			}
		}
	}

log(MODULE_NAME, "UpdateThread", "End");

	return 0;
}