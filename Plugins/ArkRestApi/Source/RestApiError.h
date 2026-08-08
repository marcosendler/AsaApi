#pragma once

#include <stdexcept>
#include <string>

namespace ArkRestApi
{
	// Thrown by route handlers to short-circuit with a specific HTTP status code.
	// Caught in HttpServer.cpp and turned into a JSON error response.
	class RestApiError : public std::runtime_error
	{
	public:
		RestApiError(int httpStatus, const std::string& message)
			: std::runtime_error(message), httpStatus(httpStatus)
		{
		}

		int httpStatus;
	};
} // namespace ArkRestApi
