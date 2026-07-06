#include "Routes.h"

#include <optional>

#include <API/ARK/Ark.h>
#include <IApiUtils.h>
#include <Tools.h>

#include "RestApiError.h"

namespace ArkRestApi::Routes
{
	namespace
	{
		std::string EscapeFmtBraces(const std::string& text)
		{
			std::string escaped;
			escaped.reserve(text.size());
			for (char c : text)
			{
				escaped.push_back(c);
				if (c == '{' || c == '}')
				{
					escaped.push_back(c);
				}
			}
			return escaped;
		}

		AShooterPlayerController* FindPlayerById(uint64 playerId)
		{
			auto& controllers = AsaApi::GetApiUtils().GetWorld()->PlayerControllerListField();
			for (TWeakObjectPtr<APlayerController> weakPc : controllers)
			{
				auto* pc = static_cast<AShooterPlayerController*>(weakPc.Get());
				if (pc != nullptr && AsaApi::IApiUtils::GetPlayerID(static_cast<AController*>(pc)) == playerId)
				{
					return pc;
				}
			}
			return nullptr;
		}

		// Resolves a player from a json object containing one of: steamName, eosId, playerId.
		AShooterPlayerController* ResolvePlayer(const nlohmann::json& source)
		{
			if (source.contains("steamName") && source["steamName"].is_string())
			{
				const std::string name = source["steamName"].get<std::string>();
				AShooterPlayerController* pc =
					AsaApi::GetApiUtils().FindPlayerFromPlatformName(FString::FromStringUTF8(name));
				if (pc == nullptr)
				{
					throw RestApiError(404, "Player not found by steamName: " + name);
				}
				return pc;
			}

			if (source.contains("eosId") && source["eosId"].is_string())
			{
				const std::string eosId = source["eosId"].get<std::string>();
				AShooterPlayerController* pc =
					AsaApi::GetApiUtils().FindPlayerFromEOSID(FString::FromStringUTF8(eosId));
				if (pc == nullptr)
				{
					throw RestApiError(404, "Player not found by eosId: " + eosId);
				}
				return pc;
			}

			if (source.contains("playerId"))
			{
				const uint64 playerId = source["playerId"].get<uint64>();
				AShooterPlayerController* pc = FindPlayerById(playerId);
				if (pc == nullptr)
				{
					throw RestApiError(404, "Player not found by playerId");
				}
				return pc;
			}

			throw RestApiError(400, "Provide one of: steamName, eosId, playerId");
		}

		FVector ReadVector(const nlohmann::json& body)
		{
			if (!body.contains("x") || !body.contains("y") || !body.contains("z"))
			{
				throw RestApiError(400, "x, y and z are required");
			}

			const float x = static_cast<float>(body.at("x").get<double>());
			const float y = static_cast<float>(body.at("y").get<double>());
			const float z = static_cast<float>(body.at("z").get<double>());
			return FVector{x, y, z};
		}
	} // namespace

	nlohmann::json GetStatus()
	{
		const auto status = AsaApi::GetApiUtils().GetStatus();
		const int onlinePlayers = AsaApi::GetApiUtils().GetWorld()->PlayerControllerListField().Num();

		return {
			{"status", status == AsaApi::ServerStatus::Ready ? "Ready" : "Loading"},
			{"onlinePlayers", onlinePlayers}
		};
	}

	nlohmann::json ListPlayers()
	{
		nlohmann::json players = nlohmann::json::array();

		auto& controllers = AsaApi::GetApiUtils().GetWorld()->PlayerControllerListField();
		for (TWeakObjectPtr<APlayerController> weakPc : controllers)
		{
			auto* pc = static_cast<AShooterPlayerController*>(weakPc.Get());
			if (pc == nullptr)
			{
				continue;
			}

			FString eosId;
			pc->GetUniqueNetIdAsString(&eosId);

			const FVector pos = AsaApi::IApiUtils::GetPosition(pc);

			players.push_back({
				{"playerId", AsaApi::IApiUtils::GetPlayerID(static_cast<AController*>(pc))},
				{"steamName", AsaApi::IApiUtils::GetSteamName(pc).ToStringUTF8()},
				{"characterName", AsaApi::IApiUtils::GetCharacterName(pc).ToStringUTF8()},
				{"eosId", eosId.ToStringUTF8()},
				{"tribeId", AsaApi::IApiUtils::GetTribeID(pc)},
				{"ip", AsaApi::IApiUtils::GetIPAddress(pc).ToStringUTF8()},
				{"isDead", AsaApi::IApiUtils::IsPlayerDead(pc)},
				{"position", {{"x", pos.X}, {"y", pos.Y}, {"z", pos.Z}}}
			});
		}

		return {{"players", players}};
	}

	nlohmann::json KickPlayer(const nlohmann::json& body)
	{
		AShooterGameMode* gameMode = AsaApi::GetApiUtils().GetShooterGameMode();

		if (body.contains("steamName") && body["steamName"].is_string())
		{
			FString steamName = FString::FromStringUTF8(body["steamName"].get<std::string>());
			const bool success = gameMode->KickPlayer(&steamName);
			return {{"success", success}};
		}

		AShooterPlayerController* pc = ResolvePlayer(body);
		FString reason = FString::FromStringUTF8(body.value("reason", std::string("Kicked via REST API")));
		gameMode->KickPlayerController(pc, reason);
		return {{"success", true}};
	}

	nlohmann::json BanPlayer(const nlohmann::json& body)
	{
		if (!body.contains("steamName") || !body["steamName"].is_string())
		{
			throw RestApiError(400, "steamName is required to ban a player");
		}

		const unsigned int durationMinutes = body.value("durationMinutes", 0u);
		FString steamName = FString::FromStringUTF8(body["steamName"].get<std::string>());
		const bool success = AsaApi::GetApiUtils().GetShooterGameMode()->BanPlayer(&steamName, durationMinutes);
		return {{"success", success}};
	}

	nlohmann::json Broadcast(const nlohmann::json& body)
	{
		if (!body.contains("message") || !body["message"].is_string())
		{
			throw RestApiError(400, "message is required");
		}

		const std::wstring message = AsaApi::Tools::Utf8Decode(EscapeFmtBraces(body["message"].get<std::string>()));
		const FLinearColor color{1.f, 1.f, 1.f, 1.f};

		AsaApi::GetApiUtils().SendServerMessageToAll(color, message.c_str());

		if (body.value("alsoChat", false))
		{
			FString sender = FString::FromStringUTF8(body.value("senderName", std::string("Server")));
			AsaApi::GetApiUtils().SendChatMessageToAll(sender, message.c_str());
		}

		return {{"success", true}};
	}

	nlohmann::json MessagePlayer(const nlohmann::json& body)
	{
		if (!body.contains("message") || !body["message"].is_string())
		{
			throw RestApiError(400, "message is required");
		}

		AShooterPlayerController* pc = ResolvePlayer(body);
		FString sender = FString::FromStringUTF8(body.value("senderName", std::string("Server")));
		const std::wstring message = AsaApi::Tools::Utf8Decode(EscapeFmtBraces(body["message"].get<std::string>()));

		AsaApi::GetApiUtils().SendChatMessage(pc, sender, message.c_str());
		return {{"success", true}};
	}

	nlohmann::json TeleportToPosition(const nlohmann::json& body)
	{
		AShooterPlayerController* pc = ResolvePlayer(body);
		const FVector pos = ReadVector(body);
		const bool success = AsaApi::IApiUtils::TeleportToPos(pc, pos);
		return {{"success", success}};
	}

	nlohmann::json TeleportToPlayer(const nlohmann::json& body)
	{
		if (!body.contains("from") || !body.contains("to"))
		{
			throw RestApiError(400, "from and to player selectors are required");
		}

		AShooterPlayerController* me = ResolvePlayer(body["from"]);
		AShooterPlayerController* him = ResolvePlayer(body["to"]);
		const bool checkForDino = body.value("checkForDino", true);
		const float maxDist = body.value("maxDistance", -1.0f);

		const std::optional<FString> error = AsaApi::IApiUtils::TeleportToPlayer(me, him, checkForDino, maxDist);
		if (error.has_value())
		{
			return {{"success", false}, {"reason", error->ToStringUTF8()}};
		}

		return {{"success", true}};
	}

	nlohmann::json SpawnDino(const nlohmann::json& body)
	{
		if (!body.contains("blueprint") || !body["blueprint"].is_string())
		{
			throw RestApiError(400, "blueprint is required");
		}

		AShooterPlayerController* nearPlayer = nullptr;
		if (body.contains("nearPlayer"))
		{
			nearPlayer = ResolvePlayer(body["nearPlayer"]);
		}

		FVector location{0.f, 0.f, 0.f};
		FVector* locationPtr = nullptr;
		if (body.contains("x") && body.contains("y") && body.contains("z"))
		{
			location = ReadVector(body);
			locationPtr = &location;
		}

		FString blueprint = FString::FromStringUTF8(body["blueprint"].get<std::string>());
		const int level = body.value("level", 1);
		const bool forceTame = body.value("forceTame", false);
		const bool neutered = body.value("neutered", false);

		APrimalDinoCharacter* dino =
			AsaApi::GetApiUtils().SpawnDino(nearPlayer, blueprint, locationPtr, level, forceTame, neutered);
		if (dino == nullptr)
		{
			throw RestApiError(500,
				"Failed to spawn dino - check the blueprint path and that at least one player is online");
		}

		return {{"success", true}};
	}

	nlohmann::json SpawnItem(const nlohmann::json& body)
	{
		if (!body.contains("blueprint") || !body["blueprint"].is_string())
		{
			throw RestApiError(400, "blueprint is required");
		}

		const FVector pos = ReadVector(body);
		const std::wstring blueprint = AsaApi::Tools::Utf8Decode(body["blueprint"].get<std::string>());
		const int amount = body.value("amount", 1);
		const float quality = body.value("quality", 0.0f);
		const bool forceBlueprint = body.value("forceBlueprint", false);
		const float lifeSpan = body.value("lifeSpan", 0.0f);

		const bool success =
			AsaApi::GetApiUtils().SpawnDrop(blueprint.c_str(), pos, amount, quality, forceBlueprint, lifeSpan);
		if (!success)
		{
			throw RestApiError(500, "Failed to spawn item - at least one player must be online");
		}

		return {{"success", true}};
	}

	nlohmann::json GetInventoryCount(const std::string& playerKey, const std::string& itemName)
	{
		if (itemName.empty())
		{
			throw RestApiError(400, "item query parameter is required");
		}

		AShooterPlayerController* pc =
			AsaApi::GetApiUtils().FindPlayerFromPlatformName(FString::FromStringUTF8(playerKey));
		if (pc == nullptr)
		{
			throw RestApiError(404, "Player not found by steamName: " + playerKey);
		}

		const int count = AsaApi::IApiUtils::GetInventoryItemCount(pc, FString::FromStringUTF8(itemName));
		return {{"item", itemName}, {"count", count}};
	}

	nlohmann::json GetTribeId(const std::string& playerKey)
	{
		AShooterPlayerController* pc =
			AsaApi::GetApiUtils().FindPlayerFromPlatformName(FString::FromStringUTF8(playerKey));
		if (pc == nullptr)
		{
			throw RestApiError(404, "Player not found by steamName: " + playerKey);
		}

		return {{"tribeId", AsaApi::IApiUtils::GetTribeID(pc)}};
	}

	nlohmann::json SaveWorld()
	{
		AsaApi::GetApiUtils().GetShooterGameMode()->SaveWorld(true, true, false);
		return {{"success", true}};
	}
} // namespace ArkRestApi::Routes
