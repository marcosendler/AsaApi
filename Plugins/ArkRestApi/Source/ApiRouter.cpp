#include "ApiRouter.h"

#include <optional>
#include <regex>

#include "GameThreadDispatcher.h"
#include "RestApiError.h"
#include "Routes.h"

namespace ArkRestApi
{
	namespace
	{
		std::string UrlDecode(const std::string& value)
		{
			std::string result;
			result.reserve(value.size());

			for (size_t i = 0; i < value.size(); ++i)
			{
				if (value[i] == '%' && i + 2 < value.size())
				{
					const std::string hex = value.substr(i + 1, 2);
					result.push_back(static_cast<char>(std::stoi(hex, nullptr, 16)));
					i += 2;
				}
				else if (value[i] == '+')
				{
					result.push_back(' ');
				}
				else
				{
					result.push_back(value[i]);
				}
			}

			return result;
		}

		// Builds the same {steamName|eosId|playerId} selector shape Routes::ResolvePlayer expects
		// for POST bodies, but from a path segment + query params on a GET request. Preferring
		// playerId, then eosId, keeps the selector unambiguous even if the caller also passes a
		// (possibly duplicate) name in the path; `playerKey` is only used as a steamName fallback
		// when neither query param is present. Must be called from inside the game-thread lambda
		// passed to RunOnGameThread, so a malformed playerId throws a RestApiError that's caught
		// there instead of escaping onto the calling Poco thread.
		nlohmann::json BuildPlayerSelector(const std::string& playerKey,
			const std::optional<std::string>& playerIdParam, const std::optional<std::string>& eosIdParam)
		{
			if (playerIdParam.has_value())
			{
				try
				{
					return {{"playerId", std::stoull(*playerIdParam)}};
				}
				catch (const std::exception&)
				{
					throw RestApiError(400, "playerId query parameter must be a number");
				}
			}

			if (eosIdParam.has_value())
			{
				return {{"eosId", *eosIdParam}};
			}

			return {{"steamName", playerKey}};
		}

		ApiResponse RunOnGameThread(std::function<nlohmann::json()> routeFn, int maxQueueWaitMs)
		{
			auto wrapped = [routeFn = std::move(routeFn)]() -> nlohmann::json
			{
				try
				{
					nlohmann::json result = routeFn();
					result["_httpStatus"] = 200;
					return result;
				}
				catch (const RestApiError& error)
				{
					return {{"error", error.what()}, {"_httpStatus", error.httpStatus}};
				}
			};

			std::optional<nlohmann::json> result =
				GameThreadDispatcher::Get().RunOnGameThreadJson(std::move(wrapped), maxQueueWaitMs);

			if (!result.has_value())
			{
				return {504, {{"error", "Timed out waiting for the game thread"}}};
			}

			int status = result->value("_httpStatus", 500);
			result->erase("_httpStatus");
			return {status, *result};
		}
	} // namespace

	ApiResponse ApiRouter::Dispatch(
		const std::string& method,
		const std::string& path,
		const std::unordered_map<std::string, std::string>& query,
		const nlohmann::json& body,
		int maxQueueWaitMs)
	{
		static const std::regex kInventoryCountPattern(R"(^/api/v1/players/([^/]+)/inventory-count$)");
		static const std::regex kTribePattern(R"(^/api/v1/players/([^/]+)/tribe$)");

		std::smatch match;

		if (method == "GET" && path == "/api/v1/status")
		{
			return RunOnGameThread([] { return Routes::GetStatus(); }, maxQueueWaitMs);
		}

		if (method == "GET" && path == "/api/v1/players")
		{
			return RunOnGameThread([] { return Routes::ListPlayers(); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/kick")
		{
			return RunOnGameThread([body] { return Routes::KickPlayer(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/ban")
		{
			return RunOnGameThread([body] { return Routes::BanPlayer(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/broadcast")
		{
			return RunOnGameThread([body] { return Routes::Broadcast(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/message")
		{
			return RunOnGameThread([body] { return Routes::MessagePlayer(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/notify")
		{
			return RunOnGameThread([body] { return Routes::NotifyPlayer(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/teleport")
		{
			return RunOnGameThread([body] { return Routes::TeleportToPosition(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/teleport-to-player")
		{
			return RunOnGameThread([body] { return Routes::TeleportToPlayer(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/spawn/dino")
		{
			return RunOnGameThread([body] { return Routes::SpawnDino(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/spawn/dino-cryopod")
		{
			return RunOnGameThread([body] { return Routes::SpawnDinoAsCryopod(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/spawn/item")
		{
			return RunOnGameThread([body] { return Routes::SpawnItem(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/give-item")
		{
			return RunOnGameThread([body] { return Routes::GiveItem(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/give-items")
		{
			return RunOnGameThread([body] { return Routes::GiveItems(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/give-engrams")
		{
			return RunOnGameThread([body] { return Routes::GiveEngrams(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/give-exp")
		{
			return RunOnGameThread([body] { return Routes::GiveExperience(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/set-level")
		{
			return RunOnGameThread([body] { return Routes::SetPlayerLevel(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/clear-inventory")
		{
			return RunOnGameThread([body] { return Routes::ClearInventory(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/god")
		{
			return RunOnGameThread([body] { return Routes::ToggleGodMode(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/players/set-stats")
		{
			return RunOnGameThread([body] { return Routes::SetPlayerStats(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/world/save")
		{
			return RunOnGameThread([] { return Routes::SaveWorld(); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/world/destroy-all-enemies")
		{
			return RunOnGameThread([] { return Routes::DestroyAllEnemies(); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/world/time")
		{
			return RunOnGameThread([body] { return Routes::SetTimeOfDay(body); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/server/shutdown")
		{
			return RunOnGameThread([] { return Routes::ShutdownServer(); }, maxQueueWaitMs);
		}

		if (method == "POST" && path == "/api/v1/server/restart")
		{
			return RunOnGameThread([] { return Routes::RestartServer(); }, maxQueueWaitMs);
		}

		if (method == "GET" && std::regex_match(path, match, kInventoryCountPattern))
		{
			const std::string playerKey = UrlDecode(match[1].str());
			const auto queryIt = query.find("item");
			const std::string itemName = queryIt != query.end() ? queryIt->second : std::string();

			const auto playerIdIt = query.find("playerId");
			const auto eosIdIt = query.find("eosId");
			const std::optional<std::string> playerIdParam =
				playerIdIt != query.end() ? std::optional<std::string>(playerIdIt->second) : std::nullopt;
			const std::optional<std::string> eosIdParam =
				eosIdIt != query.end() ? std::optional<std::string>(eosIdIt->second) : std::nullopt;

			return RunOnGameThread([playerKey, itemName, playerIdParam, eosIdParam]
			{
				return Routes::GetInventoryCount(BuildPlayerSelector(playerKey, playerIdParam, eosIdParam), itemName);
			}, maxQueueWaitMs);
		}

		if (method == "GET" && std::regex_match(path, match, kTribePattern))
		{
			const std::string playerKey = UrlDecode(match[1].str());

			const auto playerIdIt = query.find("playerId");
			const auto eosIdIt = query.find("eosId");
			const std::optional<std::string> playerIdParam =
				playerIdIt != query.end() ? std::optional<std::string>(playerIdIt->second) : std::nullopt;
			const std::optional<std::string> eosIdParam =
				eosIdIt != query.end() ? std::optional<std::string>(eosIdIt->second) : std::nullopt;

			return RunOnGameThread([playerKey, playerIdParam, eosIdParam]
			{
				return Routes::GetTribeId(BuildPlayerSelector(playerKey, playerIdParam, eosIdParam));
			}, maxQueueWaitMs);
		}

		return {404, {{"error", "Unknown route: " + method + " " + path}}};
	}
} // namespace ArkRestApi
