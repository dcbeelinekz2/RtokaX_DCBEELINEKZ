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
#ifndef hashUsrManagerH
#define hashUsrManagerH
//---------------------------------------------------------------------------
struct User;
//---------------------------------------------------------------------------

class clsHashManager {
private:
    struct IpTableItem {
        IpTableItem *prev, *next;
        User * FirstUser;
        uint16_t ui16Count;
    };

    User *nicktable[65536];
    IpTableItem *iptable[65536];
public:
    static clsHashManager * mPtr;

    clsHashManager();
    ~clsHashManager();

    bool Add(User * u);
    void Remove(User * u);

    User * FindUser(char * sNick, const size_t &szNickLen);
    User * FindUser(User * u);
    User * FindUser(const uint8_t * ui128IpHash);

    uint32_t GetUserIpCount(User * u) const;
};
//---------------------------------------------------------------------------

#endif
