#pragma once

#include <string>

#include <json.hpp>

namespace ArkRestApi::Routes
{
	// Every function here must only ever be called from the game thread
	// (i.e. from inside GameThreadDispatcher's queued handlers). They throw
	// ArkRestApi::RestApiError for expected failures (bad input, player not found, ...).

	nlohmann::json GetStatus();
	nlohmann::json ListPlayers();

	nlohmann::json KickPlayer(const nlohmann::json& body);
	nlohmann::json BanPlayer(const nlohmann::json& body);

	nlohmann::json Broadcast(const nlohmann::json& body);
	nlohmann::json MessagePlayer(const nlohmann::json& body);

	nlohmann::json TeleportToPosition(const nlohmann::json& body);
	nlohmann::json TeleportToPlayer(const nlohmann::json& body);

	nlohmann::json SpawnDino(const nlohmann::json& body);
	nlohmann::json SpawnItem(const nlohmann::json& body);
	nlohmann::json GiveItem(const nlohmann::json& body);

	nlohmann::json GiveEngrams(const nlohmann::json& body);
	nlohmann::json GiveExperience(const nlohmann::json& body);
	nlohmann::json SetPlayerLevel(const nlohmann::json& body);
	nlohmann::json ClearInventory(const nlohmann::json& body);
	nlohmann::json ToggleGodMode(const nlohmann::json& body);

	nlohmann::json GetInventoryCount(const std::string& playerKey, const std::string& itemName);
	nlohmann::json GetTribeId(const std::string& playerKey);

	nlohmann::json SaveWorld();
	nlohmann::json DestroyAllEnemies();
	nlohmann::json SetTimeOfDay(const nlohmann::json& body);
} // namespace ArkRestApi::Routes
