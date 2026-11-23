#include "kz_profile.h"
#include "cs2kz.h"
#include "utils/http.h"
#include "kz/anticheat/kz_anticheat.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/option/kz_option.h"
#include "kz/global/kz_global.h"
#include "kz/language/kz_language.h"
#include "kz/profile/hud/profile.h"
#include "utils/simplecmds.h"

#include "sdk/recipientfilters.h"
#include "public/networksystem/inetworkmessages.h"

// Rank bands by minimum rating from highest to lowest.
static_global const KZ::profile::RankBand rankBands[] = {
	{37500, "Legend", "{gold}", "rank-legend"},
	{35000, "Master", "{gold}", "rank-master"},
	{30000, "Pro", "{yellow}", "rank-pro"},
	{25000, "Semipro", "{red}", "rank-semipro"},
	{20000, "Expert", "{orchid}", "rank-expert"},
	{15000, "Skilled", "{darkblue}", "rank-skilled"},
	{10000, "Regular", "{darkblue}", "rank-regular"},
	{5000, "Casual", "{blue}", "rank-casual"},
	{0.000001, "Beginner", "{grey}", "rank-beginner"},
	{0, "New", "{grey}", "rank-new"},
};

const KZ::profile::RankBand &KZ::profile::GetRankBand(f64 rating)
{
	for (const RankBand &band : rankBands)
	{
		if (rating >= band.minRating)
		{
			return band;
		}
	}
	return rankBands[KZ_ARRAYSIZE(rankBands) - 1];
}

#define RATING_REFRESH_PERIOD 120.0f // seconds

CConVar<bool> kz_profile_rating_badge_enabled("kz_profile_rating_badge_enabled", FCVAR_NONE, "Whether to show competitive rank in scoreboard.", true);

static_function void OnClantagEnabledChanged(CConVar<bool> *ref, CSplitScreenSlot nSlot, const bool *newValue, const bool *oldValue)
{
	if (g_KZPlugin.loading || g_KZPlugin.unloading)
	{
		return;
	}
	for (KZPlayer *player : g_pKZPlayerManager->GetAllSlots())
	{
		if (player->profileService)
		{
			player->profileService->UpdateClantag();
		}
	}
}

CConVar<bool> kz_profile_clantag_enabled("kz_profile_clantag_enabled", FCVAR_NONE,
										 "Whether to set the player's clan tag (mode and rank) in the scoreboard.", true, OnClantagEnabledChanged);

void KZProfileService::OnPlayerActive()
{
	CReliableSingleUserRecipientFilter filter(this->player->GetPlayerSlot());
	NetMessageInfo_t *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("CCSUsrMsg_ServerRankRevealAll");
	CNetMessage *msg = netmsg->AllocateMessage();
	interfaces::pGameEventSystem->PostEventAbstract(0, false, &filter, netmsg, msg, 0);
	delete msg;
}

void KZProfileService::OnCheckTransmit()
{
	if (!kz_profile_rating_badge_enabled.Get())
	{
		return;
	}
	for (KZPlayer *player : g_pKZPlayerManager->GetInGamePlayers())
	{
		player->profileService->UpdateCompetitiveRank();
	}
}

void KZProfileService::RequestRating()
{
	if (!KZGlobalService::IsAvailable())
	{
		return;
	}
	if (!this->player->IsAuthenticated() || !this->player->IsConnected())
	{
		KZ_LOG_DEBUG(LogChannel::Profile, "Player %s not authenticated or not connected, cannot request rating.\n", this->player->GetName());
		return;
	}
	KZ::api::Mode mode;
	if (!KZ::api::DecodeModeString(this->player->modeService->GetModeShortName(), mode))
	{
		KZ_LOG_DEBUG(LogChannel::Profile, "Player %s has invalid mode '%s', cannot request rating.\n", this->player->GetName(),
					 this->player->modeService->GetModeShortName());
		return;
	}
	this->desiredMode = static_cast<u8>(mode);
	u64 steamID64 = this->player->GetSteamId64();
	if (steamID64 == 0)
	{
		KZ_LOG_DEBUG(LogChannel::Profile, "Player %s has invalid SteamID, cannot request rating.\n", this->player->GetName());
		return;
	}
	if (!this->player->styleServices.empty())
	{
		KZ_LOG_DEBUG(LogChannel::Profile, "Player %s has styles enabled, skipping rating request.\n", this->player->GetName());
		return;
	}
	this->timeToNextRatingRefresh = Plat_FloatTime() + RATING_REFRESH_PERIOD + RandomFloat(-30.0f, 30.0f);
	std::string apiURL = std::string(KZOptionService::GetOptionStr("apiUrl", "https://api.cs2kz.org"));
	// Removing trailing slash if present to avoid double slashes in the URL.
	if (!apiURL.empty() && apiURL.back() == '/')
	{
		apiURL.pop_back();
	}
	std::string url = apiURL + "/players/" + std::to_string(steamID64);
	HTTP::Request request(HTTP::Method::GET, url);
	KZ_LOG_DEBUG(LogChannel::Profile, "Requesting rating for player %s (%llu) in mode %d.\n", this->player->GetName(), steamID64,
				 static_cast<u8>(mode));
	auto callback = [steamID64, mode](HTTP::Response response)
	{
		KZ_LOG_DEBUG(LogChannel::Profile, "Received response for player %llu: status %d.\n", steamID64, response.status);
		if (response.status != 200)
		{
			KZ_LOG_DEBUG(LogChannel::Profile, "Non-200 response for player %llu: status %d.\n", steamID64, response.status);
			return;
		}
		KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(steamID64);
		if (player == nullptr)
		{
			KZ_LOG_DEBUG(LogChannel::Profile, "Player not found for SteamID %llu.\n", steamID64);
			return;
		}
		// The player mode has changed since the request was made.
		if (player->profileService->desiredMode != static_cast<u8>(mode))
		{
			KZ_LOG_DEBUG(LogChannel::Profile, "Player %s mode changed since request, ignoring response.\n", player->GetName());
			return;
		}
		Json json(response.Body().value_or(""));

		const char *ratingField = (mode == KZ::api::Mode::Classic) ? "ckz_rating" : "vnl_rating";
		f32 oldRating = player->profileService->currentRating;

		if (!json.Get(ratingField, player->profileService->currentRating))
		{
			KZ_LOG_DEBUG(LogChannel::Profile, "Failed to parse rating from response for player %s.\n", player->GetName());
			return;
		}
		if (oldRating <= 0.0f && player->profileService->currentRating > 0.0f)
		{
			CInGameRecipientFilter filter;
			NetMessageInfo_t *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("CCSUsrMsg_ServerRankRevealAll");
			CNetMessage *msg = netmsg->AllocateMessage();
			interfaces::pGameEventSystem->PostEventAbstract(0, false, &filter, netmsg, msg, 0);
			delete msg;
		}
		KZ_LOG_DEBUG(LogChannel::Profile, "Updating rating for player %s: %.2f.\n", player->GetName(), player->profileService->currentRating);
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
	if (!this->player->styleServices.empty())
	{
		return false;
	}
	// No rating if player is banned.
	if (this->player->anticheatService->isBanned)
	{
		return false;
	}
	KZ::api::Mode mode;
	return KZ::api::DecodeModeString(this->player->modeService->GetModeShortName(), mode);
}

void KZProfileService::SetClanTagOverride(const char *tag)
{
	V_strncpy(this->clanTagOverride, tag ? tag : "", sizeof(this->clanTagOverride));
	this->UpdateClantag();
}

void KZProfileService::UpdateClantag()
{
	if (!this->player->IsConnected()
		|| (this->player->GetController() && this->player->GetController()->m_iConnected() != PlayerConnectedState::PlayerConnected))
	{
		if (this->player->GetController() && V_strlen(this->player->GetController()->m_szClan().String()) > 0)
		{
			this->SetClantag("");
		}
		return;
	}

	if (this->clanTagOverride[0] != '\0')
	{
		this->SetClantag(this->clanTagOverride);
		return;
	}
	if (!kz_profile_clantag_enabled.Get())
	{
		if (this->player->GetController() && this->clanTag[0] != '\0' && KZ_STREQ(this->player->GetController()->m_szClan().String(), this->clanTag))
		{
			this->SetClantag("");
		}
		return;
	}
	if (this->CanDisplayRank())
	{
		// The client wraps clan tags in brackets itself.
		V_snprintf(this->clanTag, sizeof(this->clanTag), "%s %s", this->player->modeService->GetModeShortName(),
				   KZ::profile::GetRankBand(this->currentRating).name);
	}
	else
	{
		V_snprintf(this->clanTag, sizeof(this->clanTag), "%s%s", this->player->modeService->GetModeShortName(),
				   !this->player->styleServices.empty() ? "*" : "");
	}

	if (this->clanTag[0] == '\0')
	{
		return;
	}
	this->SetClantag(this->clanTag);
}

void KZProfileService::OnPhysicsSimulatePost()
{
	if (Plat_FloatTime() >= this->timeToNextRatingRefresh)
	{
		this->RequestRating();
	}

	// The server overwrites m_szClan with the player's Steam group tag once their GC persona data arrives.
	CCSPlayerController *controller = this->player->GetController();
	bool ownsClanTag = this->clanTagOverride[0] != '\0' || kz_profile_clantag_enabled.Get();
	if (ownsClanTag && this->clanTag[0] != '\0' && controller && controller->m_iConnected() == PlayerConnectedState::PlayerConnected
		&& !KZ_STREQ(controller->m_szClan().String(), this->clanTag))
	{
		this->player->SetClan(this->clanTag);
	}
}

void KZProfileService::UpdateCompetitiveRank()
{
	if (!this->player->IsInGame() || !kz_profile_rating_badge_enabled.GetBool() || !this->player->GetController())
	{
		return;
	}
	i32 rating = this->CanDisplayRank() ? static_cast<i32>(floor(this->currentRating)) : 0;
	this->player->GetController()->m_iCompetitiveRankType(11);
	this->player->GetController()->m_iCompetitiveRanking(rating);
}

std::string KZProfileService::GetPrefix(bool colors)
{
	if (this->chatPrefixOverride[0] != '\0')
	{
		return this->chatPrefixOverride;
	}
	if (this->clanTag[0] == '\0')
	{
		this->UpdateClantag();
	}
	if (this->CanDisplayRank())
	{
		const KZ::profile::RankBand &rank = KZ::profile::GetRankBand(this->currentRating);
		return std::string(colors ? rank.chatColor : "") + "[" + this->player->modeService->GetModeShortName() + " " + rank.name
			   + (colors ? "]{default}" : "]");
	}
	else
	{
		return std::string(colors ? "{default}[" : "[") + this->player->modeService->GetModeShortName()
			   + (!this->player->styleServices.empty() ? "*" : "") + (colors ? "]{default}" : "]");
	}
}

// Opens the caller's profile or the one named by the argument in a window.
SCMD(kz_profile, SCFL_PLAYER | SCFL_GLOBAL)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (!ProfileWindow::IsAvailable())
	{
		player->languageService->PrintChat(true, false, "Profile - Unavailable");
		return true;
	}
	if (!player->uiService->GetWindow<ProfileWindow>()->ShowQuery(args->ArgS()))
	{
		player->languageService->PrintChat(true, false, "Profile - Window Unavailable");
	}
	return true;
}
