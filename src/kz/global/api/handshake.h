#pragma once

#include <unordered_map>

#include "version.h"
#include "common.h"
#include "utils/json.h"

#include "kz/global/api/players.h"
#include "kz/global/api/maps.h"

namespace KZ::API::handshake
{
	class Hello
	{
	public:
		Hello(const std::string &currentMapName, std::string_view pluginChecksum) : currentMapName(currentMapName), pluginChecksum(pluginChecksum) {}

		void AddPlayer(u64 steamId, std::string name)
		{
			this->players[steamId] = {steamId, name};
		}

		bool ToJson(Json &json) const
		{
			json.Set("id", (u64)0);
			json.Set("plugin_version", VERSION_STRING);
			json.Set("plugin_version_checksum", this->pluginChecksum);
			json.Set("map", this->currentMapName);
			json.Set("players", this->players);

			return true;
		}

	private:
		const std::string currentMapName;
		const std::string_view pluginChecksum;
		std::unordered_map<u64, KZ::API::PlayerInfo> players {};
	};

	struct HelloAck
	{
		f64 heartbeatInterval = -1.0;
		std::optional<KZ::API::Map> map = std::nullopt;

		bool FromJson(const Json &json)
		{
			if (!json.Get("heartbeat_interval", this->heartbeatInterval))
			{
				return false;
			}

			if (!json.Get("map", this->map))
			{
				return false;
			}

			if (!this->map.has_value())
			{
				META_CONPRINTF("[KZ::Global] Current map is not global.\n");
			}

			return true;
		}
	};
} // namespace KZ::API::handshake
