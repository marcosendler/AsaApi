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
	nlohmann::json NotifyPlayer(const nlohmann::json& body);

	nlohmann::json TeleportToPosition(const nlohmann::json& body);
	nlohmann::json TeleportToPlayer(const nlohmann::json& body);

	nlohmann::json SpawnDino(const nlohmann::json& body);
	nlohmann::json SpawnDinoAsCryopod(const nlohmann::json& body);
	nlohmann::json SpawnItem(const nlohmann::json& body);
	nlohmann::json GiveItem(const nlohmann::json& body);
	nlohmann::json GiveItems(const nlohmann::json& body);

	nlohmann::json GiveEngrams(const nlohmann::json& body);
	nlohmann::json GiveExperience(const nlohmann::json& body);
	nlohmann::json SetPlayerLevel(const nlohmann::json& body);
	nlohmann::json ClearInventory(const nlohmann::json& body);
	nlohmann::json ToggleGodMode(const nlohmann::json& body);
	nlohmann::json SetPlayerStats(const nlohmann::json& body);

	// playerSelector is a json object containing one of: steamName, eosId, playerId (same shape
	// accepted in POST bodies elsewhere in this file).
	nlohmann::json GetInventoryCount(const nlohmann::json& playerSelector, const std::string& itemName);
	nlohmann::json GetTribeId(const nlohmann::json& playerSelector);

	nlohmann::json SaveWorld();
	nlohmann::json DestroyAllEnemies();
	nlohmann::json SetTimeOfDay(const nlohmann::json& body);

	// Both call UShooterCheatManager::DoExit() - there is no engine-level distinction between
	// "shutdown" and "restart" from inside the game process. DoExit() just terminates the
	// process; whatever launches the server (systemd, a script, Pterodactyl/AMP, ...) is what
	// decides whether it comes back up afterwards.
	nlohmann::json ShutdownServer();
	nlohmann::json RestartServer();
} // namespace ArkRestApi::Routes
