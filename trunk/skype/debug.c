#include "debug.h"

#ifdef _DEBUG
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#define INITBUF 1024		/* Initial size of buffer */

static CRITICAL_SECTION m_WriteFileMutex;
static FILE *m_fpLogFile = INVALID_HANDLE_VALUE;
static char *m_szLogBuf = NULL;
static DWORD m_iBufSize = 0;

void init_debug(void) {
	char *p;
	char logfile[MAX_PATH];
		
	ZeroMemory(logfile, sizeof(logfile));
	p=logfile+GetModuleFileName(NULL, logfile, sizeof(logfile));
	if (!(p=strrchr (logfile, '\\'))) p=logfile; else p++;
	strcpy (p, "skype_log.txt");
	m_szLogBuf = calloc (1, (m_iBufSize = INITBUF));
	m_fpLogFile = fopen(logfile, "a");
	InitializeCriticalSection(&m_WriteFileMutex);
}

void end_debug (void) {
	if (m_szLogBuf) free (m_szLogBuf);
	if (m_fpLogFile) fclose (m_fpLogFile);
	DeleteCriticalSection(&m_WriteFileMutex);
}

void do_log(const char *pszFormat, ...) {
	char *ct, *pNewBuf;
	va_list ap;
	time_t lt;
	int iLen;

	if (!m_szLogBuf || !m_fpLogFile) return;
	EnterCriticalSection(&m_WriteFileMutex);
	time(&lt);
	ct=ctime(&lt);
	ct[strlen(ct)-1]=0;
	do
	{
		va_start(ap, pszFormat);
		iLen = _vsnprintf(m_szLogBuf, m_iBufSize, pszFormat, ap); 
		va_end(ap);
		if (iLen == -1)
		{
		  if (!(pNewBuf = (char*)realloc (m_szLogBuf, m_iBufSize*2)))
		  {
			  iLen = strlen (m_szLogBuf);
			  break;
		  }
		  m_szLogBuf = pNewBuf;
		  m_iBufSize*=2;
		}
	} while (iLen == -1);
	fprintf (m_fpLogFile, "%s   %s\n", ct, m_szLogBuf);
	fflush (m_fpLogFile);
	LeaveCriticalSection(&m_WriteFileMutex);
}
#endif