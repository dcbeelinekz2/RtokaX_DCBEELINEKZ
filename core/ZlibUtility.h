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
#ifndef zlibutilityH
#define zlibutilityH
//---------------------------------------------------------------------------

class clsZlibUtility {
private:
    size_t szZbufferSize;
    char *sZbuffer;
public:
    static clsZlibUtility * mPtr;

	clsZlibUtility();
	~clsZlibUtility();
	
    char * CreateZPipe(const char *sInData, const size_t &sInDataSize, uint32_t &iOutDataLen);
    char * CreateZPipe(char *sInData, const size_t &sInDataSize, char *sOutData, size_t &szOutDataLen, size_t &szOutDataSize);
    char * CreateZPipe(char *sInData, const unsigned int &sInDataSize, char *sOutData, unsigned int &iOutDataLen, unsigned int &iOutDataSize, size_t (* pAllignFunc)(size_t n));
};
//---------------------------------------------------------------------------

#endif
