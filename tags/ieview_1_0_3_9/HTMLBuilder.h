/*

IEView Plugin for Miranda IM
Copyright (C) 2005  Piotr Piastucki

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
class HTMLBuilder;

#ifndef HTMLBUILDER_INCLUDED
#define HTMLBUILDER_INCLUDED

#include "IEView.h"
#include "m_MathModule.h"

class TextToken {
private:
	int  type;
	bool end;
	int  tag;
	DWORD value;
	wchar_t *wtext;
	char *text;
	wchar_t *wlink;
	char *link;
	TextToken *next;
	static TextToken* 	tokenizeBBCodes(const wchar_t *text, int len);
public:
	enum TOKENS {
		END      = 0,
		TEXT,
		LINK,
		WWWLINK,
		SMILEY,
		BBCODE,
		MATH,
	};
	enum BBCODES {
		BB_B = 0,
		BB_I,
		BB_U,
		BB_COLOR,
		BB_SIZE,
		BB_IMG,
	};
	TextToken(int type, const char *text, int len);
	TextToken(int type, const wchar_t *wtext, int len);
	~TextToken();
	int 				getType();
	const char *		getText();
	const wchar_t*      getTextW();
	const char *		getLink();
	const wchar_t *		getLinkW();
	void 				setLink(const char *link);
	void 				setLink(const wchar_t *wlink);
	int 				getTag();
	void                setTag(int);
	bool 				isEnd();
	void                setEnd(bool);
	TextToken *			getNext();
	void   				setNext(TextToken *);
//	void				toString(char **str, int *sizeAlloced);
	void				toString(wchar_t **str, int *sizeAlloced);
//	static char *		urlEncode(const char *str);
//	static char *		urlEncode2(const char *str);
//	static TextToken* 	tokenizeLinks(const char *text);
//	static TextToken*	tokenizeSmileys(const char *proto, const char *text);
	// UNICODE
	wchar_t *			urlEncode(const wchar_t *str);
	static TextToken* 	tokenizeLinks(const wchar_t *wtext);
	static TextToken* 	tokenizeSmileys(const char *proto, const wchar_t *wtext);
	static TextToken* 	tokenizeBBCodes(const wchar_t *text);
	static TextToken* 	tokenizeMath(const wchar_t *text);
};

class HTMLBuilder {
private:
	static int mimFlags;
	enum MIMFLAGS {
		MIM_CHECKED = 1,
		MIM_UNICODE = 2
	};
protected:
	DWORD lastEventTime;
	int iLastEventType;
    enum ENCODEFLAGS {
        ENF_NONE = 0,
        ENF_SMILEYS = 1,
        ENF_NAMESMILEYS = 2,
        ENF_BBCODES = 4,
        ENF_LINKS = 8,
        ENF_ALL = 255
    };    
//	virtual char *encode(const char *text, const char *proto, bool replaceSmiley);
	virtual wchar_t *encode(const wchar_t *text, const char *proto, int flags);
	virtual char *encodeUTF8(const wchar_t *text, const char *proto, int flags);
	virtual char *encodeUTF8(const char *text, const char *proto, int flags);
	virtual char *encodeUTF8(const char *text, int cp, const char *proto, int flags);
	virtual bool encode(const wchar_t *text, const char *proto, wchar_t **output, int *outputSize,  int level, int flags);
	virtual char* getRealProto(HANDLE hContact);
	virtual char* getProto(HANDLE hContact);
	virtual char *getContactName(HANDLE hContact, const char *szProto);
	virtual void getUINs(HANDLE hContact, char *&uinIn, char *&uinOut);
	virtual HANDLE getRealContact(HANDLE hContact);
	virtual DWORD getLastEventTime();
	virtual void setLastEventTime(DWORD);
	virtual int getLastEventType();
	virtual void setLastEventType(int);
	virtual bool isSameDate(DWORD time1, DWORD time2);
	virtual bool isUnicodeMIM();
public:
	virtual void buildHead(IEView *, IEVIEWEVENT *event)=0;
	virtual void appendEvent(IEView *, IEVIEWEVENT *event)=0;
};

#endif
