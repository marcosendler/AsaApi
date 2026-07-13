#include "HttpServer.h"

#include <sstream>
#include <unordered_map>

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/URI.h>

#include <json.hpp>

#include "ApiRouter.h"
#include "PluginLog.h"

namespace ArkRestApi
{
	namespace
	{
		bool ConstantTimeEquals(const std::string& a, const std::string& b)
		{
			if (a.size() != b.size())
			{
				return false;
			}

			unsigned char diff = 0;
			for (size_t i = 0; i < a.size(); ++i)
			{
				diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
			}
			return diff == 0;
		}

		class RestRequestHandler final : public Poco::Net::HTTPRequestHandler
		{
		public:
			explicit RestRequestHandler(std::shared_ptr<RestApiConfig> config) : config_(std::move(config))
			{
			}

			void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override
			{
				response.setContentType("application/json");

				const Poco::URI uri(request.getURI());
				const std::string path = uri.getPath();

				if (path == "/health")
				{
					response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
					std::ostream& out = response.send();
					out << nlohmann::json{{"status", "ok"}}.dump();
					return;
				}

				if (!IsAuthorized(request))
				{
					response.setStatus(Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
					std::ostream& out = response.send();
					out << nlohmann::json{{"error", "Missing or invalid bearer token"}}.dump();
					return;
				}

				std::unordered_map<std::string, std::string> query;
				for (const auto& param : uri.getQueryParameters())
				{
					query[param.first] = param.second;
				}

				nlohmann::json body = nlohmann::json::object();
				const std::string& method = request.getMethod();
				if (method == "POST" || method == "PUT" || method == "PATCH")
				{
					std::istream& in = request.stream();
					std::ostringstream ss;
					ss << in.rdbuf();
					const std::string raw = ss.str();

					if (!raw.empty())
					{
						try
						{
							body = nlohmann::json::parse(raw);
						}
						catch (const nlohmann::json::exception& error)
						{
							response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
							std::ostream& out = response.send();
							out << nlohmann::json{{"error", std::string("Invalid JSON body: ") + error.what()}}.dump();
							return;
						}
					}
				}

				const ApiResponse apiResponse =
					ApiRouter::Dispatch(method, path, query, body, config_->maxQueueWaitMs);

				response.setStatus(static_cast<Poco::Net::HTTPResponse::HTTPStatus>(apiResponse.httpStatus));
				std::ostream& out = response.send();
				out << apiResponse.body.dump();
			}

		private:
			bool IsAuthorized(Poco::Net::HTTPServerRequest& request) const
			{
				if (config_->bearerToken.empty() || !request.has("Authorization"))
				{
					return false;
				}

				const std::string header = request.get("Authorization");
				static const std::string prefix = "Bearer ";
				if (header.size() <= prefix.size() || header.compare(0, prefix.size(), prefix) != 0)
				{
					return false;
				}

				return ConstantTimeEquals(header.substr(prefix.size()), config_->bearerToken);
			}

			std::shared_ptr<RestApiConfig> config_;
		};

		class RestRequestHandlerFactory final : public Poco::Net::HTTPRequestHandlerFactory
		{
		public:
			explicit RestRequestHandlerFactory(std::shared_ptr<RestApiConfig> config) : config_(std::move(config))
			{
			}

			Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override
			{
				return new RestRequestHandler(config_);
			}

		private:
			std::shared_ptr<RestApiConfig> config_;
		};
	} // namespace

	HttpServer::HttpServer() = default;
	HttpServer::~HttpServer() = default;

	bool HttpServer::Start(const RestApiConfig& config)
	{
		try
		{
			auto sharedConfig = std::make_shared<RestApiConfig>(config);

			Poco::Net::ServerSocket socket;
			socket.bind(Poco::Net::SocketAddress(config.bindAddress, config.port), true);
			socket.listen();

			auto* params = new Poco::Net::HTTPServerParams;
			params->setMaxThreads(config.maxThreads);
			params->setMaxQueued(64);

			server_ = std::make_unique<Poco::Net::HTTPServer>(
				new RestRequestHandlerFactory(sharedConfig), socket, params);
			server_->start();

			GetPluginLog()->info("ArkRestApi: listening on {}:{}", config.bindAddress, config.port);
			return true;
		}
		catch (const std::exception& error)
		{
			GetPluginLog()->error("ArkRestApi: failed to start HTTP server: {}", error.what());
			server_.reset();
			return false;
		}
	}

	void HttpServer::Stop()
	{
		if (server_)
		{
			server_->stopAll(true);
			server_.reset();
		}
	}
} // namespace ArkRestApi
