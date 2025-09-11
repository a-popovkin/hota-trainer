#pragma once
#include <windows.h>
#include <map>
#include <unordered_map>
#include <optional>
#include <atomic>


enum class PlayerColor : int8_t
{
	Red = 0,
	Blue = 1,
	Tan = 2,
	Green = 3,
	Orange = 4,
	Purple = 5,
	Teal = 6,
	Pink = 7,
	Unknown = -1
};

enum class TrainerState : unsigned char
{
	Idle = 0,
	HotaProcessFound,
	GameTablesFound,
	Training
};


class Trainer {
public:
	bool Start();
	bool Stop();

	void UpdateMovementMultiplier(double movement, bool freeze);
	void UpdateGoldMultiplier(double gold, bool freeze);

private:
	std::atomic<double> m_goldMultiplier = 1.0;
	std::atomic<double> m_movementMultiplier = 1.0;

	std::atomic<bool> m_freezeGold = false;
	std::atomic<bool> m_freezeMovement = false;

	bool m_isStarted;

	HANDLE m_stopEvent;
	HANDLE m_workerThread;
	static void WorkerThread(void* pParam);
	static const DWORD s_checkInterval = 100; // every 100ms

	TrainerState m_state = TrainerState::Idle;
	HANDLE m_hotaProcess = NULL;

	std::byte* m_globalClassAddress = nullptr;
	std::byte* m_heroesTableAddress = nullptr;
	std::byte* m_playersTableAddress = nullptr;

	uint16_t m_currentWeekday = 0;
	std::map<PlayerColor, int32_t> m_localHumans;
	std::unordered_map<short, int32_t> m_heroesCurrentMovements; // only owned by local humans

	void LookupHotaProcess();
	void LookupGameTables();
	void Train();

	bool CheckHotaRunning();
	bool CheckLocalHumans();
	void TrainGold();
	void TrainMovement();

	void PatchHeroMovement(std::byte* pHero, short heroIndex, bool isNewDay);
	void PatchHeroMaxMovement(std::byte* pHero);


	inline std::byte* GetPlayerClassByColor(PlayerColor playerColor) const;
	inline std::byte* GetHeroClassByIndex(size_t index) const;

	template<typename T> inline std::optional<T> ReadMemory(const void* address);
	template<typename T> inline bool WriteMemory(std::byte* address, const T& value);
};