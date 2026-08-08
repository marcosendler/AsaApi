#pragma once

#include <string>

namespace ArkRestApi
{
	struct RestApiConfig
	{
		bool enabled = true;
		std::string bindAddress = "0.0.0.0";
		unsigned short port = 8766;
		std::string bearerToken;
		int maxQueueWaitMs = 5000;
		int maxThreads = 8;
	};
} // namespace ArkRestApi
