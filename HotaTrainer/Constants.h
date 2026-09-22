#pragma once

namespace Constants {
	const wchar_t* s_HotaHDProcessName = L"h3hota HD.exe";
	const wchar_t* s_HotaProcessName = L"h3hota.exe";

	constexpr size_t s_heroesTableSize = 0x3E000;
	constexpr size_t s_maxHeroesCount = 0xD8;

	constexpr size_t s_heroClassSize = 0x492;
	constexpr size_t s_heroColorOffset = 0x22; // offset from HeroClass entry
	constexpr size_t s_maxMovementOffset = 0x49; // offset from HeroClass entry
	constexpr size_t s_currentMovementOffset = 0x4D; // offset from HeroClass entry

	constexpr size_t s_gameClassSize = 0x7B000;
	constexpr size_t s_weekdayOffset = 0x1F63E; // offset from GameClass entry
	constexpr size_t s_playersTableOffset = 0x20AD0; // offset from GameClass entry
	constexpr char s_playersCount = 8;

	constexpr size_t s_playerClassSize = 0x168;
	constexpr size_t s_playerGoldOffset = 0xB4; // offset from PlayerClass entry
	constexpr size_t s_playerIsLocalOffset = 0xE1; // offset from PlayerClass entry
	constexpr size_t s_playerIsHumanOffset = 0xE2; // offset from PlayerClass entry

}