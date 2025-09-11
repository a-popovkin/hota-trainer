#include <windows.h>
#include <process.h>
#include <tlhelp32.h>

#include "Logging.h"
#include "Constants.h"
#include "Trainer.h"

using namespace std;


bool Trainer::Start()
{
    if (m_isStarted)
        return true;

    m_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!m_stopEvent)
    {
        LOG(L"CreateEvent ERROR %d\n", GetLastError());
        return false;
    }

    m_workerThread = reinterpret_cast<HANDLE>(_beginthread(WorkerThread, 0, this));
    if (!m_workerThread)
    {
        LOG(L"_beginthread ERROR %d\n", GetLastError());
        CloseHandle(m_stopEvent);
        return false;
    }

    m_isStarted = true;

    return m_isStarted;
}

bool Trainer::Stop()
{
    if (!m_isStarted)
        return true;

    if (!SetEvent(m_stopEvent))
        LOG(L"SetEvent ERROR %d\n", GetLastError());

    LOG(L"Stopping worker thread");

    WaitForSingleObject(m_workerThread, INFINITE);

    m_isStarted = false;
    return !m_isStarted;
}

void Trainer::UpdateMovementMultiplier(double movement, bool freeze)
{
    m_freezeMovement.store(freeze);
    m_movementMultiplier.store(movement);
}

void Trainer::UpdateGoldMultiplier(double gold, bool freeze)
{
    m_freezeGold.store(freeze);
    m_goldMultiplier.store(gold);
}

void Trainer::WorkerThread(void* pParam)
{
    auto pThis = reinterpret_cast<Trainer*>(pParam);

    while (true)
    {
        DWORD waitResult = WaitForSingleObject(pThis->m_stopEvent, s_checkInterval);
        if (waitResult == WAIT_OBJECT_0)
            break;

        switch (pThis->m_state)
        {
        case TrainerState::Idle:
            pThis->LookupHotaProcess();
            break;
        case TrainerState::HotaProcessFound:
            pThis->LookupGameTables();
            break;
        case TrainerState::GameTablesFound:
            pThis->Train();
            break;
        default:
            break;
        }
    }
}

void Trainer::LookupHotaProcess()
{
    DWORD processID = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        LOG(L"CreateToolhelp32Snapshot ERROR %d\n", GetLastError());
        return;
    }

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, Constants::s_HotaHDProcessName) == 0)
            {
                processID = pe.th32ProcessID;
                break;
            }

            if (_wcsicmp(pe.szExeFile, Constants::s_HotaProcessName) == 0)
            {
                processID = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);

    m_hotaProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processID);
    if (!m_hotaProcess) {
        LOG(L"OpenProcess(%d) ERROR %d\n", processID, GetLastError());
        return;
    }

    LOG(L"HotA process found\n");
    m_state = TrainerState::HotaProcessFound;
}

void Trainer::LookupGameTables()
{
    if (not CheckHotaRunning())
        return;

    std::byte* pHeroesTable = nullptr;
    std::byte* pGameClass = nullptr;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    LPBYTE addr = static_cast<LPBYTE>(sysInfo.lpMinimumApplicationAddress);

    while (addr < sysInfo.lpMaximumApplicationAddress)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQueryEx(m_hotaProcess, addr, &mbi, sizeof(mbi)) == 0)
            break;

        
        if (mbi.State == MEM_COMMIT and mbi.AllocationProtect == PAGE_READWRITE)
        {
            auto pBaseAddress = static_cast<std::byte*>(mbi.BaseAddress);
            switch (mbi.RegionSize) {
            case Constants::s_heroesTableSize:
                if (pHeroesTable) 
                    LOG(L"Found another Heroes table candidate (%p): ignoring. Actual: %p\n", pBaseAddress, pHeroesTable);
                else 
                    pHeroesTable = pBaseAddress;
                
                break;
            case Constants::s_gameClassSize:
                if (pGameClass) 
                    LOG(L"Found another Game class candidate (%p): ignoring. Actual: %p\n", pBaseAddress, pGameClass);
                else
                    pGameClass = pBaseAddress;

                break;
            default:
                break;
            }
        }

        addr += mbi.RegionSize;
    }

    if (pHeroesTable and pGameClass) {
        m_heroesTableAddress = pHeroesTable;
        m_globalClassAddress = pGameClass;
        m_playersTableAddress = pGameClass + Constants::s_playersTableOffset;
        
        LOG(L"Game tables found\n");
        m_state = TrainerState::GameTablesFound;
    }
}

bool Trainer::CheckHotaRunning()
{
    DWORD exitCode = 0;
    if (GetExitCodeProcess(m_hotaProcess, &exitCode)) {
        if (exitCode == STILL_ACTIVE) {
            return true;
        }
        else {
            m_hotaProcess = NULL;
            m_state = TrainerState::Idle;
            LOG(L"HotA process exited\n");
        }
    }

    return false;
}

bool Trainer::CheckLocalHumans()
{
    bool changed = false;
    std::map<PlayerColor, int32_t> localHumans = {};

    for (char index = 0; index < Constants::s_playersCount; ++index) {
        const std::byte* pPlayer = GetPlayerClassByColor(static_cast<PlayerColor>(index));

        auto isLocal = ReadMemory<bool>(pPlayer + Constants::s_playerIsLocalOffset);
        auto isHuman = ReadMemory<bool>(pPlayer + Constants::s_playerIsHumanOffset);

        if (isLocal.has_value() && isHuman.has_value() && *isLocal && *isHuman) {
            localHumans.insert(make_pair(static_cast<PlayerColor>(index), -1));

            if (not m_localHumans.contains(static_cast<PlayerColor>(index)))
                changed = true;
        }
    }

    changed |= localHumans.size() != m_localHumans.size();

    if (not localHumans.empty() and changed)
        m_localHumans = localHumans;


    return not m_localHumans.empty();
}

void Trainer::Train()
{
    if (not CheckHotaRunning())
        return;

    if (not CheckLocalHumans())
        return;

    if (m_goldMultiplier.load() > 1.0 or m_freezeGold)
        TrainGold();

    if (m_movementMultiplier.load() > 1.0 or m_freezeMovement)
        TrainMovement();
}

void Trainer::TrainGold()
{
    for (auto& [playerColor, lastKnownGold] : m_localHumans)
    {
        auto pGoldAddress = GetPlayerClassByColor(playerColor) + Constants::s_playerGoldOffset;
        auto playerGold = ReadMemory<int32_t>(pGoldAddress);

        if (not playerGold.has_value())
            continue;

        if (lastKnownGold < 0)
        {
            // set initial value
            lastKnownGold = *playerGold;
            continue;
        }

        const auto diff = *playerGold - lastKnownGold;
        if (m_freezeGold)
        {
            // freeze and continue
            WriteMemory<int32_t>(pGoldAddress, lastKnownGold);
            continue;
        }

        if (diff > 0) // if newer is greater
        {
            const auto newGold = static_cast<int32_t>(lastKnownGold + diff * m_goldMultiplier.load());

            if (WriteMemory<int32_t>(pGoldAddress, newGold))
                lastKnownGold = newGold;
        }
        else
        {
            lastKnownGold = *playerGold;
        }
    }
}

void Trainer::TrainMovement()
{
    const auto currentWeekday = ReadMemory<uint16_t>(m_globalClassAddress + Constants::s_weekdayOffset);

    if (not currentWeekday.has_value())
        return;

    bool isNewDay = m_currentWeekday != *currentWeekday;
    m_currentWeekday = *currentWeekday;

    for (short index = 0; index < Constants::s_maxHeroesCount; ++index)
    {
        std::byte* pHero = GetHeroClassByIndex(index);
        
        const auto heroColor = ReadMemory<int8_t>(pHero + Constants::s_heroColorOffset);
        if (not heroColor.has_value())
            continue;

        if (not m_localHumans.contains(static_cast<PlayerColor>(*heroColor)))
        {
            // remove if present in tracking heroes
            m_heroesMovements.erase(index);
            continue;
        }

        // this hero is owned by local human
        PatchHeroMovement(pHero, index, isNewDay);
    }
}

void Trainer::PatchHeroMovement(std::byte* pHero, short heroIndex, bool isNewDay)
{
    auto pMovementAddress = pHero + Constants::s_currentMovementOffset;
    auto pMaxMovementAddress = pHero + Constants::s_maxMovementOffset;

    const auto heroMovement = ReadMemory<int32_t>(pMovementAddress);
    if (not heroMovement.has_value())
        return;

    const auto heroMaxMovement = ReadMemory<int32_t>(pMaxMovementAddress);
    if (not heroMaxMovement.has_value())
        return;


    bool isKnownHero = m_heroesMovements.contains(heroIndex);
    if (not isKnownHero) 
        m_heroesMovements[heroIndex] = make_pair(*heroMovement, *heroMaxMovement);
    
    if (m_freezeMovement)
    {
        // freeze and exit
        auto& lastKnownMovement = m_heroesMovements[heroIndex].first;
        WriteMemory<int32_t>(pMovementAddress, lastKnownMovement);
        return;
    }

    if (isNewDay)
    {
        const auto newMovement = static_cast<int32_t>(*heroMovement * m_movementMultiplier.load());
        if (WriteMemory<int32_t>(pMovementAddress, newMovement))
            m_heroesMovements[heroIndex].first = newMovement;
    }
    else
    {
        auto& lastKnownMovement = m_heroesMovements[heroIndex].first;

        const auto diff = *heroMovement - lastKnownMovement;
        if (diff > 0) // if newer is greater
        {
            const auto newMovement = static_cast<int32_t>(lastKnownMovement + diff * m_movementMultiplier.load());

            if (WriteMemory<int32_t>(pMovementAddress, newMovement))
                lastKnownMovement = newMovement;
        }
        else
        {
            lastKnownMovement = *heroMovement;
        }
    }
    
    // max movement patching

    // patch only if value has been in-game updated
    if (m_heroesMovements[heroIndex].second == *heroMaxMovement)
    {
        // firstly remember real new max movement
        m_heroesMovements[heroIndex].second = *heroMaxMovement;
        const auto newMaxMovement = static_cast<int32_t>(m_heroesMovements[heroIndex].second * m_movementMultiplier.load());

        // patch in-game value only, locally store normal max value
        WriteMemory<int32_t>(pMaxMovementAddress, newMaxMovement);
    }
}


inline std::byte* Trainer::GetPlayerClassByColor(PlayerColor playerColor) const
{
    return m_playersTableAddress + static_cast<const char>(playerColor) * Constants::s_playerClassSize;
}

inline std::byte* Trainer::GetHeroClassByIndex(size_t index) const
{
    return m_heroesTableAddress + index * Constants::s_heroClassSize;
}

template<typename T>
inline optional<T> Trainer::ReadMemory(const void* address)
{
    T buffer;
    SIZE_T bytesRead = 0;
    if (ReadProcessMemory(m_hotaProcess, address, &buffer, sizeof(T), &bytesRead) && bytesRead == sizeof(T)) 
        return buffer;
    
    return nullopt;
}

template<typename T>
inline bool Trainer::WriteMemory(std::byte* address, const T& value)
{
    SIZE_T bytesWritten = 0;
    return WriteProcessMemory(m_hotaProcess, address, &value, sizeof(T), &bytesWritten) && bytesWritten == sizeof(T);
}
