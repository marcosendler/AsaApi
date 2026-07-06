#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <optional>

#include <json.hpp>

namespace ArkRestApi
{
	// All ARK/Unreal Engine calls must happen on the main game thread.
	// HTTP requests are handled on Poco worker threads, so route handlers
	// are queued here and executed from the AddOnTickCallback on the next tick.
	class GameThreadDispatcher
	{
	public:
		static GameThreadDispatcher& Get();

		using JsonHandler = std::function<nlohmann::json()>;

		// Queues `handler` to run on the game thread and blocks the calling thread
		// until it completes or `timeoutMs` elapses. Returns std::nullopt on timeout.
		// If the wait times out, the queued handler is marked cancelled so it will
		// not run (and will not touch objects owned by the timed-out caller).
		std::optional<nlohmann::json> RunOnGameThreadJson(JsonHandler handler, int timeoutMs);

		void OnTick(float deltaSeconds);

	private:
		GameThreadDispatcher() = default;

		std::mutex queueMutex_;
		std::deque<std::function<void()>> queue_;
	};
} // namespace ArkRestApi
