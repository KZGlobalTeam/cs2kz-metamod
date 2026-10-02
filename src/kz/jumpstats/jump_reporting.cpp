#include "kz_jumpstats.h"
#include "kz/anticheat/kz_anticheat.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/option/kz_option.h"
#include "kz/language/kz_language.h"
#include "kz/hud/kz_hud.h"

#include "utils/utils.h"
#include "utils/simplecmds.h"
#include "utils/tables.h"

static_global const char *columnKeys[] = {"#.",
										  "Sync",
										  "Gain",
										  "",
										  "Loss",
										  "",
										  "Max",
										  "Air Time",
										  "Bad Angles (Short)",
										  "Overlap (Short)",
										  "Dead Air (Short)",
										  "Width (Short)",
										  "Average Gain (Short)",
										  "Gain Efficiency (Short)",
										  "Angle Ratio"};

// clang-format off
extern const JSFieldDef JS_FIELDS[(i32)JSField::Count] =
{
	{"block",     "jsShowBlock",     "Menu - JS Field Block",           NULL,                      {"Block"},                                                         true},
	{"strafes",   "jsShowStrafes",   "Menu - JS Field Strafes",         NULL,                      {"Strafes"},                                                       false},
	{"sync",      "jsShowSync",      "Menu - JS Field Sync",            NULL,                      {"Sync"},                                                          false},
	{"premax",    "jsShowPreMax",    "Menu - JS Field Pre Max",         NULL,                      {"Pre", "Max"},                                                    false},
	{"edge",      "jsShowEdge",      "Menu - JS Field Edge",            NULL,                      {"Edge"},                                                          false},
	{"height",    "jsShowHeight",    "Menu - JS Field Height",          NULL,                      {"Height"},                                                        true},
	{"airtime",   "jsShowAirTime",   "Menu - JS Field Air Time",        NULL,                      {"Air Time"},                                                      true},
	{"width",     "jsShowWidth",     "Menu - JS Field Width",           NULL,                      {"Width"},                                                         true},
	{"gaineff",   "jsShowGainEff",   "Menu - JS Field Gain Efficiency", NULL,                      {"Gain Efficiency (Short)"},                                       true},
	{"airpath",   "jsShowAirPath",   "Menu - JS Field Air Path",        NULL,                      {"Air Path"},                                                      true},
	{"badangles", "jsShowBadAngles", "Menu - JS Field Bad Angles",      NULL,                      {"Bad Angles (Short)", "Overlap (Short)", "Dead Air (Short)"},     true},
	{"deviation", "jsShowDeviation", "Menu - JS Field Deviation",       "Deviation",               {"Deviation (Short)"},                                             true},
	{"miss",      "jsShowMiss",      "Menu - JS Field Miss",            NULL,                      {"Miss"},                                                          true},
	{"release",   "jsShowRelease",   "Menu - JS Field Release",         "Jumpstats HUD - Release", {"Release (Short)"},                                               false},
	{"offset",    "jsShowOffset",    "Menu - JS Field Offset",          NULL,                      {"Offset"},                                                        true},
};
// clang-format on

void KZJumpstatsService::GetFieldLayout(KZPlayer *player, JSFieldLayout &out)
{
	auto *opts = player->optionService;
	bool placed[(i32)JSField::Count] {};
	i32 count = 0;
	// Unknown names are skipped, and fields missing from the list go at the end.
	CSplitString slugs(opts->GetPreferenceStr("jsFieldOrder", JS_DEFAULT_FIELD_ORDER), ",");
	FOR_EACH_VEC(slugs, i)
	{
		for (i32 f = 0; f < (i32)JSField::Count; f++)
		{
			if (!placed[f] && KZ_STREQ(slugs[i], JS_FIELDS[f].slug))
			{
				placed[f] = true;
				out.order[count++] = (JSField)f;
				break;
			}
		}
	}
	for (i32 f = 0; f < (i32)JSField::Count; f++)
	{
		if (!placed[f])
		{
			out.order[count++] = (JSField)f;
		}
		out.shown[f] = opts->GetPreferenceBool(JS_FIELDS[f].prefKey, true);
	}
}

void KZJumpstatsService::SetFieldOrder(KZPlayer *player, const JSField (&order)[(i32)JSField::Count])
{
	std::string value;
	for (JSField field : order)
	{
		value += value.empty() ? JS_FIELDS[(i32)field].slug : std::string(",") + JS_FIELDS[(i32)field].slug;
	}
	player->optionService->SetPreferenceStr("jsFieldOrder", value.c_str());
}

static_function void AddFieldValue(JSFieldText &out, const char *format, ...)
{
	char buffer[64];
	va_list args;
	va_start(args, format);
	V_vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	out.values[out.count++] = buffer;
}

void Jump::FormatField(JSField field, bool chat, bool greyTier, JSFieldText &out)
{
	out = JSFieldText();
	out.present = true;
	switch (field)
	{
		case JSField::Block:
		{
			// Colored by the block's own tier, the way the distance is by the distance's.
			out.present = this->GetBlock() > 0.0f;
			out.color = JSFieldColor::Tier;
			out.tier = greyTier ? DistanceTier_Meh : this->GetJumpPlayer()->modeService->GetDistanceTier(this->GetReportJumpType(), this->GetBlock());
			AddFieldValue(out, "%.0f", this->GetBlock());
			break;
		}
		case JSField::Strafes:
		{
			AddFieldValue(out, "%i", this->GetStrafeCount());
			break;
		}
		case JSField::Sync:
		{
			AddFieldValue(out, "%.0f%%", this->GetSync() * 100.0f);
			break;
		}
		case JSField::PreMax:
		{
			AddFieldValue(out, chat ? "%.0f" : "%.1f", this->GetTakeoffSpeed());
			AddFieldValue(out, chat ? "%.0f" : "%.1f", this->GetMaxSpeed());
			break;
		}
		case JSField::Edge:
		{
			out.present = this->GetEdge(false) >= 0.0f;
			AddFieldValue(out, "%.2f", this->GetEdge(false));
			break;
		}
		case JSField::Height:
		{
			AddFieldValue(out, chat ? "%.2f" : "%.1f", this->GetMaxHeight());
			break;
		}
		case JSField::AirTime:
		{
			KZPlayer *jumper = this->GetJumpPlayer();
			AddFieldValue(out, "%.3fs", jumper->landingTimeActual - jumper->takeoffTime);
			break;
		}
		case JSField::Width:
		{
			AddFieldValue(out, "%.1f°", this->GetWidth());
			break;
		}
		case JSField::GainEfficiency:
		{
			AddFieldValue(out, "%.0f%%", this->GetGainEfficiency() * 100.0f);
			break;
		}
		case JSField::AirPath:
		{
			AddFieldValue(out, "%.2f", this->GetAirPath());
			break;
		}
		case JSField::BadAngles:
		{
			AddFieldValue(out, "%.0f%%", this->GetBadAngles() * 100.0f);
			AddFieldValue(out, "%.0f%%", this->GetOverlap() * 100.0f);
			AddFieldValue(out, "%.0f%%", this->GetDeadAir() * 100.0f);
			break;
		}
		case JSField::Deviation:
		{
			out.present = this->GetBlock() > 0.0f;
			// Which side of the jump direction the landing is on. Nothing is added at zero.
			const f32 deviation = this->GetSignedDeviation();
			const f32 rounded = roundf(fabsf(deviation) * 10.0f) / 10.0f;
			AddFieldValue(out, "%.1f%s", rounded, rounded == 0.0f ? "" : deviation > 0.0f ? " L" : " R");
			break;
		}
		case JSField::Miss:
		{
			out.present = this->GetMiss() > 0.0f;
			AddFieldValue(out, "%.2f", this->GetMiss());
			break;
		}
		case JSField::Release:
		{
			// Same cases and colors as GetReleaseString. Where it does not apply, the chat leaves it out and the HUD greys it.
			const JumpType type = this->GetReportJumpType();
			const f32 release = this->GetReleaseInTick();
			if ((type != JumpType_LongJump && type != JumpType_LadderJump && type != JumpType_WeirdJump) || release < -20)
			{
				out.present = !chat;
				out.color = JSFieldColor::Grey;
				AddFieldValue(out, "-");
			}
			else if (release > 10)
			{
				out.color = JSFieldColor::Red;
				AddFieldValue(out, "✗");
			}
			else if (release > 0)
			{
				out.color = JSFieldColor::Red;
				AddFieldValue(out, "+%.1f", release);
			}
			else if (release == 0)
			{
				out.color = JSFieldColor::Green;
				AddFieldValue(out, "✓");
			}
			else
			{
				out.color = JSFieldColor::Blue;
				AddFieldValue(out, "%.1f", release);
			}
			break;
		}
		case JSField::Offset:
		{
			AddFieldValue(out, "%+.2f", this->GetOffset());
			break;
		}
		default:
			out.present = false;
			break;
	}
}

// Indexed by JSFieldColor. Tier takes the tier's own color instead.
static_global const char *const JS_CHAT_COLORS[] = {"{olive}", "{grey}", "{red}", "{green}", "{blue}", "{grey}"};

// Leaves room for the chat prefix and for the color codes to expand, inside the 512 bytes a chat message gets.
#define JS_CHAT_LINE_MAX 380

std::string Jump::GetInvalidationReasonString(const char *reason, const char *language)
{
	if (!reason || reason[0] == '\0')
	{
		return "";
	}
	const char *lang = language ? language : this->GetJumpPlayer()->languageService->GetLanguage();
	std::string reasonText = KZLanguageService::PrepareMessageWithLang(lang, reason);
	return std::string(KZLanguageService::PrepareMessageWithLang(lang, "Jumpstats Report - Invalidation Reason", reasonText.c_str()));
}

void KZJumpstatsService::PrintJumpToChat(KZPlayer *target, Jump *jump, bool extended)
{
	if (!target || !target->IsInGame())
	{
		return;
	}
	const char *language = target->languageService->GetLanguage();
	JumpType reportType = jump->GetReportJumpType();
	DistanceTier color = jump->GetJumpPlayer()->modeService->GetDistanceTier(reportType, jump->GetDistance());
	bool jsAlways = target->optionService->GetPreferenceBool("jsAlways", false);
	bool isFailstat = jump->IsFailstat();
	if (isFailstat && !target->optionService->GetPreferenceBool("jsFailstats", true))
	{
		return;
	}
	// The HUD panel and the chat each have their own minimum tier.
	auto meetsMinTier = [&](const char *prefKey, const char *defaultKey)
	{
		const DistanceTier minTier = static_cast<DistanceTier>(
			target->optionService->GetPreferenceInt(prefKey, KZOptionService::GetOptionInt(defaultKey, DistanceTier_Impressive)));
		return jsAlways || (minTier != DistanceTier_None && color >= minTier);
	};
	const bool chatTier = meetsMinTier("jsMinTier", "defaultJSMinTier");
	const bool hudTier = meetsMinTier("jsMinTierHud", "defaultJSMinTierHud");
	if (!chatTier && !hudTier)
	{
		return;
	}
	if (isFailstat || jump->GetOffset() <= -JS_EPSILON || !jump->IsValid() || jsAlways)
	{
		color = DistanceTier_Meh;
	}
	// type "HUD" wont show the chat ones, "Both" will show both :aga:
	const i64 reportTo = target->optionService->GetPreferenceInt("jsReportType", JSReportType_Hud);
	bool toChat = reportTo != JSReportType_Hud;
	// A jump the HUD panel cannot show goes to chat instead.
	if (reportTo != JSReportType_Chat && hudTier && !target->hudService->ShowJumpstat(jump, color))
	{
		toChat = true;
	}
	if (!toChat || !chatTier)
	{
		return;
	}
	const char *jumpColor = distanceTierColors[color];

	std::string jumpTypeShort = jumpTypeShortStr[reportType];
	if (isFailstat)
	{
		jumpTypeShort += "-F";
	}

	// Basic fields go on the first line and extended ones on a second, in the player's order.
	std::vector<std::string> lines[2];
	lines[0].push_back(KZLanguageService::PrepareMessageWithLang(language, "Jumpstats Report - Chat Header", jumpColor, jumpTypeShort.c_str(),
																 jump->GetDistance(true, false, 1)));
	const JSFieldLayout &fields = target->hudService->GetOwnPrefs().jsFields;
	for (JSField field : fields.order)
	{
		const JSFieldDef &def = JS_FIELDS[(i32)field];
		if (!fields.shown[(i32)field] || (def.extended && !extended))
		{
			continue;
		}
		JSFieldText text;
		jump->FormatField(field, true, color <= DistanceTier_Meh, text);
		if (!text.present)
		{
			continue;
		}
		const char *valueColor = JS_CHAT_COLORS[(i32)text.color];
		if (text.color == JSFieldColor::Tier)
		{
			valueColor = distanceTierColors[text.tier];
		}
		for (i32 i = 0; i < text.count; i++)
		{
			const char *labelKey = def.labels[i];
			if (field == JSField::Strafes && jump->GetStrafeCount() <= 1)
			{
				labelKey = "Strafe";
			}
			const std::string label = KZLanguageService::PrepareMessageWithLang(language, labelKey);
			const std::string segment = KZLanguageService::PrepareMessageWithLang(language, "Jumpstats Report - Chat Segment", valueColor,
																				  text.values[i].c_str(), label.c_str());
			// Chat messages have a fixed size, so a line that would outgrow one carries on in another.
			std::vector<std::string> &group = lines[def.extended ? 1 : 0];
			if (group.empty() || group.back().size() + segment.size() > JS_CHAT_LINE_MAX)
			{
				group.push_back(segment);
			}
			else
			{
				group.back() += " {grey}| " + segment;
			}
		}
	}

	// Only the first line gets the chat prefix, as the others carry on from it.
	bool first = true;
	for (const std::vector<std::string> &group : lines)
	{
		for (const std::string &line : group)
		{
			target->PrintChat(first, false, "%s", line.c_str());
			first = false;
		}
	}
}

void KZJumpstatsService::PrintJumpToConsole(KZPlayer *target, Jump *jump, bool broadcast)
{
	if (!target || !target->IsInGame())
	{
		return;
	}
	if (jump->IsFailstat() && !target->optionService->GetPreferenceBool("jsFailstatsConsole", true))
	{
		return;
	}
	if (broadcast && (jump->IsFailstat() || jump->GetOffset() <= -JS_EPSILON))
	{
		return;
	}

	JumpType reportType = jump->GetReportJumpType();
	DistanceTier color = jump->GetJumpPlayer()->modeService->GetDistanceTier(reportType, jump->GetDistance());
	DistanceTier minTier = static_cast<DistanceTier>(
		broadcast ? target->optionService->GetPreferenceInt("jsBroadcastMinTierConsole",
															KZOptionService::GetOptionInt("defaultJSBroadcastMinTierConsole", DistanceTier_Ownage))
				  : target->optionService->GetPreferenceInt("jsMinTierConsole",
															KZOptionService::GetOptionInt("defaultJSMinTierConsole", DistanceTier_Impressive)));
	// We only print if the jump meets one of these requirements:
	// - The jump tier is equal or higher than the minimum tier set in preferences
	// - The "jsAlways" option is enabled and this is not a broadcasted jump
	// - The target is a CSTV (HLTV) client
	bool shouldPrint = minTier != DistanceTier_None && color >= minTier;
	shouldPrint |= !broadcast && target->optionService->GetPreferenceBool("jsAlways", false);
	shouldPrint |= target->IsCSTV();
	if (broadcast && jump->GetJumpPlayer()->anticheatService->isBanned)
	{
		shouldPrint = false;
	}
	if (!shouldPrint)
	{
		return;
	}
	const char *language = target->languageService->GetLanguage();
	std::string jumpType = jumpTypeStr[reportType];
	const char *summaryPhrase = jump->IsFailstat() ? "Jumpstats Report - Console Failstat Summary" : "Jumpstats Report - Console Summary";
	// clang-format off
	target->languageService->PrintConsole(false, false, summaryPhrase,
		jump->GetJumpPlayer()->GetName(),
		jump->GetDistance(),
		jumpType.c_str(),
		jump->GetInvalidationReasonString(jump->invalidateReason)
	);
	// clang-format on
	std::string modeStyleNames = jump->GetJumpPlayer()->modeService->GetModeShortName();
	FOR_EACH_VEC(jump->GetJumpPlayer()->styleServices, i)
	{
		modeStyleNames += " +";
		modeStyleNames += jump->GetJumpPlayer()->styleServices[i]->GetStyleShortName();
	}
	std::string releaseString = "";
	if (reportType == JumpType_LongJump || reportType == JumpType_LadderJump || reportType == JumpType_WeirdJump)
	{
		releaseString = jump->GetReleaseString(false);
	}
	// Build inline stats for console
	std::string edgeString = "";
	std::string landingEdgeString = "";
	std::string blockString = "";
	std::string missString = "";
	if (jump->GetEdge(false) >= 0.0f)
	{
		edgeString = KZLanguageService::PrepareMessageWithLang(language, "Jumpstat Report - Console Segment - Edge", jump->GetEdge(false));
	}
	if (jump->GetEdge(true) > 0.0f)
	{
		landingEdgeString =
			KZLanguageService::PrepareMessageWithLang(language, "Jumpstat Report - Console Segment - Landing Edge", jump->GetEdge(true));
	}
	if (jump->GetBlock() > 0.0f)
	{
		blockString = KZLanguageService::PrepareMessageWithLang(language, "Jumpstat Report - Console Segment - Block", jump->GetBlock());
	}
	if (jump->GetMiss() > 0.0f)
	{
		missString = KZLanguageService::PrepareMessageWithLang(language, "Jumpstat Report - Console Segment - Miss", jump->GetMiss());
	}

	// clang-format off
	target->languageService->PrintConsole(false, false, "Jumpstat Report - Console Details 1",
		modeStyleNames.c_str(),
		blockString.c_str(),
		edgeString.c_str(),
		landingEdgeString.c_str(),
		missString.c_str(),
		jump->GetStrafeCount(),
		KZLanguageService::PrepareMessageWithLang(language, jump->GetStrafeCount() > 1 ? "Strafes" : "Strafe").c_str(),
		jump->GetSync() * 100.0f,
		jump->GetTakeoffSpeed(),
		jump->GetMaxSpeed(),
		jump->GetBadAngles() * 100.0f,
		jump->GetOverlap() * 100.0f,
		jump->GetDeadAir() * 100.0f,
		jump->GetMaxHeight(),
		releaseString.c_str()
	);

	target->languageService->PrintConsole(false, false, "Jumpstat Report - Console Details 2",
		jump->GetGainEfficiency() * 100.0f,
		jump->GetAirPath(),
		jump->GetDeviation(),
		jump->GetWidth(),
		jump->GetJumpPlayer()->landingTimeActual - jump->GetJumpPlayer()->takeoffTime,
		jump->GetOffset(),
		jump->GetDuckTime(true),
		jump->GetDuckTime(false)
	);
	
	CUtlString headers[KZ_ARRAYSIZE(columnKeys)];
	for (u32 i = 0; i < KZ_ARRAYSIZE(columnKeys); i++)
	{
		headers[i] = target->languageService->PrepareMessage(columnKeys[i]).c_str();
	}
	utils::Table<KZ_ARRAYSIZE(columnKeys)> table("", headers);
	
	FOR_EACH_VEC(jump->strafes, i)
	{
		char strafeNumberString[5];
		char syncString[16], gainString[16], lossString[16], externalGainString[16], externalLossString[16], maxString[16], durationString[16];
		char badAngleString[16], overlapString[16], deadAirString[16], widthString[16], avgGainString[16], gainEffString[16];
		char angRatioString[32];
		V_snprintf(strafeNumberString, sizeof(strafeNumberString), "%i.", i+1);
		V_snprintf(syncString, sizeof(syncString), "%.0f%%%%", jump->strafes[i].GetSync() * 100.0f);
		V_snprintf(gainString, sizeof(gainString), "%.2f", jump->strafes[i].GetGain());
		V_snprintf(externalGainString, sizeof(externalGainString), "(+%.2f)", fabs(jump->strafes[i].GetGain(true)));
		V_snprintf(lossString, sizeof(lossString), "-%.2f", fabs(jump->strafes[i].GetLoss()));
		V_snprintf(externalLossString, sizeof(externalLossString), "(-%.2f)", fabs(jump->strafes[i].GetLoss(true)));
		V_snprintf(maxString, sizeof(maxString), "%.2f", jump->strafes[i].GetStrafeMaxSpeed());
		V_snprintf(durationString, sizeof(durationString), "%.3f", jump->strafes[i].GetStrafeDuration());
		V_snprintf(badAngleString, sizeof(badAngleString), "%.1f", jump->strafes[i].GetBadAngleDuration() * ENGINE_FIXED_TICK_RATE);
		V_snprintf(overlapString, sizeof(overlapString), "%.1f", jump->strafes[i].GetOverlapDuration() * ENGINE_FIXED_TICK_RATE);
		V_snprintf(deadAirString, sizeof(deadAirString), "%.1f", jump->strafes[i].GetDeadAirDuration() * ENGINE_FIXED_TICK_RATE);
		V_snprintf(widthString, sizeof(widthString), "%.1f", fabsf(jump->strafes[i].GetWidth()));
		V_snprintf(avgGainString, sizeof(avgGainString), "%.2f", jump->strafes[i].GetGain() / jump->strafes[i].GetStrafeDuration() * ENGINE_FIXED_TICK_INTERVAL);
		V_snprintf(gainEffString, sizeof(gainEffString), "%.0f%%%%", jump->strafes[i].GetGain() / jump->strafes[i].GetMaxGain() * 100.0f);

		if (jump->strafes[i].arStats.available)
		{
			V_snprintf(angRatioString, sizeof(angRatioString),
				"%.2f/%.2f/%.2f",
				jump->strafes[i].arStats.average,
				jump->strafes[i].arStats.median,
				jump->strafes[i].arStats.max
			);
		}
		else
		{
			V_snprintf(angRatioString, sizeof(angRatioString), "N/A");
		}

		table.SetRow(i,
			strafeNumberString,
			syncString,
			gainString,
			externalGainString,
			lossString,
			externalLossString,
			maxString,
			durationString,
			badAngleString,
			overlapString,
			deadAirString,
			widthString,
			avgGainString,
			gainEffString,
			angRatioString
		);
	}
	// clang-format on

	if (jump->strafes.Count() > 0)
	{
		target->PrintConsole(false, false, table.GetHeader());
		for (u32 i = 0; i < table.GetNumEntries(); i++)
		{
			target->PrintConsole(false, false, table.GetLine(i));
		}
	}

	if (!broadcast)
	{
		std::string strafeLeft, strafeRight, mouseLeft, mouseRight;
		if (jump->BuildConsoleStrafeMouseGraph(strafeLeft, strafeRight, mouseLeft, mouseRight))
		{
			target->languageService->PrintConsole(false, false, "Jumpstat Report - Console Graph - Strafe Keys");
			target->languageService->PrintConsole(false, false, "Jumpstat Report - Console Graph - Left", strafeLeft.c_str());
			target->languageService->PrintConsole(false, false, "Jumpstat Report - Console Graph - Right", strafeRight.c_str());
			target->languageService->PrintConsole(false, false, "Jumpstat Report - Console Graph - Mouse Movement");
			target->languageService->PrintConsole(false, false, "Jumpstat Report - Console Graph - Left", mouseLeft.c_str());
			target->languageService->PrintConsole(false, false, "Jumpstat Report - Console Graph - Right", mouseRight.c_str());
		}
	}
}

void KZJumpstatsService::BroadcastJumpToChat(KZPlayer *target, Jump *jump)
{
	if (!target || !target->IsInGame())
	{
		return;
	}

	if (jump->GetJumpPlayer()->anticheatService->isBanned)
	{
		return;
	}
	if (!jump->IsValid())
	{
		return;
	}
	DistanceTier tier = jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->GetReportJumpType(), jump->GetDistance());
	if (tier == DistanceTier_None)
	{
		return;
	}
	const char *jumpColor = distanceTierColors[tier];

	DistanceTier broadcastTier = static_cast<DistanceTier>(target->optionService->GetPreferenceInt(
		"jsBroadcastMinTier", KZOptionService::GetOptionInt("defaultJSBroadcastMinTier", DistanceTier_Ownage)));
	bool broadcastEnabled = broadcastTier != DistanceTier_None;
	bool validBroadcastTier = tier >= broadcastTier;
	if (broadcastEnabled && validBroadcastTier)
	{
		// clang-format off
		target->languageService->PrintChat(true, false, "Broadcast Jumpstat Chat Report", 
			jump->GetJumpPlayer()->GetName(), 
			jumpColor,
			jump->GetDistance(),
			jumpTypeStr[jump->GetReportJumpType()],
			jump->GetJumpPlayer()->modeService->GetModeName()
		);
		// clang-format on
	}
}

void KZJumpstatsService::PlayJumpstatSound(KZPlayer *target, Jump *jump, bool broadcast)
{
	if (jump->IsFailstat())
	{
		return;
	}
	if (broadcast && jump->GetOffset() <= -JS_EPSILON)
	{
		return;
	}
	DistanceTier tier = jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->GetReportJumpType(), jump->GetDistance());
	DistanceTier soundMinTier =
		broadcast ? static_cast<DistanceTier>(target->optionService->GetPreferenceInt(
						"jsBroadcastSoundMinTier", KZOptionService::GetOptionInt("defaultJSBroadcastSoundMinTier", DistanceTier_Ownage)))
				  : static_cast<DistanceTier>(target->optionService->GetPreferenceInt(
						"jsSoundMinTier", KZOptionService::GetOptionInt("defaultJSSoundMinTier", DistanceTier_Impressive)));
	// We only print if the jump meets one of these requirements:
	// - The jump tier is equal or higher than the minimum tier set in preferences
	// - The "jsAlways" option is not enabled
	// - The target is a CSTV (HLTV) client
	bool shouldPlay = soundMinTier != DistanceTier_None && tier >= soundMinTier && tier > DistanceTier_Meh;
	shouldPlay |= target->IsCSTV();

	if (broadcast && jump->GetJumpPlayer()->anticheatService->isBanned)
	{
		shouldPlay = false;
	}
	if (!shouldPlay || target->optionService->GetPreferenceBool("jsAlways", false))
	{
		return;
	}
	utils::PlaySoundToClient(target->GetPlayerSlot(), distanceTierSounds[tier], target->optionService->GetPreferenceFloat("jsVolume", 0.75f));
}

void KZJumpstatsService::AnnounceJump(Jump *jump)
{
	if (jump->GetJumpType() == JumpType_FullInvalid)
	{
		return;
	}
	bool isFailstat = jump->IsFailstat();
	for (i32 i = 1; i <= MAXPLAYERS; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (!player || !player->IsInGame())
		{
			continue;
		}
		// If the player is the one who did the jump or is spectating the jumper, we show more details in chat.
		if (player == jump->GetJumpPlayer() || player->specService->GetSpectatedPlayer() == jump->GetJumpPlayer())
		{
			if ((jump->GetOffset() <= -JS_EPSILON || !jump->IsValid()) && !isFailstat && !player->optionService->GetPreferenceBool("jsAlways", false))
			{
				continue;
			}
			if (!player->optionService->GetPreferenceBool("jsReporting", true))
			{
				continue;
			}
			if (!isFailstat || player->optionService->GetPreferenceBool("jsFailstats", true))
			{
				KZJumpstatsService::PrintJumpToChat(player, jump, player->optionService->GetPreferenceBool("jsExtendedChatStats", false));
			}
			KZJumpstatsService::PrintJumpToConsole(player, jump);
			KZJumpstatsService::PlayJumpstatSound(player, jump);
		}
		// If the player is a CSTV bot, we only display console output.
		else if (player->IsCSTV())
		{
			// CSTV bots see all jumps.
			KZJumpstatsService::PrintJumpToConsole(player, jump);
		}
		// If the player has broadcasting enabled, we show a brief summary in chat and play a sound.
		else
		{
			if (!jump->IsValid() || jump->IsFailstat() || jump->GetOffset() <= -JS_EPSILON)
			{
				continue;
			}
			// Banned players' jumps are never broadcast.
			if (jump->GetJumpPlayer()->anticheatService->isBanned)
			{
				continue;
			}
			if (!player->optionService->GetPreferenceBool("jsReporting", true))
			{
				continue;
			}
			KZJumpstatsService::BroadcastJumpToChat(player, jump);
			KZJumpstatsService::PrintJumpToConsole(player, jump, true);
			KZJumpstatsService::PlayJumpstatSound(player, jump, true);
		}
	}
}
