#pragma once

#include <string>
#include <unordered_map>

#include <json.hpp>

namespace ArkRestApi
{
	struct ApiResponse
	{
		int httpStatus = 200;
		nlohmann::json body;
	};

	// Routes an already-authenticated HTTP request to the matching Routes:: handler,
	// running it on the game thread and blocking (this is called from a Poco worker
	// thread) until it completes or times out.
	class ApiRouter
	{
	public:
		static ApiResponse Dispatch(
			const std::string& method,
			const std::string& path,
			const std::unordered_map<std::string, std::string>& query,
			const nlohmann::json& body,
			int maxQueueWaitMs);
	};
} // namespace ArkRestApi
