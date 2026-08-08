#include <fstream>
#include <memory>

#include <ICommands.h>
#include <Tools.h>
#include <json.hpp>

#include "GameThreadDispatcher.h"
#include "HttpServer.h"
#include "PluginLog.h"
#include "RestApiConfig.h"

namespace ArkRestApi
{
	namespace
	{
		std::unique_ptr<HttpServer> g_httpServer;

		RestApiConfig LoadConfig()
		{
			RestApiConfig config;
			const std::string path = AsaApi::Tools::GetCurrentDir() + "/ArkApi/Plugins/ArkRestApi/config.json";

			std::ifstream file(path);
			if (!file.is_open())
			{
				ArkRestApi::GetPluginLog()->warn("ArkRestApi: config.json not found at {}, using defaults", path);
				return config;
			}

			try
			{
				nlohmann::json json;
				file >> json;

				config.enabled = json.value("Enabled", config.enabled);
				config.bindAddress = json.value("BindAddress", config.bindAddress);
				config.port = json.value("Port", config.port);
				config.bearerToken = json.value("BearerToken", config.bearerToken);
				config.maxQueueWaitMs = json.value("MaxQueueWaitMs", config.maxQueueWaitMs);
				config.maxThreads = json.value("MaxThreads", config.maxThreads);
			}
			catch (const std::exception& error)
			{
				ArkRestApi::GetPluginLog()->error("ArkRestApi: failed to parse config.json: {}", error.what());
			}

			return config;
		}
	} // namespace
} // namespace ArkRestApi

extern "C" __declspec(dllexport) void Plugin_Init()
{
	const ArkRestApi::RestApiConfig config = ArkRestApi::LoadConfig();

	if (!config.enabled)
	{
		ArkRestApi::GetPluginLog()->info("ArkRestApi: disabled via config.json");
		return;
	}

	if (config.bearerToken.empty() || config.bearerToken == "CHANGE_ME_TO_A_LONG_RANDOM_SECRET")
	{
		ArkRestApi::GetPluginLog()->error("ArkRestApi: refusing to start - set a real BearerToken in config.json");
		return;
	}

	ArkRestApi::g_httpServer = std::make_unique<ArkRestApi::HttpServer>();
	if (!ArkRestApi::g_httpServer->Start(config))
	{
		ArkRestApi::g_httpServer.reset();
		return;
	}

	AsaApi::GetCommands().AddOnTickCallback(FString::FromStringUTF8("ArkRestApi_Tick"), [](float delta)
	{
		ArkRestApi::GameThreadDispatcher::Get().OnTick(delta);
	});
}

extern "C" __declspec(dllexport) void Plugin_Unload()
{
	AsaApi::GetCommands().RemoveOnTickCallback(FString::FromStringUTF8("ArkRestApi_Tick"));

	if (ArkRestApi::g_httpServer)
	{
		ArkRestApi::g_httpServer->Stop();
		ArkRestApi::g_httpServer.reset();
	}
}
