#include "stdafx.h"
#include "types.h"

#define _PRINT_DEBUG

BOOL g_running = TRUE;

std::once_flag g_flag;

using DWGetLogonStatus_t = int (*)(int);

using MoveResponseToInventory_t = bool(__fastcall*)(LPVOID, int);

extern void Log_(const char* fmt, ...);
#define LOG(fmt, ...) Log_(xorstr_(fmt), ##__VA_ARGS__)

#define LOG_ADDR(var_name)                                        \
        LOG(#var_name ": 0x%llX (0x%llX)", var_name, var_name > base ? var_name - base : 0);    

#define INRANGE(x,a,b)    (x >= a && x <= b) 
#define getBits( x )    (INRANGE((x&(~0x20)),'A','F') ? ((x&(~0x20)) - 'A' + 0xa) : (INRANGE(x,'0','9') ? x - '0' : 0))
#define getByte( x )    (getBits(x[0]) << 4 | getBits(x[1]))

void Log_(const char* fmt, ...) {
    char        text[4096];
    va_list        ap;
    va_start(ap, fmt);
    vsprintf_s(text, fmt, ap);
    va_end(ap);

    std::ofstream logfile(xorstr_("log.txt"), std::ios::app);
    if (logfile.is_open() && text)    logfile << text << std::endl;
    logfile.close();
}

__int64 find_pattern(__int64 range_start, __int64 range_end, const char* pattern) {
    const char* pat = pattern;
    __int64 firstMatch = NULL;
    __int64 pCur = range_start;
    __int64 region_end;
    MEMORY_BASIC_INFORMATION mbi{};
    while (sizeof(mbi) == VirtualQuery((LPCVOID)pCur, &mbi, sizeof(mbi))) {
        if (pCur >= range_end - strlen(pattern))
            break;
        if (!(mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_READWRITE))) {
            pCur += mbi.RegionSize;
            continue;
        }
        region_end = pCur + mbi.RegionSize;
        while (pCur < region_end)
        {
            if (!*pat)
                return firstMatch;
            if (*(PBYTE)pat == '\?' || *(BYTE*)pCur == getByte(pat)) {
                if (!firstMatch)
                    firstMatch = pCur;
                if (!pat[1] || !pat[2])
                    return firstMatch;

                if (*(PWORD)pat == '\?\?' || *(PBYTE)pat != '\?')
                    pat += 3;
                else
                    pat += 2;
            }
            else {
                if (firstMatch)
                    pCur = firstMatch;
                pat = pattern;
                firstMatch = 0;
            }
            pCur++;
        }
    }
    return NULL;
}

namespace game {

    __int64 base;
    __int64 lootBase;
    __int64 fpGetLogonStatus;
    __int64 fpMoveResponseToInventory;
    __int64 fpFindStringtable;
    __int64 fpStringtableGetColumnValueForRow;
    __int64 fpGetLootBase;

    bool init() {

        base = (__int64)GetModuleHandle(NULL);
        return true;
    }

    bool find_sigs() {

        MODULEINFO moduleInfo;
        if (!GetModuleInformation((HANDLE)-1, GetModuleHandle(NULL), &moduleInfo, sizeof(MODULEINFO)) || !moduleInfo.lpBaseOfDll) {
            LOG("Couldnt GetModuleInformation");
            return NULL;
        }
        LOG("Base: 0x%llx", moduleInfo.lpBaseOfDll);
        LOG("Size: 0x%llx", moduleInfo.SizeOfImage);

        __int64 searchStart = (__int64)moduleInfo.lpBaseOfDll;
        __int64 searchEnd = (__int64)moduleInfo.lpBaseOfDll + moduleInfo.SizeOfImage;

        bool result = true;

        auto resolve_jmp = [](__int64 addr) -> __int64 {
            return *(int*)(addr + 1) + addr + 5;
        };

        auto resolve_lea = [](__int64 addr) -> __int64 {
            return *(int*)(addr + 3) + addr + 7;
        };

        LOG_ADDR(fpGetLogonStatus = resolve_jmp(
            find_pattern(searchStart, searchEnd, xorstr_("E8 ? ? ? ? 83 F8 02 0F 84 ? ? ? ? 48 89"))));

        LOG_ADDR(fpFindStringtable = resolve_jmp(
            find_pattern(searchStart, searchEnd, xorstr_("E8 ? ? ? ? 48 8B 8C 24 ? ? ? ? E8 ? ? ? ? 44"))));

        LOG_ADDR(fpStringtableGetColumnValueForRow = resolve_jmp(
            find_pattern(searchStart, searchEnd, xorstr_("E8 ? ? ? ? 48 8D 4B 02 FF"))));

        //LOG_ADDR(fpMoveResponseToInventory =
        //    (find_pattern(searchStart, searchEnd, xorstr_("83 7C 24 ? ? 74 1B 83 7C 24 ? ? 0F 84")) - 0x2B));

        //LOG_ADDR(fpGetLootBase = resolve_jmp(
        //    (find_pattern(searchStart, searchEnd, xorstr_("E8 ? ? ? ? 48 89 44 24 ? 41 B9 ? ? ? ? 44 ")))));

        fpMoveResponseToInventory = (__int64)0x63458A0 + base;
        fpGetLootBase = (__int64)0x6342D40 + base;

        return result;
    }

    static void* GetLootBase() {

        return reinterpret_cast<void* (__cdecl*)()>(fpGetLootBase)();
    }

    static void FindStringTable(const char* name, StringTable** table) {

        reinterpret_cast<void(__cdecl*)(const char*, StringTable**)>(fpFindStringtable)(name, table);
    }

    static char* StringTable_GetColumnValueForRow(void* stringTable, int row, int column) {

        return reinterpret_cast<char* (__cdecl*)(void*, int, int)>(fpStringtableGetColumnValueForRow)(stringTable, row, column);
    }
}

MoveResponseToInventory_t fpMoveResponseOrig = NULL;

LPCSTR mCAMOS[]{
    //EXTRAS
    "weapon",
    "feature",
    "attachment",
    "reticle",
    "perk",
    "equipment",
    "operator",
    "operator_skin",
    "operator_quip",
    "executions",
    "playercards",
    "emblems",
    "accessory",
    "gestures",
    "camo",
    "weapon_charm",
    "sticker",
    "vehicle_camo",
    "vehicle_horn",
    "vehicle_track",
    "gs_save_slot",
    //CW Camos
    //AR
   "iw8_ar_t9accurate_attachment",
   "iw8_ar_t9accurate_camos",
   "iw8_ar_t9british_attachment",
   "iw8_ar_t9british_camos",
   "iw8_ar_t9damage_attachment",
   "iw8_ar_t9damage_camos",
   "iw8_ar_t9fastburst_attachment",
   "iw8_ar_t9fastburst_camos",
   "iw8_ar_t9fastfire_attachment",
   "iw8_ar_t9fastfire_camos",
   "iw8_ar_t9fasthandling_attachment",
   "iw8_ar_t9fasthandling_camos",
   "iw8_ar_t9longburst_attachment",
   "iw8_ar_t9longburst_camos",
   "iw8_ar_t9mobility_attachment",
   "iw8_ar_t9mobility_camos",
   "iw8_ar_t9slowfire_attachment",
   "iw8_ar_t9slowfire_camos",
   "iw8_ar_t9slowhandling_attachment",
   "iw8_ar_t9slowhandling_camos",
   "iw8_ar_t9standard_attachment",
   "iw8_ar_t9standard_camos",
   "iw8_ar_t9season6_attachment",
   "iw8_ar_t9season6_camos",
   "s4_ar_stango44temp_attachment",
   "s4_ar_stango44temp_camos",
   //LMGS
   "iw8_lm_t9accurate_attachment",
   "iw8_lm_t9accurate_camos",
   "iw8_lm_t9fastfire_attachment",
   "iw8_lm_t9fastfire_camos",
   "iw8_lm_t9light_attachment",
   "iw8_lm_t9light_camos",
   "iw8_lm_t9slowfire_attachment",
   "iw8_lm_t9slowfire_camos",
   //PISTOLS
   "iw8_pi_t9burst_attachment",
   "iw8_pi_t9burst_camos",
   "iw8_pi_t9fullauto_attachment",
   "iw8_pi_t9fullauto_camos",
   "iw8_pi_t9pistolshot_attachment",
   "iw8_pi_t9pistolshot_camos",
   "iw8_pi_t9revolver_attachment",
   "iw8_pi_t9revolver_camos",
   "iw8_pi_t9semiauto_attachment",
   "iw8_pi_t9semiauto_camos",
   //SHOTGUNS
   "iw8_sh_t9fullauto_attachment",
   "iw8_sh_t9fullauto_camos",
   "iw8_sh_t9pump_attachment",
   "iw8_sh_t9pump_camos",
   "iw8_sh_t9semiauto_attachment",
   "iw8_sh_t9semiauto_camos",
   "iw8_sh_t9leveraction_attachment",
   "iw8_sh_t9leveraction_camos",
   //SMGS
   "iw8_sm_t9accurate_attachment",
   "iw8_sm_t9accurate_camos",
   "iw8_sm_t9burst_attachment",
   "iw8_sm_t9burst_camos",
   "iw8_sm_t9capacity_attachment",
   "iw8_sm_t9capacity_camos",
   "iw8_sm_t9cqb_attachment",
   "iw8_sm_t9cqb_camos",
   "iw8_sm_t9fastfire_attachment",
   "iw8_sm_t9fastfire_camos",
   "iw8_sm_t9handling_attachment",
   "iw8_sm_t9handling_camos",
   "iw8_sm_t9heavy_attachment",
   "iw8_sm_t9heavy_camos",
   "iw8_sm_t9nailgun_attachment",
   "iw8_sm_t9nailgun_camos",
   "iw8_sm_t9powerburst_attachment",
   "iw8_sm_t9powerburst_camos",
   "iw8_sm_t9semiauto_attachment",
   "iw8_sm_t9semiauto_camos",
   "iw8_sm_t9spray_attachment",
   "iw8_sm_t9spray_camos",
   "iw8_sm_t9standard_attachment",
   "iw8_sm_t9standard_camos",
   "iw8_sm_t9season6_attachment",
   "iw8_sm_t9season6_camos",
   "iw8_sm_t9season6_attachment",
   "iw8_sm_t9season6_camos",
   //SNIPERS
   "iw8_sn_t9accurate_attachment",
   "iw8_sn_t9accurate_camos",
   "iw8_sn_t9cannon_attachment",
   "iw8_sn_t9cannon_camos",
   "iw8_sn_t9crossbow_attachment",
   "iw8_sn_t9crossbow_camos",
   "iw8_sn_t9damagesemi_attachment",
   "iw8_sn_t9damagesemi_camos",
   "iw8_sn_t9powersemi_attachment",
   "iw8_sn_t9powersemi_camos",
   "iw8_sn_t9precisionsemi_attachment",
   "iw8_sn_t9precisionsemi_camos",
   "iw8_sn_t9quickscope_attachment",
   "iw8_sn_t9quickscope_camos",
   "iw8_sn_t9standard_attachment",
   "iw8_sn_t9standard_camos",
   "s4_sn_m1golftemp_attachment",
   "s4_sn_m1golftemp_camos",
   // melee
   "iw8_me_t9ballisticknife_attachment",
   "iw8_me_t9ballisticknife_camos",
   "iw8_me_t9bat_attachment",
   "iw8_me_t9bat_camos",
   "iw8_me_t9cane_attachment",
   "iw8_me_t9cane_camos",
   "iw8_me_t9etool_attachment",
   "iw8_me_t9etool_camos",
   "iw8_me_t9loadout_attachment",
   "iw8_me_t9loadout_camos",
   "iw8_me_t9mace_attachment",
   "iw8_me_t9mace_camos",
   "iw8_me_t9machete_attachment",
   "iw8_me_t9machete_camos",
   "iw8_me_t9sledgehammer_attachment",
   "iw8_me_t9sledgehammer_camos",
   "iw8_me_t9wakizashi_attachment",
   "iw8_me_t9wakizashi_camos",
   "iw8_me_t9coldwar_attachment",
   "iw8_me_t9coldwar_camos",
   "iw8_me_t9battleaxe_attachment",
   "iw8_me_t9battleaxe_camos",


};
bool __fastcall MoveResponseToInventory_Hooked(LPVOID a1, int a2) {

    fpMoveResponseOrig(a1, a2);

    auto pLootBase = game::GetLootBase(); // signature 48 8D 0D ? ? ? ? 48 8D 44 24 ? C7 44 (LEA rcx, pLootBase)

    auto pInventory = (LootItem*)((uintptr_t)pLootBase + 64);

    auto pNumItems = (uint32_t*)((uintptr_t)pLootBase + 240064);

    int curCount = *pNumItems;

    auto updateOrAddItem = [&](int itemId, int quantity) {

        bool bFound = false;

        for (int i = 0; i < 30000; i++) {
            if (pInventory[i].m_itemId == itemId && pInventory[i].m_itemQuantity < 1) {
                pInventory[i].m_itemQuantity++;
                bFound = true;
                break;
            }
        }

        if (!bFound) {
            pInventory[curCount].m_itemId = itemId;
            pInventory[curCount].m_itemQuantity = 1;

            curCount++;
            (*pNumItems)++;

            *(BYTE*)((uintptr_t)pLootBase + 240072) = 0;
        }
    };

    char buf[1024];

    for (int i = 0; i < ARRAYSIZE(mCAMOS); i++)
    {
        sprintf_s(buf, "loot/%s_ids.csv", mCAMOS[i]);

        StringTable* string_table = nullptr;

        game::FindStringTable(buf, &string_table);

        for (int s = 0; s < string_table->rowCount; s++) {

            updateOrAddItem(atoi(game::StringTable_GetColumnValueForRow(string_table, s, 0)), 1);
        }

    }
    MH_RemoveHook((LPVOID)game::fpMoveResponseToInventory);

    return false;

    StringTable* loot_master = nullptr;

    game::FindStringTable(xorstr_("loot/loot_master.csv"), &loot_master);

    for (int i = 1; i < loot_master->rowCount; i++) {

        char* loot_type = game::StringTable_GetColumnValueForRow(loot_master, i, 2);

        if (strstr(loot_type, "iw8_") || loot_type[0] == '#')
            continue;

        char buf[1024];

        sprintf_s(buf, "loot/%s_ids.csv", loot_type);

        StringTable* string_table = nullptr;

        game::FindStringTable(buf, &string_table);

        if (!string_table)
            continue;

        for (int s = 0; s < string_table->rowCount; s++) {

            updateOrAddItem(atoi(game::StringTable_GetColumnValueForRow(string_table, s, 0)), 1);
        }
    }

    MH_RemoveHook((LPVOID)game::fpMoveResponseToInventory);

    return false;
}


void on_attach() {

    game::init();

    if (!game::find_sigs())
        return;
}

void on_detach() {

    g_running = FALSE;
}

DWORD WINAPI thread_proc(LPVOID) {

    std::call_once(g_flag, on_attach);

    while (((DWGetLogonStatus_t)game::fpGetLogonStatus)(0) != 2)
    {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(0));
    }

    if (MH_Initialize() != MH_OK)
        return ERROR_API_UNAVAILABLE;

    if (MH_CreateHook((LPVOID)game::fpMoveResponseToInventory, MoveResponseToInventory_Hooked,
        reinterpret_cast<LPVOID*>(&fpMoveResponseOrig)) == MH_OK) {

        MH_EnableHook((LPVOID)game::fpMoveResponseToInventory);
    }

    return ERROR_SUCCESS;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved) {

    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {

        I_beginthreadex(0, 0, (_beginthreadex_proc_type)thread_proc, 0, 0, 0);
    }
                           break;
    case DLL_PROCESS_DETACH:
        on_detach();
        break;
    }
    return TRUE;
}