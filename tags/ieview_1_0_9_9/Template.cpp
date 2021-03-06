#include "Template.h"
#include "Utils.h"

TokenDef::TokenDef(const char *tokenString) {
	this->tokenString = tokenString;
	this->tokenLen = strlen(tokenString);
	this->token = 0;
	this->escape = 0;
}

TokenDef::TokenDef(const char *tokenString, int token, int escape) {
	this->tokenString = tokenString;
	this->token = token;
	this->tokenLen = strlen(tokenString);
	this->escape = escape;
}

Token::Token(int type, const char *text, int escape) {
	next = NULL;
	this->type = type;
	this->escape = escape;
	if (text!=NULL) {
		this->text = Utils::dupString(text);
	} else {
        this->text = NULL;
	}
}

Token::~Token() {
	if (text!=NULL) {
		delete text;
	}
}

Token * Token::getNext() {
	return next;
}

void Token::setNext(Token *ptr) {
	next = ptr;
}

int Token::getType() {
	return type;
}

int Token::getEscape() {
	return escape;
}

const char *Token::getText() {
	return text;
}

Template::Template(const char *name, const char *text) {
	next = NULL;
	tokens = NULL;
	this->text = Utils::dupString(text);
	this->name = Utils::dupString(name);
	tokenize();
}

Template::~Template() {
	if (text != NULL) delete text;
	if (name != NULL) delete name;
	Token *ptr = tokens, *ptr2;
	tokens = NULL;
	for (;ptr!=NULL;ptr = ptr2) {
		ptr2 = ptr->getNext();
		delete ptr;
	}
}

const char *Template::getText() {
	return text;
}

const char *Template::getName() {
	return name;
}

Template* Template::getNext() {
	return next;
}

bool Template::equals(const char *name) {
	if (!strcmp(name, this->name)) {
		return true;
	}
	return false;
}

static TokenDef tokenNames[] = {
	TokenDef("%name%", Token::NAME, 0),
	TokenDef("%time%", Token::TIME, 0),
	TokenDef("%text%", Token::TEXT, 0),
	TokenDef("%date%", Token::DATE, 0),
	TokenDef("%base%", Token::BASE, 0),
	TokenDef("%avatar%", Token::AVATAR, 0),
	TokenDef("%cid%", Token::CID, 0),
	TokenDef("%proto%", Token::PROTO, 0),
	TokenDef("%avatarIn%", Token::AVATARIN, 0),
	TokenDef("%avatarOut%", Token::AVATAROUT, 0),
	TokenDef("%nameIn%", Token::NAMEIN, 0),
	TokenDef("%nameOut%", Token::NAMEOUT, 0),
	TokenDef("%uin%", Token::UIN, 0),
	TokenDef("%uinIn%", Token::UININ, 0),
	TokenDef("%uinOut%", Token::UINOUT, 0),
	TokenDef("%nickIn%", Token::NICKIN, 0),
	TokenDef("%nickOut%", Token::NICKOUT, 1),
	TokenDef("%statusMsg%", Token::STATUSMSG, 0),
	TokenDef("%fileDesc%", Token::FILEDESC, 0),

	TokenDef("%\\name%", Token::NAME, 1),
	TokenDef("%\\time%", Token::TIME, 1),
	TokenDef("%\\text%", Token::TEXT, 1),
	TokenDef("%\\date%", Token::DATE, 1),
	TokenDef("%\\base%", Token::BASE, 1),
	TokenDef("%\\avatar%", Token::AVATAR, 1),
	TokenDef("%\\cid%", Token::CID, 1),
	TokenDef("%\\proto%", Token::PROTO, 1),
	TokenDef("%\\avatarIn%", Token::AVATARIN, 1),
	TokenDef("%\\avatarOut%", Token::AVATAROUT, 1),
	TokenDef("%\\nameIn%", Token::NAMEIN, 1),
	TokenDef("%\\nameOut%", Token::NAMEOUT, 1),
	TokenDef("%\\uin%", Token::UIN, 1),
	TokenDef("%\\uinIn%", Token::UININ, 1),
	TokenDef("%\\uinOut%", Token::UINOUT, 1),
	TokenDef("%\\nickIn%", Token::NICKIN, 1),
	TokenDef("%\\nickOut%", Token::NICKOUT, 1),
	TokenDef("%\\statusMsg%", Token::STATUSMSG, 1),
	TokenDef("%\\fileDesc%", Token::FILEDESC, 1)
};

void Template::tokenize() {
	if (text!=NULL) {
//		debugView->writef("Tokenizing: %s<br>---<br>", text);
		char *str = Utils::dupString(text);
		Token *lastToken = NULL;
		int lastTokenType = Token::PLAIN;
		int lastTokenEscape = 0;
		int l = strlen(str);
		for (int i=0, lastTokenStart=0; i<=l;) {
			Token *newToken;
			int newTokenType, newTokenSize, newTokenEscape;
			if (str[i]=='\0') {
				newTokenType = Token::END;
				newTokenSize = 1;
				newTokenEscape = 0;
			} else {
				bool found = false;
				for (unsigned int j=0; j<(sizeof(tokenNames)/sizeof(tokenNames[0])); j++) {
					if (!strncmp(str+i, tokenNames[j].tokenString, tokenNames[j].tokenLen)) {
						newTokenType = tokenNames[j].token;
						newTokenSize = tokenNames[j].tokenLen;
						newTokenEscape = tokenNames[j].escape;
						found = true;
						break;
					}
				}
				if (!found) {
					newTokenType = Token::PLAIN;
					newTokenSize = 1;
					newTokenEscape = 0;
				}
			}
			if (newTokenType != Token::PLAIN) {
				if (str[i + newTokenSize] == '%') {
                    //newTokenSize++;
				}
				str[i] = '\0';
			}
			if ((lastTokenType!=newTokenType || lastTokenEscape != newTokenEscape) && i!=lastTokenStart) {
				if (lastTokenType == Token::PLAIN) {
                    newToken = new Token(lastTokenType, str+lastTokenStart, lastTokenEscape);
				} else {
					newToken = new Token(lastTokenType, NULL, lastTokenEscape);
				}
				if (lastToken != NULL) {
					lastToken->setNext(newToken);
				} else {
					tokens = newToken;
				}
				lastToken = newToken;
				lastTokenStart = i;
			}
			lastTokenEscape = newTokenEscape;
			lastTokenType = newTokenType;
			i += newTokenSize;
		}
		delete str;
	}
}

Token * Template::getTokens() {
	return tokens;
}

TemplateMap* TemplateMap::mapList = NULL;

TemplateMap::TemplateMap(const char *name) {
	entries = NULL;
	next = NULL;
	filename = NULL;
	this->name = Utils::dupString(name);
}

TemplateMap* TemplateMap::add(const char *proto, const char *filename) {
	TemplateMap *map;
	for (map=mapList; map!=NULL; map=map->next) {
		if (!strcmp(map->name, proto)) {
			map->clear();
			map->setFilename(filename);
			return map;
		}
	}
	map = new TemplateMap(proto);
	map->setFilename(filename);
	map->next = mapList;
	mapList = map;
	return map;
}

void TemplateMap::addTemplate(const char *name, const char *text) {
	Template *tmplate = new Template(name, text);
	tmplate->next = entries;
	entries = tmplate;
}

void TemplateMap::clear() {
	Template *ptr, *ptr2;
	ptr = entries;
	entries = NULL;
	for (;ptr!=NULL;ptr=ptr2) {
		ptr2 = ptr->getNext();
		delete ptr;
	}
}

static TokenDef templateNames[] = {
	TokenDef("<!--HTMLStart-->"),
	TokenDef("<!--MessageIn-->"),
	TokenDef("<!--MessageOut-->"),
	TokenDef("<!--hMessageIn-->"),
	TokenDef("<!--hMessageOut-->"),
	TokenDef("<!--File-->"),
	TokenDef("<!--hFile-->"),
	TokenDef("<!--URL-->"),
	TokenDef("<!--hURL-->"),
	TokenDef("<!--Status-->"),
	TokenDef("<!--hStatus-->"),
	TokenDef("<!--MessageInGroupStart-->"),
	TokenDef("<!--MessageInGroupInner-->"),
	TokenDef("<!--MessageInGroupEnd-->"),
	TokenDef("<!--hMessageInGroupStart-->"),
	TokenDef("<!--hMessageInGroupInner-->"),
	TokenDef("<!--hMessageInGroupEnd-->"),
	TokenDef("<!--MessageOutGroupStart-->"),
	TokenDef("<!--MessageOutGroupInner-->"),
	TokenDef("<!--MessageOutGroupEnd-->"),
	TokenDef("<!--hMessageOutGroupStart-->"),
	TokenDef("<!--hMessageOutGroupInner-->"),
	TokenDef("<!--hMessageOutGroupEnd-->"),
	TokenDef("<!--FileIn-->"),
	TokenDef("<!--hFileIn-->"),
	TokenDef("<!--FileOut-->"),
	TokenDef("<!--hFileOut-->"),
	TokenDef("<!--URLIn-->"),
	TokenDef("<!--hURLIn-->"),
	TokenDef("<!--URLOut-->"),
	TokenDef("<!--hURLOut-->"),

	TokenDef("<!--MessageInRTL-->"),
	TokenDef("<!--MessageOutRTL-->"),
	TokenDef("<!--hMessageInRTL-->"),
	TokenDef("<!--hMessageOutRTL-->"),
	TokenDef("<!--MessageInGroupStartRTL-->"),
	TokenDef("<!--MessageInGroupInnerRTL-->"),
	TokenDef("<!--MessageInGroupEndRTL-->"),
	TokenDef("<!--hMessageInGroupStartRTL-->"),
	TokenDef("<!--hMessageInGroupInnerRTL-->"),
	TokenDef("<!--hMessageInGroupEndRTL-->"),
	TokenDef("<!--MessageOutGroupStartRTL-->"),
	TokenDef("<!--MessageOutGroupInnerRTL-->"),
	TokenDef("<!--MessageOutGroupEndRTL-->"),
	TokenDef("<!--hMessageOutGroupStartRTL-->"),
	TokenDef("<!--hMessageOutGroupInnerRTL-->"),
	TokenDef("<!--hMessageOutGroupEndRTL-->")
};


TemplateMap* TemplateMap::loadTemplateFile(const char *proto, const char *filename, bool onlyInfo) {
	FILE* fh;
	char lastTemplate[1024], tmp2[1024];
	char pathstring[500];
	TemplateMap *tmap;
	tmap = TemplateMap::getTemplateMap(proto);
	if (tmap!=NULL && !strcmpi(tmap->getFilename(), filename)) {
		return tmap;
	}
	tmap = TemplateMap::add(proto, filename);
	if (filename == NULL || strlen(filename) == 0) {
		return NULL;
	}
	strcpy(pathstring, filename);
	char* pathrun = pathstring + strlen(pathstring);
	while ((*pathrun != '\\' && *pathrun != '/') && (pathrun > pathstring)) pathrun--;
	pathrun++;
	*pathrun = '\0';

	fh = fopen(filename, "rt");
	if (fh == NULL) {
		return NULL;
	}
	char store[4096];
	bool wasTemplate = false;
	char *templateText = NULL;
	int templateTextSize = 0;
	while (fgets(store, sizeof(store), fh) != NULL) {
    	if (sscanf(store, "%s", tmp2) == EOF) continue;
	    //template start
	    if (!onlyInfo) {
	    	bool bFound = false;
            for (unsigned i = 0; i < sizeof(templateNames) / sizeof (templateNames[0]); i++) {
	    		if (!strncmp(store, templateNames[i].tokenString, templateNames[i].tokenLen)) {
	    			bFound = true;
	    			break;
	    		}
	    	}
            if (bFound) {
				if (wasTemplate) {
					tmap->addTemplate(lastTemplate, templateText);
	                //debugView->writef("1. %s<br>%s", lastTemplate, templateText);
				}
				if (templateText!=NULL) {
					free (templateText);
				}
				templateText = NULL;
				templateTextSize = 0;
				wasTemplate = true;
                sscanf(store, "<!--%[^-]", lastTemplate);
			} else if (wasTemplate) {
				Utils::appendText(&templateText, &templateTextSize, "%s", store);
			}
		}
  	}
  	if (wasTemplate) {
		tmap->addTemplate(lastTemplate, templateText);
        //debugView->writef("2. %s<br>%s", lastTemplate, templateText);
	}
  	fclose(fh);
	static const char *groupTemplates[] = {"MessageInGroupStart", "MessageInGroupInner",
	                                       "hMessageInGroupStart", "hMessageInGroupInner",
										   "MessageOutGroupStart", "MessageOutGroupInner",
	                                       "hMessageOutGroupStart", "hMessageOutGroupInner"};
	tmap->grouping = true;
	for (int i=0; i<8; i++) {
		if (tmap->getTemplate(groupTemplates[i])== NULL) {
			tmap->grouping = false;
			break;
		}
	}
	return tmap;
}

bool TemplateMap::isGrouping() {
	return grouping;
}

Template* TemplateMap::getTemplate(const char *text) {
	Template *ptr;
	for (ptr=entries; ptr!=NULL; ptr=ptr->getNext()) {
		if (ptr->equals(text)) {
			break;
		}
	}
	return ptr;
}

Template* TemplateMap::getTemplate(const char *proto, const char *text) {
	TemplateMap *ptr;
	for (ptr=mapList; ptr!=NULL; ptr=ptr->next) {
		if (!strcmp(ptr->name, proto)) {
			return ptr->getTemplate(text);
		}
	}
	return NULL;
}

TemplateMap* TemplateMap::getTemplateMap(const char *proto) {
	TemplateMap *ptr;
	for (ptr=mapList; ptr!=NULL; ptr=ptr->next) {
		if (!strcmp(ptr->name, proto)) {
			return ptr;
		}
	}
	return NULL;
}

const char *TemplateMap::getFilename() {
    return filename;
}

void TemplateMap::setFilename(const char *filename) {
	if (this->filename != NULL) {
	    delete this->filename;
	}
	this->filename = Utils::dupString(filename);
	Utils::convertPath(this->filename);
}

TemplateMap* TemplateMap::loadTemplates(const char *proto, const char *filename) {
	return loadTemplateFile(proto, filename, false);
}


