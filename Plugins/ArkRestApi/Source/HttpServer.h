#pragma once

#include <memory>

#include "RestApiConfig.h"

namespace Poco::Net
{
	class HTTPServer;
}

namespace ArkRestApi
{
	class HttpServer
	{
	public:
		HttpServer();
		~HttpServer();

		bool Start(const RestApiConfig& config);
		void Stop();

	private:
		std::unique_ptr<Poco::Net::HTTPServer> server_;
	};
} // namespace ArkRestApi
