#include "players.h"

namespace KZ::API
{
	std::optional<ParseError> Player::Deserialize(const json &json, Player &player)
	{
		if (!json.is_object())
		{
			return KZ::API::ParseError("player is not an object");
		}

		if (!json.contains("name"))
		{
			return KZ::API::ParseError("player object is missing `name` property");
		}

		if (!json["name"].is_string())
		{
			return KZ::API::ParseError("player object's `name` property is not a string");
		}

		player.name = json["name"];

		if (!json.contains("steam_id"))
		{
			return KZ::API::ParseError("player object is missing `steam_id` property");
		}

		if (!json["steam_id"].is_string())
		{
			return KZ::API::ParseError("player object's `steam_id` property is not a string");
		}

		player.steamID = json["steam_id"];

		if (json.contains("is_banned"))
		{
			if (!json["is_banned"].is_boolean())
			{
				return KZ::API::ParseError("player object's `is_banned` property is not a boolean");
			}

			player.isBanned = json["is_banned"];
		}

		return std::nullopt;
	}

	json BhopStats::Serialize() const
	{
		return {{"bhops", bhops}, {"perfs", perfs}};
	}

	json NewPlayer::Serialize() const
	{
		return {{"name", name}, {"steam_id", steamID}, {"ip_address", ipAddress}};
	}

	json CourseSession::Data::Serialize() const
	{
		return {{"playtime", playtime}, {"started_runs", startedRuns}, {"finished_runs", finishedRuns}, {"bhop_stats", bhopStats.Serialize()}};
	}

	void CourseSession::UpdatePlaytime(float additionalPlaytime, Mode mode)
	{
		switch (mode)
		{
			case Mode::VANILLA:
				vanilla.playtime += additionalPlaytime;
				break;

			case Mode::CLASSIC:
				classic.playtime += additionalPlaytime;
				break;
		}
	}

	json CourseSession::Serialize() const
	{
		return {{"vanilla", vanilla.Serialize()}, {"classic", classic.Serialize()}};
	}

	float Session::GoActive()
	{
		currentState = State::ACTIVE;
		float delta = UpdateTime();
		secondsActive += delta;
		return delta;
	}

	float Session::GoAFK()
	{
		currentState = State::AFK;
		float delta = UpdateTime();
		secondsAFK += delta;
		return delta;
	}

	float Session::GoSpectating()
	{
		currentState = State::SPECTATING;
		float delta = UpdateTime();
		secondsSpectating += delta;
		return delta;
	}

	float Session::SwitchCourse(u16 courseID, Mode currentMode)
	{
		currentCourse = courseID;
		auto &courseSession = courseSessions[courseID];
		float delta = 0.0f;

		switch (currentState)
		{
			case State::ACTIVE:
				delta = GoActive();
				break;

			case State::AFK:
				delta = GoAFK();
				break;

			case State::SPECTATING:
				delta = GoSpectating();
				break;
		}

		courseSession.UpdatePlaytime(delta, currentMode);

		return delta;
	}

	void Session::Jump(bool perf)
	{
		bhopStats.bhops++;
		bhopStats.perfs += perf;
	}

	json Session::Serialize() const
	{
		json serializedCourseSessions = json::object();

		for (const auto &[courseID, courseSession] : courseSessions)
		{
			serializedCourseSessions[courseID] = courseSession.Serialize();
		}

		return {{"active", secondsActive},
				{"spectating", secondsSpectating},
				{"afk", secondsAFK},
				{"bhop_stats", bhopStats.Serialize()},
				{"course_sessions", serializedCourseSessions}};
	}

	float Session::UpdateTime()
	{
		float now = g_pKZUtils->GetServerGlobals()->realtime;
		float delta = now - latestTimestamp;
		latestTimestamp = now;
		return delta;
	}

	json PlayerUpdate::Serialize() const
	{
		return {{"name", name}, {"ip_address", ipAddress}, {"preferences", preferences}, {"session", session.Serialize()}};
	}
} // namespace KZ::API
