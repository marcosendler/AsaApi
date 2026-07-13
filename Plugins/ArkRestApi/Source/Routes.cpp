#include "Routes.h"

#include <optional>

#include <API/ARK/Ark.h>
#include <IApiUtils.h>
#include <Tools.h>

#include "PluginLog.h"
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

		std::optional<EPrimalCharacterStatusValue::Type> ParseStatName(const std::string& name)
		{
			if (name == "health") return EPrimalCharacterStatusValue::Health;
			if (name == "stamina") return EPrimalCharacterStatusValue::Stamina;
			if (name == "torpidity") return EPrimalCharacterStatusValue::Torpidity;
			if (name == "oxygen") return EPrimalCharacterStatusValue::Oxygen;
			if (name == "food") return EPrimalCharacterStatusValue::Food;
			if (name == "water") return EPrimalCharacterStatusValue::Water;
			if (name == "temperature") return EPrimalCharacterStatusValue::Temperature;
			if (name == "weight") return EPrimalCharacterStatusValue::Weight;
			if (name == "meleeDamageMultiplier") return EPrimalCharacterStatusValue::MeleeDamageMultiplier;
			if (name == "speedMultiplier") return EPrimalCharacterStatusValue::SpeedMultiplier;
			if (name == "temperatureFortitude") return EPrimalCharacterStatusValue::TemperatureFortitude;
			if (name == "craftingSpeedMultiplier") return EPrimalCharacterStatusValue::CraftingSpeedMultiplier;
			return std::nullopt;
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

			auto* playerState = static_cast<AShooterPlayerState*>(pc->PlayerStateField().Get());
			const int level = playerState != nullptr ? playerState->GetCharacterLevel() : 0;

			players.push_back({
				{"playerId", AsaApi::IApiUtils::GetPlayerID(static_cast<AController*>(pc))},
				{"steamName", AsaApi::IApiUtils::GetSteamName(pc).ToStringUTF8()},
				{"characterName", AsaApi::IApiUtils::GetCharacterName(pc).ToStringUTF8()},
				{"eosId", eosId.ToStringUTF8()},
				{"level", level},
				{"tribeId", AsaApi::IApiUtils::GetTribeID(pc)},
				{"tribeName", pc->GetTribeName().ToStringUTF8()},
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

	nlohmann::json NotifyPlayer(const nlohmann::json& body)
	{
		if (!body.contains("message") || !body["message"].is_string())
		{
			throw RestApiError(400, "message is required");
		}

		AShooterPlayerController* pc = ResolvePlayer(body);
		const std::wstring message = AsaApi::Tools::Utf8Decode(EscapeFmtBraces(body["message"].get<std::string>()));

		const FLinearColor color{
			body.value("colorR", 1.0f),
			body.value("colorG", 1.0f),
			body.value("colorB", 1.0f),
			body.value("colorA", 1.0f)
		};
		const float displayScale = body.value("displayScale", 1.3f);
		const float displayTime = body.value("displayTime", 5.0f);

		AsaApi::GetApiUtils().SendNotification(pc, color, displayScale, displayTime, nullptr, message.c_str());
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

	nlohmann::json SpawnDinoAsCryopod(const nlohmann::json& body)
	{
		if (!body.contains("blueprint") || !body["blueprint"].is_string())
		{
			throw RestApiError(400, "blueprint is required");
		}

		auto log = [](const char* msg)
		{
			GetPluginLog()->info(msg);
		};

		// The dino is captured into a cryopod owned by this player, so (unlike SpawnDino)
		// there's no free-standing "nearPlayer"/x,y,z placement - it's always spawned near
		// the recipient and immediately packed into the item.
		AShooterPlayerController* pc = ResolvePlayer(body);

		FString blueprint = FString::FromStringUTF8(body["blueprint"].get<std::string>());
		const int level = body.value("level", 1);
		const bool forceTame = body.value("forceTame", true);
		const bool neutered = body.value("neutered", false);

		log("ArkRestApi: [cryopod] calling SpawnDino");
		APrimalDinoCharacter* dino =
			AsaApi::GetApiUtils().SpawnDino(pc, blueprint, nullptr, level, forceTame, neutered);
		if (dino == nullptr)
		{
			throw RestApiError(500,
				"Failed to spawn dino - check the blueprint path and that at least one player is online");
		}
		log("ArkRestApi: [cryopod] SpawnDino ok");

		if (body.contains("gender") && body["gender"].is_string())
		{
			const std::string gender = body["gender"].get<std::string>();
			if (gender != "male" && gender != "female")
			{
				throw RestApiError(400, "gender must be \"male\" or \"female\"");
			}

			log("ArkRestApi: [cryopod] setting gender");
			dino->bIsFemale() = (gender == "female");
			dino->hasAlreadySetGender() = true;
			log("ArkRestApi: [cryopod] gender set ok");
		}

		// Cryopod-ing a dino erases the "ride/use without a saddle" bypass that forceTame
		// grants (documented ARK behavior) - a forceTame'd dino redeployed from a cryopod
		// loses inventory access unless it genuinely has a saddle equipped before capture.
		// Equip it directly on the live dino (rather than as a loose item) so the capture
		// below picks it up as part of the dino's real current state.
		if (body.contains("saddleBlueprint") && body["saddleBlueprint"].is_string())
		{
			FString saddleBlueprint = FString::FromStringUTF8(body["saddleBlueprint"].get<std::string>());
			const float saddleQuality = body.value("saddleQuality", 0.0f);

			log("ArkRestApi: [cryopod] calling GiveSaddleFromString");
			UPrimalItem* saddle = dino->GiveSaddleFromString(&saddleBlueprint, saddleQuality, 0.0f, true);
			if (saddle == nullptr)
			{
				throw RestApiError(500, "Failed to equip saddle - check the saddleBlueprint path");
			}
			log("ArkRestApi: [cryopod] GiveSaddleFromString ok");
		}

		// AShooterPlayerController::GiveCryoItemAndCaptureDino is built for the normal
		// player-aims-a-real-cryopod-at-a-dino interaction flow and does not reliably
		// preserve dino/saddle state when invoked directly from server-side code outside
		// that flow. Instead, build the cryopod item by hand the same way established
		// ARK plugins (e.g. ArkShop's GiveDinosInCryopods) do it: create an empty cryopod
		// item, read the dino's current state into a FCustomItemData snapshot, attach that
		// snapshot to the item, hand the item to the player, then remove the live dino.
		// Default is the vanilla Extinction empty cryopod. Servers running a cryopod-replacing
		// mod (e.g. Alfa Cryopod, Pelayori's Cryo Storage) may need to override this with that
		// mod's own item blueprint path via "cryopodBlueprint" - the vanilla class can fail to
		// construct (crash) once such a mod has overridden the server's PrimalGameData.
		// No "_C" suffix here, matching the exact default the official ASA ArkShop plugin uses
		// for this same call (ArkServerApi/ASA-Plugins) - BPLoadClass appears to resolve this
		// form fine for this item.
		const FString kCryopodBlueprint = body.contains("cryopodBlueprint") && body["cryopodBlueprint"].is_string()
			? FString::FromStringUTF8(body["cryopodBlueprint"].get<std::string>())
			: FString::FromStringUTF8(
				"Blueprint'/Game/Extinction/CoreBlueprints/Weapons/PrimalItem_WeaponEmptyCryopod."
				"PrimalItem_WeaponEmptyCryopod'");

		GetPluginLog()->info("ArkRestApi: [cryopod] using cryopod blueprint: {}", kCryopodBlueprint.ToStringUTF8());
		log("ArkRestApi: [cryopod] loading cryopod blueprint class");
		TSubclassOf<UPrimalItem> cryopodArchetype;
		cryopodArchetype.uClass = UVictoryCore::BPLoadClass(kCryopodBlueprint);
		if (cryopodArchetype.uClass == nullptr)
		{
			throw RestApiError(500, "Failed to load the empty cryopod blueprint");
		}
		log("ArkRestApi: [cryopod] cryopod blueprint class loaded ok");

		// Matches the exact AddNewItem call used by the official ASA ArkShop plugin's
		// GiveDino (ArkServerApi/ASA-Plugins) for this same purpose: GiveToInventory=nullptr
		// and quantityOverride=0 - passing 1 there (an earlier attempt here) crashed the
		// server, so this specific item type appears not to tolerate an explicit override.
		log("ArkRestApi: [cryopod] creating cryopod item via AddNewItem");
		UPrimalItem* cryopodItem = UPrimalItem::AddNewItem(cryopodArchetype, nullptr, false, false, 0.0f, false, 0,
			false, 0, false, nullptr, 0, false, false, true, false, false);
		if (cryopodItem == nullptr)
		{
			throw RestApiError(500, "Failed to create the cryopod item");
		}
		log("ArkRestApi: [cryopod] cryopod item created ok");

		log("ArkRestApi: [cryopod] calling GetCryoDinoData");
		FCustomItemData cryoData;
		if (UVictoryCore::GetCryoDinoData(&cryoData, pc, dino) == nullptr)
		{
			throw RestApiError(500, "Failed to capture the dino into the cryopod");
		}
		log("ArkRestApi: [cryopod] GetCryoDinoData returned ok");

		log("ArkRestApi: [cryopod] calling SetCustomItemData");
		cryopodItem->SetCustomItemData(&cryoData);
		log("ArkRestApi: [cryopod] SetCustomItemData ok, calling UpdatedItem");
		cryopodItem->UpdatedItem(true, false);
		log("ArkRestApi: [cryopod] UpdatedItem ok, calling AddItemObject");
		pc->GetPlayerInventoryComponent()->AddItemObject(cryopodItem);
		log("ArkRestApi: [cryopod] AddItemObject ok, calling dino->Destroy");

		dino->Destroy(true, false);
		log("ArkRestApi: [cryopod] dino->Destroy ok, done");
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

	nlohmann::json GiveItem(const nlohmann::json& body)
	{
		if (!body.contains("blueprint") || !body["blueprint"].is_string())
		{
			throw RestApiError(400, "blueprint is required");
		}

		AShooterPlayerController* pc = ResolvePlayer(body);
		FString blueprint = FString::FromStringUTF8(body["blueprint"].get<std::string>());
		const int quantity = body.value("quantity", 1);
		const float quality = body.value("quality", 0.0f);
		const bool forceBlueprint = body.value("forceBlueprint", false);
		const bool autoEquip = body.value("autoEquip", false);
		const float minRandomQuality = body.value("minRandomQuality", 0.0f);

		const bool success = pc->GiveItem(&blueprint, quantity, quality, forceBlueprint, autoEquip, minRandomQuality);
		if (!success)
		{
			throw RestApiError(500, "Failed to give item - check the blueprint path");
		}

		return {{"success", true}};
	}

	nlohmann::json GiveItems(const nlohmann::json& body)
	{
		if (!body.contains("items") || !body["items"].is_array() || body["items"].empty())
		{
			throw RestApiError(400, "items must be a non-empty array");
		}

		AShooterPlayerController* pc = ResolvePlayer(body);

		nlohmann::json results = nlohmann::json::array();
		for (const auto& item : body["items"])
		{
			if (!item.contains("blueprint") || !item["blueprint"].is_string())
			{
				results.push_back({{"success", false}, {"error", "blueprint is required"}});
				continue;
			}

			FString blueprint = FString::FromStringUTF8(item["blueprint"].get<std::string>());
			const int quantity = item.value("quantity", 1);
			const float quality = item.value("quality", 0.0f);
			const bool forceBlueprint = item.value("forceBlueprint", false);
			const bool autoEquip = item.value("autoEquip", false);
			const float minRandomQuality = item.value("minRandomQuality", 0.0f);

			const bool success =
				pc->GiveItem(&blueprint, quantity, quality, forceBlueprint, autoEquip, minRandomQuality);
			results.push_back(success
				? nlohmann::json{{"success", true}}
				: nlohmann::json{{"success", false}, {"error", "Failed to give item - check the blueprint path"}});
		}

		return {{"results", results}};
	}

	nlohmann::json GiveEngrams(const nlohmann::json& body)
	{
		AShooterPlayerController* pc = ResolvePlayer(body);
		const bool forceAll = body.value("forceAll", true);
		const bool tekOnly = body.value("tekOnly", false);

		pc->GiveEngrams(forceAll, tekOnly);
		return {{"success", true}};
	}

	nlohmann::json GiveExperience(const nlohmann::json& body)
	{
		if (!body.contains("amount"))
		{
			throw RestApiError(400, "amount is required");
		}

		AShooterPlayerController* pc = ResolvePlayer(body);
		UShooterCheatManager* cheatManager = AsaApi::IApiUtils::GetCheatManagerByPC(pc);
		if (cheatManager == nullptr)
		{
			throw RestApiError(500, "Player has no cheat manager available");
		}

		const uint64 playerId = AsaApi::IApiUtils::GetPlayerID(static_cast<AController*>(pc));
		const float amount = body.at("amount").get<float>();
		const bool fromTribeShare = body.value("fromTribeShare", false);
		const bool preventSharingWithTribe = body.value("preventSharingWithTribe", false);

		cheatManager->GiveExpToPlayer(static_cast<__int64>(playerId), amount, fromTribeShare, preventSharingWithTribe);
		return {{"success", true}};
	}

	nlohmann::json SetPlayerLevel(const nlohmann::json& body)
	{
		if (!body.contains("level"))
		{
			throw RestApiError(400, "level is required");
		}

		AShooterPlayerController* pc = ResolvePlayer(body);
		UShooterCheatManager* cheatManager = AsaApi::IApiUtils::GetCheatManagerByPC(pc);
		if (cheatManager == nullptr)
		{
			throw RestApiError(500, "Player has no cheat manager available");
		}

		const uint64 playerId = AsaApi::IApiUtils::GetPlayerID(static_cast<AController*>(pc));
		const __int16 level = static_cast<__int16>(body.at("level").get<int>());

		cheatManager->SetPlayerLevel(static_cast<__int64>(playerId), level);
		return {{"success", true}};
	}

	nlohmann::json ClearInventory(const nlohmann::json& body)
	{
		AShooterPlayerController* pc = ResolvePlayer(body);
		UShooterCheatManager* cheatManager = AsaApi::IApiUtils::GetCheatManagerByPC(pc);
		if (cheatManager == nullptr)
		{
			throw RestApiError(500, "Player has no cheat manager available");
		}

		const uint64 playerId = AsaApi::IApiUtils::GetPlayerID(static_cast<AController*>(pc));
		const bool clearInventory = body.value("clearInventory", true);
		const bool clearSlotItems = body.value("clearSlotItems", true);
		const bool clearEquippedItems = body.value("clearEquippedItems", true);

		cheatManager->ClearPlayerInventory(static_cast<int>(playerId), clearInventory, clearSlotItems,
			clearEquippedItems);
		return {{"success", true}};
	}

	nlohmann::json ToggleGodMode(const nlohmann::json& body)
	{
		AShooterPlayerController* pc = ResolvePlayer(body);
		UShooterCheatManager* cheatManager = AsaApi::IApiUtils::GetCheatManagerByPC(pc);
		if (cheatManager == nullptr)
		{
			throw RestApiError(500, "Player has no cheat manager available");
		}

		cheatManager->God();
		return {{"success", true}, {"note", "God mode toggled - calling this again switches it back off"}};
	}

	nlohmann::json SetPlayerStats(const nlohmann::json& body)
	{
		if (!body.contains("stats") || !body["stats"].is_object() || body["stats"].empty())
		{
			throw RestApiError(400, "stats must be a non-empty object");
		}

		AShooterPlayerController* pc = ResolvePlayer(body);
		AShooterCharacter* character = pc->GetPlayerCharacter();
		if (character == nullptr)
		{
			throw RestApiError(500, "Player has no active character");
		}

		UPrimalCharacterStatusComponent* statusComponent = character->GetCharacterStatusComponent();
		if (statusComponent == nullptr)
		{
			throw RestApiError(500, "Player character has no status component");
		}

		nlohmann::json results = nlohmann::json::object();
		for (auto it = body["stats"].begin(); it != body["stats"].end(); ++it)
		{
			const std::string statName = it.key();
			const nlohmann::json& entry = it.value();

			const std::optional<EPrimalCharacterStatusValue::Type> statType = ParseStatName(statName);
			if (!statType.has_value())
			{
				results[statName] = {{"success", false}, {"error", "Unknown stat name"}};
				continue;
			}

			if (!entry.contains("currentValue") && !entry.contains("maxValue"))
			{
				results[statName] = {{"success", false}, {"error", "Provide currentValue and/or maxValue"}};
				continue;
			}

			// Max first, then current: raising current above a stale max would otherwise
			// get clamped back down immediately.
			if (entry.contains("maxValue"))
			{
				statusComponent->SetMaxStatusValue(*statType, entry["maxValue"].get<float>());
			}

			if (entry.contains("currentValue"))
			{
				statusComponent->BPDirectSetCurrentStatusValue(*statType, entry["currentValue"].get<float>());
			}

			results[statName] = {{"success", true}};
		}

		return {{"results", results}};
	}

	nlohmann::json GetInventoryCount(const nlohmann::json& playerSelector, const std::string& itemName)
	{
		if (itemName.empty())
		{
			throw RestApiError(400, "item query parameter is required");
		}

		AShooterPlayerController* pc = ResolvePlayer(playerSelector);
		const int count = AsaApi::IApiUtils::GetInventoryItemCount(pc, FString::FromStringUTF8(itemName));
		return {{"item", itemName}, {"count", count}};
	}

	nlohmann::json GetTribeId(const nlohmann::json& playerSelector)
	{
		AShooterPlayerController* pc = ResolvePlayer(playerSelector);

		return {
			{"tribeId", AsaApi::IApiUtils::GetTribeID(pc)},
			{"tribeName", pc->GetTribeName().ToStringUTF8()}
		};
	}

	nlohmann::json SaveWorld()
	{
		AsaApi::GetApiUtils().GetShooterGameMode()->SaveWorld(true, true, false);
		return {{"success", true}};
	}

	nlohmann::json DestroyAllEnemies()
	{
		UShooterCheatManager* cheatManager = AsaApi::GetApiUtils().GetCheatManager();
		if (cheatManager == nullptr)
		{
			throw RestApiError(500, "No cheat manager available");
		}

		cheatManager->DestroyAllEnemies();
		return {{"success", true}};
	}

	nlohmann::json SetTimeOfDay(const nlohmann::json& body)
	{
		if (!body.contains("time") || !body["time"].is_string())
		{
			throw RestApiError(400, "time is required, e.g. \"1200\"");
		}

		UShooterCheatManager* cheatManager = AsaApi::GetApiUtils().GetCheatManager();
		if (cheatManager == nullptr)
		{
			throw RestApiError(500, "No cheat manager available");
		}

		FString time = FString::FromStringUTF8(body["time"].get<std::string>());
		cheatManager->SetTimeOfDay(&time);
		return {{"success", true}};
	}
} // namespace ArkRestApi::Routes
