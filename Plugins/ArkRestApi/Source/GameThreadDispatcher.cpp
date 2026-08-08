#include "GameThreadDispatcher.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>

namespace ArkRestApi
{
	GameThreadDispatcher& GameThreadDispatcher::Get()
	{
		static GameThreadDispatcher instance;
		return instance;
	}

	std::optional<nlohmann::json> GameThreadDispatcher::RunOnGameThreadJson(JsonHandler handler, int timeoutMs)
	{
		struct SharedState
		{
			std::mutex mtx;
			std::condition_variable cv;
			bool done = false;
			std::atomic<bool> cancelled{false};
			nlohmann::json result;
		};

		auto state = std::make_shared<SharedState>();

		{
			std::lock_guard<std::mutex> lock(queueMutex_);
			queue_.push_back([state, handler]()
			{
				if (state->cancelled.load())
				{
					return;
				}

				nlohmann::json result;
				try
				{
					result = handler();
				}
				catch (const std::exception& error)
				{
					result = {{"error", error.what()}};
				}
				catch (...)
				{
					result = {{"error", "Unknown error while executing request on the game thread"}};
				}

				std::lock_guard<std::mutex> lk(state->mtx);
				state->result = std::move(result);
				state->done = true;
				state->cv.notify_all();
			});
		}

		std::unique_lock<std::mutex> lock(state->mtx);
		const bool completed = state->cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
			[&state] { return state->done; });

		if (!completed)
		{
			state->cancelled.store(true);
			return std::nullopt;
		}

		return state->result;
	}

	void GameThreadDispatcher::OnTick(float /*deltaSeconds*/)
	{
		std::deque<std::function<void()>> pending;
		{
			std::lock_guard<std::mutex> lock(queueMutex_);
			std::swap(pending, queue_);
		}

		for (auto& task : pending)
		{
			task();
		}
	}
} // namespace ArkRestApi
