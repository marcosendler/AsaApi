#pragma once

#include <memory>
#include <string>
#include <vector>

#include <Logger/spdlog/spdlog.h>
#include <Tools.h>

namespace ArkRestApi
{
	// Dedicated logger for this plugin, writing to its own file inside the plugin's
	// folder (ArkApi/Plugins/ArkRestApi/ArkRestApi.log) instead of the shared AsaApi
	// log where every plugin's output gets mixed together - makes it possible to tail
	// just this plugin's messages. Also mirrors to the console like the shared logger does.
	inline std::shared_ptr<spdlog::logger>& GetPluginLog()
	{
		static std::shared_ptr<spdlog::logger> logger = []
		{
			const std::string path = AsaApi::Tools::GetCurrentDir() + "/ArkApi/Plugins/ArkRestApi/ArkRestApi.log";

			std::vector<spdlog::sink_ptr> sinks{
				std::make_shared<spdlog::sinks::wincolor_stdout_sink_mt>(),
				std::make_shared<spdlog::sinks::rotating_file_sink_mt>(path, 1024 * 1024, 5)
			};

			auto log = std::make_shared<spdlog::logger>("ArkRestApi", begin(sinks), end(sinks));
			log->set_pattern("%D %R [%l] %v");
			log->flush_on(spdlog::level::info);
			return log;
		}();

		return logger;
	}
} // namespace ArkRestApi
