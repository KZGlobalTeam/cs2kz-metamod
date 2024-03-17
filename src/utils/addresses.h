#pragma once
#include "common.h"
#include "utils/module.h"

namespace modules
{
	inline CModule *engine;
	inline CModule *tier0;
	inline CModule *server;
	inline CModule *schemasystem;
	inline CModule *steamnetworkingsockets;

	inline void Initialize()
	{
		modules::engine = new CModule(ROOTBIN, "engine2");
		modules::tier0 = new CModule(ROOTBIN, "tier0");
		modules::server = new CModule(GAMEBIN, "server");
		modules::schemasystem = new CModule(ROOTBIN, "schemasystem");
		modules::steamnetworkingsockets = new CModule(ROOTBIN, "steamnetworkingsockets");
	}

} // namespace modules
