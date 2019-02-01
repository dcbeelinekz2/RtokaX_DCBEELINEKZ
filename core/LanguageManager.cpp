/*
 * PtokaX - hub server for Direct Connect peer to peer network.

 * Copyright (C) 2002-2005  Ptaczek, Ptaczek at PtokaX dot org
 * Copyright (C) 2004-2013  Petr Kozelka, PPK at PtokaX dot org

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3
 * as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//---------------------------------------------------------------------------
#include "stdinc.h"
//---------------------------------------------------------------------------
#include "LanguageXml.h"
#include "LanguageStrings.h"
#include "LanguageManager.h"
//---------------------------------------------------------------------------
#include "SettingManager.h"
#include "ServerManager.h"
#include "utility.h"
//---------------------------------------------------------------------------
#ifdef _WIN32
	#pragma hdrstop
#endif
//---------------------------------------------------------------------------
clsLanguageManager * clsLanguageManager::mPtr = NULL;
//---------------------------------------------------------------------------

clsLanguageManager::clsLanguageManager(void) {
    for(size_t szi = 0; szi < LANG_IDS_END; szi++) {
        size_t szTextLen = strlen(LangStr[szi]);
#ifdef _WIN32
        sTexts[szi] = (char *)HeapAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, szTextLen+1);
#else
		sTexts[szi] = (char *)malloc(szTextLen+1);
#endif
        if(sTexts[szi] == NULL) {
            AppendDebugLog("%s - [MEM] Cannot allocate %" PRIu64 " bytes in clsLanguageManager::clsLanguageManager\n", (uint64_t)(szTextLen+1));

            exit(EXIT_FAILURE);
        }
        memcpy(sTexts[szi], LangStr[szi], szTextLen);
        ui16TextsLens[szi] = (uint16_t)szTextLen;
        sTexts[szi][ui16TextsLens[szi]] = '\0';
    }
}
//---------------------------------------------------------------------------

clsLanguageManager::~clsLanguageManager(void) {
    for(size_t szi = 0; szi < LANG_IDS_END; szi++) {
#ifdef _WIN32
        if(sTexts[szi] != NULL && HeapFree(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sTexts[szi]) == 0) {
			AppendDebugLog("%s - [MEM] Cannot deallocate sTexts[szi] in clsLanguageManager::~clsLanguageManager\n", 0);
        }
#else
		free(sTexts[szi]);
#endif
    }
}
//---------------------------------------------------------------------------

void clsLanguageManager::Load() {
    if(clsSettingManager::mPtr->sTexts[SETTXT_LANGUAGE] == NULL) {
        for(size_t szi = 0; szi < LANG_IDS_END; szi++) {
            char * sOldText = sTexts[szi];

            size_t szTextLen = strlen(LangStr[szi]);
#ifdef _WIN32
            sTexts[szi] = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sOldText, szTextLen+1);
#else
			sTexts[szi] = (char *)realloc(sOldText, szTextLen+1);
#endif
            if(sTexts[szi] == NULL) {
                sTexts[szi] = sOldText;

				AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsLanguageManager::Load\n", (uint64_t)(szTextLen+1));

                continue;
            }

            memcpy(sTexts[szi], LangStr[szi], szTextLen);
            ui16TextsLens[szi] = (uint16_t)szTextLen;
            sTexts[szi][ui16TextsLens[szi]] = '\0';
        }
    } else {
#ifdef _WIN32
		string sLanguageFile = clsServerManager::sPath+"\\language\\"+string(clsSettingManager::mPtr->sTexts[SETTXT_LANGUAGE],
#else
		string sLanguageFile = clsServerManager::sPath+"/language/"+string(clsSettingManager::mPtr->sTexts[SETTXT_LANGUAGE],
#endif
            (size_t)clsSettingManager::mPtr->ui16TextsLens[SETTXT_LANGUAGE])+".xml";

        TiXmlDocument doc(sLanguageFile.c_str());
        if(doc.LoadFile()) {
            TiXmlHandle cfg(&doc);
            TiXmlNode *language = cfg.FirstChild("Language").Node();
            if(language != NULL) {
                TiXmlNode *text = NULL;
                while((text = language->IterateChildren(text)) != NULL) {
                    if(text->ToElement() == NULL) {
                        continue;
                    }

                    const char * sName = text->ToElement()->Attribute("Name");
                    const char * sText = text->ToElement()->GetText();
                    size_t szLen = (sText != NULL ? strlen(sText) : 0);
                    if(szLen != 0 && szLen < 129) {
                        for(size_t szi = 0; szi < LANG_IDS_END; szi++) {
                            if(strcmp(LangXmlStr[szi], sName) == 0) {
                                char * sOldText = sTexts[szi];
#ifdef _WIN32
                                sTexts[szi] = (char *)HeapReAlloc(clsServerManager::hPtokaXHeap, HEAP_NO_SERIALIZE, (void *)sOldText, szLen+1);
#else
                                sTexts[szi] = (char *)realloc(sOldText, szLen+1);
#endif
                                if(sTexts[szi] == NULL) {
                                    sTexts[szi] = sOldText;

									AppendDebugLog("%s - [MEM] Cannot reallocate %" PRIu64 " bytes in clsLanguageManager::Load1\n", (uint64_t)(szLen+1));

                                    break;
                                }

                                memcpy(sTexts[szi], sText, szLen);
                                ui16TextsLens[szi] = (uint16_t)szLen;
                                sTexts[szi][ui16TextsLens[szi]] = '\0';
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}
//---------------------------------------------------------------------------

void clsLanguageManager::GenerateXmlExample() {
    TiXmlDocument xmldoc;
    xmldoc.InsertEndChild(TiXmlDeclaration("1.0", "windows-1252", "yes"));
    TiXmlElement xmllanguage("Language");
    xmllanguage.SetAttribute("Name", "Example English Language");
    xmllanguage.SetAttribute("Author", "PtokaX");
    xmllanguage.SetAttribute("Version", PtokaXVersionString " build " BUILD_NUMBER);

    for(int i = 0; i < LANG_IDS_END; i++) {
        TiXmlElement xmlstring("String");
        xmlstring.SetAttribute("Name", LangXmlStr[i]);
        xmlstring.InsertEndChild(TiXmlText(LangStr[i]));
        xmllanguage.InsertEndChild(xmlstring);
    }

    xmldoc.InsertEndChild(xmllanguage);
    xmldoc.SaveFile("English.xml.example");
}
//---------------------------------------------------------------------------
