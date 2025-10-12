#include "kz_profile.h"
#include "utils/http.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/option/kz_option.h"

#include "sdk/recipientfilters.h"
#include "public/networksystem/inetworkmessages.h"

enum Ranks
{
	Unknown = 0,
	New,
	Beginner,
	Casual,
	Regular,
	Skilled,
	Expert,
	Semipro,
	Pro,
	Master,
	Legend,
	NUM_RANKS
};

static_global const char *rankNames[NUM_RANKS] = {
	"Unknown", "New", "Beginner", "Casual", "Regular", "Skilled", "Expert", "Semipro", "Pro", "Master", "Legend",
};

static_global const f32 rankThresholds[NUM_RANKS] = {
	-1.0f,     // Unknown
	0.0f,      // New
	0.000001f, // Beginner
	5000.0f,   // Casual
	10000.0f,  // Regular
	15000.0f,  // Skilled
	20000.0f,  // Expert
	25000.0f,  // Semipro
	30000.0f,  // Pro
	35000.0f,  // Master
	37500.0f   // Legend
};
static_global const char *rankColors[NUM_RANKS] = {"{default}", "{grey}", "{grey}",   "{blue}", "{darkblue}", "{purple}",
												   "{orchid}",  "{red}",  "{yellow}", "{gold}", "{gold}"};

#define RATING_REFRESH_PERIOD 120.0f // seconds

CConVar<bool> kz_profile_rating_badge_enabled("kz_profile_rating_badge_enabled", FCVAR_NONE, "Whether to show competitive rank in scoreboard.", true);

void KZProfileService::OnGameFrame()
{
	if (g_pKZUtils->GetServerGlobals()->tickcount % 64 != 0)
	{
		return;
	}
	CBroadcastRecipientFilter filter;
	INetworkMessageInternal *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("CCSUsrMsg_ServerRankRevealAll");
	CNetMessage *msg = netmsg->AllocateMessage();
	interfaces::pGameEventSystem->PostEventAbstract(0, false, &filter, netmsg, msg, 0);
	delete msg;
}

void KZProfileService::OnCheckTransmit()
{
	for (i32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (player && player->profileService)
		{
			player->profileService->UpdateCompetitiveRank();
		}
	}
}

void KZProfileService::RequestRating()
{
	if (!KZGlobalService::IsAvailable() || !this->player->IsAuthenticated() || !this->player->IsConnected())
	{
		return;
	}
	KZ::API::Mode mode;
	if (!KZ::API::DecodeModeString(this->player->modeService->GetModeShortName(), mode))
	{
		return;
	}
	this->desiredMode = static_cast<u8>(mode);
	u64 steamID64 = this->player->GetSteamId64();
	if (steamID64 == 0)
	{
		return;
	}
	if (this->player->styleServices.Count() > 0)
	{
		return;
	}
	this->timeToNextRatingRefresh = g_pKZUtils->GetServerGlobals()->realtime + RATING_REFRESH_PERIOD + RandomFloat(-30.0f, 30.0f);
	std::string url = std::string(KZOptionService::GetOptionStr("apiUrl", "https://api.cs2kz.org")) + "/players/" + std::to_string(steamID64)
					  + "/profile?mode=" + std::to_string(static_cast<u8>(mode));
	HTTP::Request request(HTTP::Method::GET, url);
	auto callback = [steamID64, mode](HTTP::Response response)
	{
		if (response.status != 200)
		{
			return;
		}
		KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(steamID64);
		if (player == nullptr)
		{
			return;
		}
		// The player mode has changed since the request was made.
		if (player->profileService->desiredMode != static_cast<u8>(mode))
		{
			return;
		}
		Json json(response.Body().value_or(""));

		if (!json.Get("rating", player->profileService->currentRating))
		{
			return;
		}
		player->profileService->UpdateCompetitiveRank();
		player->profileService->UpdateClantag();
	};
	request.Send(callback);
}

bool KZProfileService::CanDisplayRank()
{
	// Haven't obtained rating yet.
	if (this->currentRating < 0.0f)
	{
		return false;
	}
	// No rating if styles are enabled.
	if (this->player->styleServices.Count() > 0)
	{
		return false;
	}
	KZ::API::Mode mode;
	return KZ::API::DecodeModeString(this->player->modeService->GetModeShortName(), mode);
}

void KZProfileService::UpdateClantag()
{
	if (!this->player->IsConnected()
		|| (this->player->GetController() && this->player->GetController()->m_iConnected() != PlayerConnectedState::PlayerConnected))
	{
		if (this->player->GetController() && this->player->GetController()->m_szClan() != "")
		{
			this->SetClantag("");
		}
		return;
	}
	if (this->CanDisplayRank())
	{
		i32 rank = Ranks::Unknown;
		for (i32 i = Ranks::NUM_RANKS - 1; i >= 0; i--)
		{
			if (this->currentRating * 0.1f >= rankThresholds[i])
			{
				rank = i;
				break;
			}
		}
		V_snprintf(this->clanTag, sizeof(this->clanTag), "[%s %s]", this->player->modeService->GetModeShortName(), rankNames[rank]);
	}
	else
	{
		V_snprintf(this->clanTag, sizeof(this->clanTag), "[%s%s]", this->player->modeService->GetModeShortName(),
				   this->player->styleServices.Count() > 0 ? "*" : "");
	}

	if (this->clanTag[0] == '\0')
	{
		return;
	}
	this->SetClantag(this->clanTag);
}

void KZProfileService::OnPhysicsSimulatePost()
{
	if (g_pKZUtils->GetServerGlobals()->realtime >= this->timeToNextRatingRefresh)
	{
		this->RequestRating();
	}
}

void KZProfileService::UpdateCompetitiveRank()
{
	if (!this->player->GetController() || !kz_profile_rating_badge_enabled.GetBool())
	{
		return;
	}
	i32 rating = this->CanDisplayRank() ? static_cast<i32>(floor(this->currentRating * 0.1f)) : 0;
	this->player->GetController()->m_iCompetitiveRankType(11);
	this->player->GetController()->m_iCompetitiveRanking(rating);
}

std::string KZProfileService::GetPrefix(bool colors)
{
	if (this->clanTag[0] == '\0')
	{
		this->UpdateClantag();
	}
	if (this->CanDisplayRank())
	{
		i32 rank = Ranks::Unknown;
		for (i32 i = Ranks::NUM_RANKS - 1; i >= 0; i--)
		{
			if (this->currentRating * 0.1f >= rankThresholds[i])
			{
				rank = i;
				break;
			}
		}
		return std::string(colors ? rankColors[rank] : "") + "[" + this->player->modeService->GetModeShortName() + " " + rankNames[rank]
			   + (colors ? "]{default}" : "]");
	}
	else
	{
		return std::string(colors ? "{default}[" : "[") + this->player->modeService->GetModeShortName()
			   + (this->player->styleServices.Count() > 0 ? "*" : "") + (colors ? "]{default}" : "]");
	}
}
