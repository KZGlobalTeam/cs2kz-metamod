#pragma once

#include "public/ics2kz.h"

// Backing implementation of the public ICS2KZ interface handed out by
// KZPlugin::OnMetamodQuery. See src/public/ics2kz.h for the consumer-facing contract.
namespace KZ::pubapi
{
	// Hooks the timer event forwarder up. Must run after KZTimerService::Init.
	void Init();

	// Drops every externally registered listener and unhooks the forwarder.
	void Shutdown();

	// The singleton, already cast to the interface type so the vtable is the right one.
	ICS2KZ *GetAPI();
} // namespace KZ::pubapi
