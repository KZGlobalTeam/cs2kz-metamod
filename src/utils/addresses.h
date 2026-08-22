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

	inline void Cleanup()
	{
		delete modules::engine;
		delete modules::tier0;
		delete modules::server;
		delete modules::schemasystem;
		delete modules::steamnetworkingsockets;
		modules::engine = nullptr;
		modules::tier0 = nullptr;
		modules::server = nullptr;
		modules::schemasystem = nullptr;
		modules::steamnetworkingsockets = nullptr;
	}

} // namespace modules
