#pragma once

#include <string>
#include <unordered_map>
#include "error.h"
#include "modes.h"
#include "utils/json.h"
#include "utils/utils.h"

namespace KZ::API
{
	/// A player object returned by the API.
	struct Player
	{
		/// Deserializes a `Player` from a JSON object.
		static std::optional<KZ::API::ParseError> Deserialize(const json &json, Player &player);

		/// The player's name.
		std::string name;

		/// The player's SteamID.
		std::string steamID;

		/// Whether the player is currently banned.
		std::optional<bool> isBanned;
	};

	/// Request payload for registering new players to the API.
	struct NewPlayer
	{
		/// The player's name.
		std::string name;

		/// The player's SteamID.
		u64 steamID;

		/// The player's IP address.
		std::string ipAddress;

		/// Serializes this player as JSON.
		json Serialize() const;
	};

	struct BhopStats
	{
		u16 bhops = 0;
		u16 perfs = 0;

		/// Serializes these stats as JSON.
		json Serialize() const;
	};

	class CourseSession
	{
	public:
		/// Updates the playtime for the given mode.
		void UpdatePlaytime(float additionalPlaytime, Mode mode);

		/// Serializes this session as JSON.
		json Serialize() const;

	private:
		struct Data
		{
			float playtime;
			u16 startedRuns;
			u16 finishedRuns;
			BhopStats bhopStats {};

			Data() : playtime(0.0f), startedRuns(0), finishedRuns(0) {}

			/// Serializes this data as JSON.
			json Serialize() const;
		};

		Data vanilla {};
		Data classic {};
	};

	class Session
	{
	public:
		// HACK: this ctor is called before the clock initializes, so we can't set a correct `latestTimestamp` right away.
		Session() : secondsActive(0.0f), secondsAFK(0.0f), secondsSpectating(0.0f), latestTimestamp(0) {}

		/// Constructs a new session with a custom starting point in time.
		///
		/// This should only be called when a player joins!
		Session(float timestamp) : secondsActive(0.0f), secondsAFK(0.0f), secondsSpectating(0.0f), latestTimestamp(timestamp) {}

		/// Switches the player's state to "active" and updates playtime.
		///
		/// # Return
		///
		/// Returns the delta between now and the last time we updated something.
		float GoActive();

		/// Switches the player's state to "AFK" and updates playtime.
		///
		/// # Return
		///
		/// Returns the delta between now and the last time we updated something.
		float GoAFK();

		/// Switches the player's state to "spectating" and updates playtime.
		///
		/// # Return
		///
		/// Returns the delta between now and the last time we updated something.
		float GoSpectating();

		/// Updates `latestTimestamp` and returns the delta between now and the previous `latestTimestamp`.
		float UpdateTime();

		/// Switches the current course to the given `courseID` and updates stats for the given mode.
		///
		/// # Return
		///
		/// Returns the delta between now and the last time we updated something.
		float SwitchCourse(u16 courseID, Mode currentMode);

		/// Registers that the player jumped.
		void Jump(bool perf);

		/// Serializes this session as JSON.
		json Serialize() const;

	private:
		enum class State
		{
			ACTIVE,
			AFK,
			SPECTATING,
		};

		float secondsActive;
		float secondsAFK;
		float secondsSpectating;
		float latestTimestamp;
		State currentState = State::ACTIVE;
		BhopStats bhopStats {};
		std::optional<u16> currentCourse {};
		std::unordered_map<u16, CourseSession> courseSessions {};
	};

	struct PlayerUpdate
	{
		/// The player's name.
		std::string name;

		/// The player's IP address.
		std::string ipAddress;

		/// The player's current preferences.
		json preferences;

		/// The player's session data.
		Session session;

		/// Serializes this update as JSON.
		json Serialize() const;
	};
} // namespace KZ::API
