#include "kz_jumpstats.h"
#include "kz/option/kz_option.h"
#include "utils/ctimer.h"
#include "iserver.h"
#include "filesystem.h"

static_global bool alreadyRecording = false;
static_global CTimer<> *demoTimer;

static_function f64 StopDemoRecording()
{
	interfaces::pEngine->ServerCommand("tv_stoprecord");
	alreadyRecording = false;
	return -1;
}

void KZJumpstatsService::StartDemoRecording(CUtlString playerName)
{
	if (alreadyRecording || !g_pFullFileSystem || !KZOptionService::GetOptionInt("autoDemoRecording"))
	{
		return;
	}
	g_pFullFileSystem->CreateDirHierarchy("kzdemos");
	alreadyRecording = true;

	time_t ltime;
	time(&ltime);

	CNetworkGameServerBase *networkGameServer = (CNetworkGameServerBase *)g_pNetworkServerService->GetIGameServer();
	CUtlString currentMap;
	if (networkGameServer)
	{
		currentMap = networkGameServer->GetMapName();
	}

	CUtlString command;
	playerName = playerName.Remove("\\").Remove("/");
	command.Format("tv_record kzdemos/%s_%s_%li", currentMap.Get(), playerName.Get(), ltime);
	interfaces::pEngine->ServerCommand("tv_record_immediate 1");
	interfaces::pEngine->ServerCommand(command.Get());

	demoTimer = StartTimer(StopDemoRecording, 60.0, false, true);
}

void KZJumpstatsService::OnServerActivate()
{
	alreadyRecording = false;
}
