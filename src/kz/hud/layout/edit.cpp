#include "kz/hud/layout/layout.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/tables.h"
#include "kz/option/menu/kz_menu.h"
#include "kz/language/kz_language.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "sdk/entity/ccscustomplayercamera.h"
#include "sdk/entity/cbaseplayerweapon.h"
#include "sdk/usercmd.h"
#include "utils/utils.h"

#include <vendor/mm-cs2menus/src/public/ics2menus.h>
extern ICS2Menus *g_pMenus;

#include "tier0/memdbgon.h"

// The layout is always 1080 units tall; its width follows the aspect ratio the player picked in the menu.
#define MHUD_EDIT_UNITS_PER_PCT_Y 10.8f
// Layout units the element moves per raw mouse count, about a cursor at 1080p.
#define MHUD_EDIT_UNITS_PER_COUNT 1.0f
#define MHUD_EDIT_SNAP_UNITS      20.0f
// Box width per character, in font sizes. No font we ship averages wider than this for HUD text, so the box never
// comes out smaller than the text whatever the font; characters outside ASCII (CJK phrases) get a full em.
#define MHUD_EDIT_BOX_EM_PER_CHAR  0.6f
#define MHUD_EDIT_BOX_EM_WIDE_CHAR 1.0f
#define MHUD_EDIT_BOX_EM_HEIGHT    1.3f
// Layout units of breathing room around every box.
#define MHUD_EDIT_BOX_PADDING 8.0f
// The largest box box-size.css defines. The jumpstats panel at 200% is about 970 units tall.
#define MHUD_EDIT_BOX_MAX_WIDTH  1600
#define MHUD_EDIT_BOX_MAX_HEIGHT 1000
// Re-centre pitch well before the +-89 clamp, leaving a full round trip of room for the snap to land.
#define MHUD_EDIT_RESNAP_PITCH 45.0f
#define MHUD_EDIT_CAMERA_NAME  "kz_hudedit_camera"
// The jumpstats panel at 100%, from mhud.css. The panel's fixed part is its padding, header, distance and divider.
#define MHUD_EDIT_JS_WIDTH          288.0f
#define MHUD_EDIT_JS_PANEL_HEIGHT   109.0f
#define MHUD_EDIT_JS_ROW_HEIGHT     19.0f
#define MHUD_EDIT_JS_HISTORY_HEIGHT 146.0f
// Compact mode has only the pills, with no gap under them for the panel.
#define MHUD_EDIT_JS_COMPACT_HEIGHT 138.0f
// The course panel at 100%, from mhud.css.
#define MHUD_EDIT_COURSE_WIDTH          320.0f
#define MHUD_EDIT_COURSE_PADDING        20.0f
#define MHUD_EDIT_COURSE_HEAD_HEIGHT    53.0f
#define MHUD_EDIT_COURSE_PROGRESS_GAP   6.0f
#define MHUD_EDIT_COURSE_ROW_HEIGHT     19.0f
#define MHUD_EDIT_COURSE_HISTORY_HEIGHT 146.0f

// The buttons inside each element's edit box, after its id. Corners are in EditState::corner order.
#define MHUD_EDIT_MOVE_SUFFIX "_move"
static_global const char *const EDIT_CORNER_SUFFIXES[] = {"_tl", "_tr", "_bl", "_br"};

struct EditTextDef
{
	const char *panelId;
	const char *variable;
	const char *phraseKey;
};

// The drag hint shows the keys the player has bound. Each key label in mhud.xml reads {g:csgo_key:editkey}, and the
// game's csgo_key formatter swaps every %command% in that variable for the key bound to it. The HUD's own bind_*
// variables would not work here, because they live on the CSGOHud root and custom layouts are not under it.
static_global const EditTextDef EDIT_TEXTS[] = {
	{"mhud_edit_hint", "edithint", "HUD Edit - Pick Hint"},  {"mhud_edit_place", "editplace", "HUD Edit - Place"},
	{"mhud_edit_cancel", "editcancel", "HUD Edit - Cancel"}, {"mhud_edit_default", "editdefault", "HUD Edit - Default"},
	{"mhud_edit_free", "editfree", "HUD Edit - Free Move"},  {"mhud_edit_done_label", "editdone", "HUD Edit - Done"},
};

// Key label panel, and the command whose bound key it shows.
static_global const char *const EDIT_KEYS[][2] = {
	{"mhud_edit_key_place", "%attack%"},
	{"mhud_edit_key_cancel", "%attack2%"},
	{"mhud_edit_key_default", "%reload%"},
	{"mhud_edit_key_free", "%sprint%"},
};

static_global const char *const GUIDE_PANELS[] = {"mhud_guide_x", "mhud_guide_y"};
static_global const char *const GUIDE_POS_PANELS[] = {"mhud_guide_x_pos", "mhud_guide_y_pos"};

static_function f32 NormalizeYawDelta(f32 delta)
{
	delta = fmodf(delta + 180.0f, 360.0f);
	return delta < 0.0f ? delta + 180.0f : delta - 180.0f;
}

// Uses the same rule as CInButtonState::IsButtonNewlyPressed, where any state from UP_DOWN upwards had a press.
static_function u64 GetNewlyPressedButtons(const CInButtonStatePB &buttons)
{
	return buttons.buttonstate3() | (buttons.buttonstate1() & buttons.buttonstate2());
}

static_function f32 GetClientConVarFloat(CPlayerSlot slot, const char *name, f32 fallback)
{
	const char *value = interfaces::pEngine->GetClientConVarValue(slot, name);
	const f32 result = value ? (f32)utils::StringToFloat(value) : 0.0f;
	return fabsf(result) > 0.0001f ? result : fallback;
}

// Stripping +attack on the server is not enough, because the client still predicts the shot. m_flNextAttack is
// networked and predicted, so holding it in the future stops the weapon on both ends, whichever weapon is out.
static_function void HoldWeapons(CCSPlayerPawn *pawn, bool hold, f32 &saved)
{
	CCSPlayer_WeaponServices *weapons = pawn ? static_cast<CCSPlayer_WeaponServices *>(pawn->m_pWeaponServices()) : nullptr;
	if (!weapons)
	{
		return;
	}
	const f32 now = g_pKZUtils->GetServerGlobals()->curtime;
	const f32 current = weapons->m_flNextAttack().GetTime();
	// Nothing the game sets reaches a minute ahead, so that far out means it is still our hold. A deploy after a weapon
	// switch replaces it; take that value as the one to restore and hold again.
	const bool held = current > now + 60.0f;
	if (hold && !held)
	{
		saved = current;
		weapons->m_flNextAttack(GameTime_t(now + 3600.0f));
	}
	// The hold is released half a second late. When edit mode ends on the click that places an element, that click is
	// still held for a round trip and would otherwise fire.
	else if (!hold && held)
	{
		weapons->m_flNextAttack(GameTime_t(fmaxf(saved, now + 0.5f)));
	}

	// Each weapon's own attack ticks are held the same way. The secondary one gates attack2 (burst, silencer, scope,
	// knife stab), and reloads wait on the primary one. Every carried weapon is held, so switching cannot get around it.
	const i32 tick = g_pKZUtils->GetServerGlobals()->tickcount;
	const i32 heldTicks = (i32)(3600.0f / ENGINE_FIXED_TICK_INTERVAL);
	const i32 heldThreshold = tick + (i32)(60.0f / ENGINE_FIXED_TICK_INTERVAL);
	const i32 graceTicks = (i32)(0.5f / ENGINE_FIXED_TICK_INTERVAL);
	CUtlVector<CHandle<CBasePlayerWeapon>> *carried = weapons->m_hMyWeapons();
	for (i32 i = 0; carried && i < carried->Count(); i++)
	{
		CBasePlayerWeapon *weapon = carried->Element(i).Get();
		if (!weapon)
		{
			continue;
		}
		if (hold && weapon->m_nNextPrimaryAttackTick() <= heldThreshold)
		{
			weapon->m_nNextPrimaryAttackTick(tick + heldTicks);
		}
		else if (!hold && weapon->m_nNextPrimaryAttackTick() > heldThreshold)
		{
			weapon->m_nNextPrimaryAttackTick(tick + graceTicks);
		}
		if (hold && weapon->m_nNextSecondaryAttackTick() <= heldThreshold)
		{
			weapon->m_nNextSecondaryAttackTick(tick + heldTicks);
		}
		else if (!hold && weapon->m_nNextSecondaryAttackTick() > heldThreshold)
		{
			weapon->m_nNextSecondaryAttackTick(tick + graceTicks);
		}
	}
}

static_function void RemoveEditCamera(CHandle<CBaseEntity> &handle, CHandle<CBaseEntity> &previousView)
{
	// Null on server exit.
	if (CCSCustomPlayerCamera *camera = GameEntitySystem() ? static_cast<CCSCustomPlayerCamera *>(handle.Get()) : nullptr)
	{
		CCSPlayerPawnBase *pawn = camera->m_hPawn().Get();
		camera->SetMode(CUSTOM_CAMERA_MODE_DISABLED);
		g_pKZUtils->RemoveEntity(camera);
		// Give the view back to whatever held it before the drag, such as a map camera, unless something took it since.
		CPlayer_CameraServices *cameraServices = pawn ? pawn->m_pCameraServices() : nullptr;
		CBaseEntity *previous = previousView.Get();
		if (cameraServices && previous && previous != (CBaseEntity *)pawn && !cameraServices->m_hViewEntity().Get())
		{
			cameraServices->m_hViewEntity(previousView);
		}
	}
	handle.Term();
	previousView.Term();
}

static_function void SetBoxClass(CCSCustomHudLayout *layout, const char *panelId, i32 &cache, f32 units, const char *prefix, i32 max)
{
	const i32 value = Clamp(RoundFloatToInt(units / 2.0f) * 2, 0, max);
	if (cache == value)
	{
		return;
	}
	char className[32];
	if (cache != INT_MIN)
	{
		V_snprintf(className, sizeof(className), "%s--%ipx", prefix, cache);
		layout->SetHasClass(panelId, className, k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	V_snprintf(className, sizeof(className), "%s--%ipx", prefix, value);
	layout->SetHasClass(panelId, className, k_eHudPanelClassStatus_HasClass);
	cache = value;
}

// === Entering and leaving ===========================================================

void KZHUDService::StartHudEdit(MHUDElement element)
{
	if (this->IsEditingHud())
	{
		return;
	}
	const CPlayerSlot slot = this->player->GetPlayerSlot();
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	bool created = false;
	CCSCustomHudLayout *layout = this->IsUsingLayoutStyle() && pawn && pawn->IsAlive() ? this->EnsureOwnedLayout(created) : NULL;
	if (!layout || !layout->GetPlayerLayoutState(slot))
	{
		this->player->languageService->PrintChat(true, false, "HUD Edit - Unavailable");
		return;
	}
	if (this->player->menuService->IsOpen())
	{
		this->player->menuService->Close();
	}
	if (g_pMenus)
	{
		g_pMenus->CancelMenu(slot.Get());
		g_pMenus->SetExternalBusy(slot.Get(), true);
	}
	this->edit = EditState();
	this->edit.mode = EditMode::Picking;
	this->edit.unitsPerPctX = this->GetOwnPrefs().screenWidth / 100.0f;
	layout->SetInputCaptureEnabled(slot, true);
	if (element < MHUDElement::Count && this->GetOwnPrefs().elements[(i32)element].enabled)
	{
		// Started from an element's position stepper, so after this one drag the player goes straight back there.
		this->edit.returnToMenu = true;
		this->BeginDrag(element);
	}
}

void KZHUDService::StopHudEdit()
{
	this->EndDrag(false);
	this->AbortHudEdit();
}

void KZHUDService::AbortHudEdit()
{
	if (!this->IsEditingHud())
	{
		return;
	}
	const CPlayerSlot slot = this->player->GetPlayerSlot();
	RemoveEditCamera(this->edit.camera, this->edit.previousView);
	if (GameEntitySystem())
	{
		HoldWeapons(this->player->GetPlayerPawn(), false, this->edit.nextAttack);
	}
	if (CBaseEntity *ent = GameEntitySystem() ? this->ownedLayout.Get() : nullptr)
	{
		((CCSCustomHudLayout *)ent)->SetInputCaptureEnabled(slot, false);
	}
	if (g_pMenus)
	{
		g_pMenus->SetExternalBusy(slot.Get(), false);
	}
	this->edit = EditState();
}

void KZHUDService::ValidateHudEdit()
{
	if (!this->IsEditingHud())
	{
		return;
	}
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (!pawn || !pawn->IsAlive() || !this->IsUsingLayoutStyle())
	{
		this->AbortHudEdit();
		return;
	}
	// Weapons stay held for the whole session, not just the drag, since the click that places an element is still held
	// for a round trip.
	HoldWeapons(pawn, true, this->edit.nextAttack);
	if (this->edit.mode == EditMode::Dragging)
	{
		CCSCustomPlayerCamera *camera = static_cast<CCSCustomPlayerCamera *>(this->edit.camera.Get());
		if (!camera || camera->m_hPawn().Get() != pawn)
		{
			this->AbortHudEdit();
		}
	}
}

void KZHUDService::OnCustomHudClicked(CPlayerSlot slot, CCSCustomHudLayout *layout, const char *buttonId)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (!player || !player->hudService)
	{
		return;
	}
	KZHUDService *hud = player->hudService;
	if (hud->edit.mode != EditMode::Picking || (CBaseEntity *)layout != hud->ownedLayout.Get())
	{
		return;
	}
	if (KZ_STREQ(buttonId, "mhud_edit_done"))
	{
		hud->StopHudEdit();
		return;
	}
	for (i32 i = 0; i < (i32)MHUDElement::Count; i++)
	{
		const char *hitPanelId = MHUD_ELEMENTS[i].hitPanelId;
		const size_t length = strlen(hitPanelId);
		if (strncmp(buttonId, hitPanelId, length) != 0 || !hud->GetOwnPrefs().elements[i].enabled)
		{
			continue;
		}
		const char *suffix = buttonId + length;
		if (KZ_STREQ(suffix, MHUD_EDIT_MOVE_SUFFIX))
		{
			hud->BeginDrag((MHUDElement)i);
			return;
		}
		for (i32 corner = 0; corner < KZ_ARRAYSIZE(EDIT_CORNER_SUFFIXES); corner++)
		{
			if (KZ_STREQ(suffix, EDIT_CORNER_SUFFIXES[corner]))
			{
				hud->BeginDrag((MHUDElement)i, corner);
				return;
			}
		}
	}
}

// === Dragging =======================================================================

void KZHUDService::BeginDrag(MHUDElement element, i32 corner)
{
	const CPlayerSlot slot = this->player->GetPlayerSlot();
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	CCSCustomHudLayout *layout = (CCSCustomHudLayout *)this->ownedLayout.Get();
	if (!pawn || !pawn->IsAlive() || !layout)
	{
		return;
	}
	CPlayer_CameraServices *cameraServices = pawn->m_pCameraServices();
	const CHandle<CBaseEntity> previousView = cameraServices ? cameraServices->m_hViewEntity() : CHandle<CBaseEntity>();
	CCSCustomPlayerCamera *camera = CCSCustomPlayerCamera::CreateLocked(pawn, MHUD_EDIT_CAMERA_NAME);
	if (!camera)
	{
		return;
	}
	this->edit.previousView = previousView;
	const MHUDPrefs::Element &prefs = this->GetOwnPrefs().elements[(i32)element];
	this->edit.mode = EditMode::Dragging;
	this->edit.element = element;
	this->edit.startX = this->edit.dragX = prefs.x;
	this->edit.startY = this->edit.dragY = prefs.y;
	this->edit.corner = corner;
	this->edit.startSize = this->edit.dragSize = prefs.size;
	if (corner >= 0)
	{
		this->GetEditBoxSize(element, this->edit.startWidth, this->edit.startHeight);
		f32 box[4];
		this->GetEditBox(element, prefs.x, prefs.y, box);
		this->edit.fixedX = (corner & 1) ? box[0] : box[1];
		this->edit.fixedY = (corner & 2) ? box[2] : box[3];
	}
	this->edit.guideShown[0] = this->edit.guideShown[1] = false;
	this->edit.camera = camera->GetRefEHandle();
	this->edit.viewAngles = pawn->m_angEyeAngles();
	this->edit.haveAngles = false;
	this->edit.lastCmdNum = INT_MIN;
	this->edit.yawSum = this->edit.pitchSum = 0.0f;
	this->edit.snapPending = false;

	// Negative m_yaw/m_pitch (inverted mouse) keeps its sign, so the element still follows the hand.
	const f32 sensitivity = GetClientConVarFloat(slot, "sensitivity", 1.0f);
	this->edit.yawPerCount = GetClientConVarFloat(slot, "m_yaw", 0.022f) * sensitivity;
	this->edit.pitchPerCount = GetClientConVarFloat(slot, "m_pitch", 0.022f) * sensitivity;

	this->BuildEditTargets();
	layout->SetInputCaptureEnabled(slot, false);
	this->SnapEditView(QAngle(0.0f, this->edit.viewAngles.y, 0.0f));
}

void KZHUDService::EndDrag(bool confirm)
{
	if (this->edit.mode != EditMode::Dragging)
	{
		return;
	}
	if (confirm)
	{
		const MHUDElementDef &def = MHUD_ELEMENTS[(i32)this->edit.element];
		this->player->optionService->SetPreferenceFloat(def.xKey, this->edit.dragX);
		this->player->optionService->SetPreferenceFloat(def.yKey, this->edit.dragY);
		if (this->edit.corner >= 0)
		{
			this->player->optionService->SetPreferenceFloat(def.sizeKey, this->edit.dragSize);
		}
	}
	RemoveEditCamera(this->edit.camera, this->edit.previousView);
	// Hand back the view the player had before the drag turned it.
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (pawn && pawn->IsAlive())
	{
		g_pKZUtils->SnapViewAngles(pawn, this->edit.viewAngles);
	}
	if (this->edit.returnToMenu)
	{
		this->AbortHudEdit();
		this->player->menuService->Resume();
		return;
	}
	this->edit.mode = EditMode::Picking;
	this->edit.corner = -1;
	this->edit.guideShown[0] = this->edit.guideShown[1] = false;
	if (CCSCustomHudLayout *layout = (CCSCustomHudLayout *)this->ownedLayout.Get())
	{
		layout->SetInputCaptureEnabled(this->player->GetPlayerSlot(), true);
	}
}

void KZHUDService::SnapEditView(const QAngle &angles)
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (!pawn)
	{
		return;
	}
	g_pKZUtils->SnapViewAngles(pawn, angles);
	// The stored target is quantized the way the client will receive it, so rebase on that, not on angles.
	CSchemaCollection<ViewAngleServerChange_t> changes = pawn->m_ServerViewAngleChanges();
	const i32 count = changes.Count();
	if (count > 0)
	{
		ViewAngleServerChange_t *change = changes.Element(count - 1);
		this->edit.snapIndex = change->nIndex();
		this->edit.snapAngles = change->qAngle();
		this->edit.snapPending = true;
	}
}

// Movement, buttons and weapon switches pass through untouched, because rewriting them only makes the client mispredict.
// Weapons are held through their attack times instead. While dragging, the pawn keeps the view it had before the drag,
// so spectators do not watch it spin, while the client's own angles keep moving the element.
static_function void PinEditViewAngles(PlayerCommand *cmd, const QAngle &viewAngles)
{
	CMsgQAngle *angles = cmd->mutable_base()->mutable_viewangles();
	angles->set_x(viewAngles.x);
	angles->set_y(viewAngles.y);
	angles->set_z(0.0f);
}

void KZHUDService::OnProcessUsercmds(PlayerCommand *cmds, i32 numCmds)
{
	CCSPlayerPawn *pawn = this->player->GetPlayerPawn();
	if (!this->IsEditingHud() || !pawn)
	{
		return;
	}
	for (i32 i = 0; i < numCmds; i++)
	{
		if (this->edit.mode != EditMode::Dragging)
		{
			continue;
		}
		// Copied first, because tracking can end the drag and reset the edit state on this very command.
		const QAngle viewAngles = this->edit.viewAngles;
		// A command can arrive more than once, and is only tracked the first time.
		if (cmds[i].cmdNum > this->edit.lastCmdNum)
		{
			this->TrackEditCommand(pawn, cmds[i]);
		}
		PinEditViewAngles(&cmds[i], viewAngles);
	}
}

void KZHUDService::TrackEditCommand(CCSPlayerPawn *pawn, PlayerCommand &cmd)
{
	this->edit.lastCmdNum = cmd.cmdNum;
	const CBaseUserCmdPB &base = cmd.base();
	const QAngle angles(base.viewangles().x(), base.viewangles().y(), 0.0f);

	// A snap from elsewhere (checkpoint, teleport, map) replaces ours, so rebase on whichever the client applies.
	CSchemaCollection<ViewAngleServerChange_t> changes = pawn->m_ServerViewAngleChanges();
	const i32 count = changes.Count();
	if (count > 0)
	{
		ViewAngleServerChange_t *change = changes.Element(count - 1);
		if (change->nIndex() != this->edit.snapIndex)
		{
			this->edit.snapIndex = change->nIndex();
			this->edit.snapAngles = change->qAngle();
			this->edit.snapPending = true;
		}
	}

	// Until the client applies the snap its angles continue from the old ones; from then on, from the snap.
	QAngle reference = this->edit.lastAngles;
	if (this->edit.snapPending && base.consumed_server_angle_changes() >= this->edit.snapIndex)
	{
		reference = this->edit.snapAngles;
		this->edit.snapPending = false;
	}
	if (this->edit.haveAngles)
	{
		this->edit.yawSum += NormalizeYawDelta(angles.y - reference.y);
		this->edit.pitchSum += angles.x - reference.x;
	}
	this->edit.lastAngles = angles;
	this->edit.haveAngles = true;

	this->UpdateDragPosition(GetNewlyPressedButtons(base.buttons_pb()), base.buttons_pb().buttonstate1());

	if (this->edit.mode == EditMode::Dragging && !this->edit.snapPending && fabsf(angles.x) > MHUD_EDIT_RESNAP_PITCH)
	{
		this->SnapEditView(QAngle(0.0f, angles.y, 0.0f));
	}
}

void KZHUDService::UpdateDragPosition(u64 newlyPressed, u64 held)
{
	if (this->edit.corner >= 0)
	{
		// Turning right lowers yaw, looking down raises pitch.
		const f32 unitsX = -this->edit.yawSum / this->edit.yawPerCount * MHUD_EDIT_UNITS_PER_COUNT;
		const f32 unitsY = this->edit.pitchSum / this->edit.pitchPerCount * MHUD_EDIT_UNITS_PER_COUNT;
		this->UpdateResize(newlyPressed, unitsX, unitsY);
	}
	else
	{
		this->UpdateMove(newlyPressed, held);
	}
	if (newlyPressed & IN_ATTACK)
	{
		this->EndDrag(true);
	}
	else if (newlyPressed & IN_ATTACK2)
	{
		this->EndDrag(false);
	}
}

// Dragging a corner scales the element about the opposite corner, which stays where it is.
void KZHUDService::UpdateResize(u64 newlyPressed, f32 unitsX, f32 unitsY)
{
	const MHUDElementDef &def = MHUD_ELEMENTS[(i32)this->edit.element];
	if (newlyPressed & IN_RELOAD)
	{
		this->edit.startSize = this->edit.dragSize = (f32)def.sizeDefault;
		this->GetEditBoxSize(this->edit.element, this->edit.startWidth, this->edit.startHeight);
		this->edit.yawSum = this->edit.pitchSum = 0.0f;
		unitsX = unitsY = 0.0f;
	}
	// The grabbed corner's offset from the fixed one, without the padding, which does not scale with the element. How far
	// the mouse moved the corner along that diagonal sets the new size.
	const f32 dirX = (this->edit.corner & 1) ? 1.0f : -1.0f;
	const f32 dirY = (this->edit.corner & 2) ? 1.0f : -1.0f;
	const f32 cornerX = dirX * (this->edit.startWidth - MHUD_EDIT_BOX_PADDING);
	const f32 cornerY = dirY * (this->edit.startHeight - MHUD_EDIT_BOX_PADDING);
	const f32 lengthSqr = cornerX * cornerX + cornerY * cornerY;
	const f32 scale = lengthSqr > 0.0f ? ((cornerX + unitsX) * cornerX + (cornerY + unitsY) * cornerY) / lengthSqr : 1.0f;
	this->edit.dragSize = (f32)Clamp(RoundFloatToInt(this->edit.startSize * scale), def.sizeMin, def.sizeMax);

	f32 width, height;
	this->GetEditBoxSize(this->edit.element, width, height);
	width /= this->edit.unitsPerPctX;
	height /= MHUD_EDIT_UNITS_PER_PCT_Y;
	const f32 left = dirX > 0.0f ? this->edit.fixedX : this->edit.fixedX - width;
	const f32 top = dirY > 0.0f ? this->edit.fixedY : this->edit.fixedY - height;
	const MHUDAlign align = this->GetOwnPrefs().elements[(i32)this->edit.element].align;
	f32 x = align == MHUDAlign::Left ? left : (align == MHUDAlign::Right ? left + width : left + width * 0.5f);
	f32 y = IsMHUDElementTopAnchored(this->edit.element) ? top : top + height * 0.5f;
	this->ClampEditBox(this->edit.element, x, y);
	this->edit.dragX = RoundFloatToInt(x * 10.0f) / 10.0f;
	this->edit.dragY = RoundFloatToInt(y * 10.0f) / 10.0f;
	this->edit.guideShown[0] = this->edit.guideShown[1] = false;
}

void KZHUDService::UpdateMove(u64 newlyPressed, u64 held)
{
	const MHUDElementDef &def = MHUD_ELEMENTS[(i32)this->edit.element];
	if (newlyPressed & IN_RELOAD)
	{
		this->edit.startX = (f32)def.xDefault;
		this->edit.startY = (f32)def.yDefault;
		this->edit.yawSum = this->edit.pitchSum = 0.0f;
	}

	// Turning right lowers yaw, looking down raises pitch.
	const f32 countsX = -this->edit.yawSum / this->edit.yawPerCount;
	const f32 countsY = this->edit.pitchSum / this->edit.pitchPerCount;
	f32 x = this->edit.startX + countsX * MHUD_EDIT_UNITS_PER_COUNT / this->edit.unitsPerPctX;
	f32 y = this->edit.startY + countsY * MHUD_EDIT_UNITS_PER_COUNT / MHUD_EDIT_UNITS_PER_PCT_Y;
	// This works like a cursor at the screen edge. Pushing further moves the start along, so coming back responds at
	// once.
	const f32 rawX = x, rawY = y;
	this->ClampEditBox(this->edit.element, x, y);
	this->edit.startX += x - rawX;
	this->edit.startY += y - rawY;

	this->edit.guideShown[0] = this->edit.guideShown[1] = false;
	if (!(held & IN_SPEED))
	{
		f32 box[4];
		this->GetEditBox(this->edit.element, x, y, box);
		const f32 own[2][3] = {{box[0], (box[0] + box[1]) * 0.5f, box[1]}, {box[2], (box[2] + box[3]) * 0.5f, box[3]}};
		const f32 anchor[2] = {x, y};
		const f32 defaults[2] = {(f32)def.xDefault, (f32)def.yDefault};
		const f32 threshold[2] = {MHUD_EDIT_SNAP_UNITS / this->edit.unitsPerPctX, MHUD_EDIT_SNAP_UNITS / MHUD_EDIT_UNITS_PER_PCT_Y};
		f32 shift[2] = {};
		for (i32 axis = 0; axis < 2; axis++)
		{
			f32 best = threshold[axis];
			const EditTargets &targets = this->edit.targets[axis];
			for (i32 line = 0; line < 3; line++)
			{
				for (i32 t = 0; t < targets.count; t++)
				{
					const f32 delta = targets.lines[t] - own[axis][line];
					if (fabsf(delta) < best)
					{
						best = fabsf(delta);
						shift[axis] = delta;
						this->edit.guide[axis] = targets.lines[t];
						this->edit.guideShown[axis] = true;
					}
				}
			}
			// The default position is a point, so only the anchor snaps to it.
			const f32 delta = defaults[axis] - anchor[axis];
			if (fabsf(delta) < best)
			{
				shift[axis] = delta;
				this->edit.guide[axis] = defaults[axis];
				this->edit.guideShown[axis] = true;
			}
		}
		x += shift[0];
		y += shift[1];
		this->ClampEditBox(this->edit.element, x, y);
	}
	this->edit.dragX = RoundFloatToInt(x * 10.0f) / 10.0f;
	this->edit.dragY = RoundFloatToInt(y * 10.0f) / 10.0f;
}

// === Geometry =======================================================================

f32 KZHUDService::GetLayoutSize(MHUDElement element)
{
	if (this->edit.mode == EditMode::Dragging && this->edit.element == element && this->edit.corner >= 0)
	{
		return this->edit.dragSize;
	}
	return this->GetPrefs().elements[(i32)element].size;
}

void KZHUDService::GetEditBoxSize(MHUDElement element, f32 &width, f32 &height)
{
	const MHUDPrefs &prefs = this->GetOwnPrefs();
	const f32 size = this->GetLayoutSize(element);
	if (element == MHUDElement::Jumpstats)
	{
		// The sample shows every row the player has on, and the history unless it is off.
		i32 rows = 0;
		for (bool shown : prefs.jsFields.shown)
		{
			rows += shown ? 1 : 0;
		}
		const f32 scale = Clamp((i32)size, MHUD_JS_SIZE_MIN, MHUD_JS_SIZE_MAX) / 100.0f;
		width = MHUD_EDIT_JS_WIDTH * scale + MHUD_EDIT_BOX_PADDING;
		if (prefs.jsCompact)
		{
			height = MHUD_EDIT_JS_COMPACT_HEIGHT * scale + MHUD_EDIT_BOX_PADDING;
			return;
		}
		const f32 history = prefs.jsHistory ? MHUD_EDIT_JS_HISTORY_HEIGHT : 0.0f;
		height = (history + MHUD_EDIT_JS_PANEL_HEIGHT + rows * MHUD_EDIT_JS_ROW_HEIGHT) * scale + MHUD_EDIT_BOX_PADDING;
		return;
	}
	if (element == MHUDElement::Course)
	{
		i32 rows = prefs.courseRecords ? (prefs.coursePro ? 2 : 1) : 0;
		const f32 scale = Clamp((i32)size, MHUD_JS_SIZE_MIN, MHUD_JS_SIZE_MAX) / 100.0f;
		const f32 history = prefs.courseSplits ? MHUD_EDIT_COURSE_HISTORY_HEIGHT : 0.0f;
		// The sample shows every enabled zone-count row and the route progress row.
		f32 head = prefs.courseMap ? MHUD_EDIT_COURSE_HEAD_HEIGHT : 0.0f;
		if (prefs.courseProgress || prefs.showProgress)
		{
			rows += (prefs.courseProgress ? MHUD_COURSE_PROGRESS_COUNT : 0) + (prefs.showProgress ? 1 : 0);
			head += MHUD_EDIT_COURSE_PROGRESS_GAP;
		}
		width = MHUD_EDIT_COURSE_WIDTH * scale + MHUD_EDIT_BOX_PADDING;
		height = (history + MHUD_EDIT_COURSE_PADDING + head + rows * MHUD_EDIT_COURSE_ROW_HEIGHT) * scale + MHUD_EDIT_BOX_PADDING;
		return;
	}
	if (element == MHUDElement::Keys)
	{
		// Mirrors keys-size.css, where the size-20 boxes (55x35 wide, 40x40 square, 10px gaps) scale with the size.
		const f32 scale = Clamp((i32)size, MHUD_SIZE_MIN, MHUD_SIZE_MAX) / 20.0f;
		const f32 keyWidth = roundf((prefs.keysSquare ? 40.0f : 55.0f) * scale);
		const f32 keyHeight = roundf((prefs.keysSquare ? 40.0f : 35.0f) * scale);
		const f32 gap = roundf(10.0f * scale);
		width = keyWidth * 3.0f + gap * 2.0f + MHUD_EDIT_BOX_PADDING;
		height = keyHeight * 2.0f + gap + MHUD_EDIT_BOX_PADDING;
		return;
	}
	f32 ems = 0.0f;
	const std::string text = this->GetEditSampleText(element);
	for (const u8 *c = (const u8 *)text.c_str(); *c; c++)
	{
		if ((*c & 0xC0) != 0x80) // skip UTF-8 continuation bytes
		{
			ems += *c < 0x80 ? MHUD_EDIT_BOX_EM_PER_CHAR : MHUD_EDIT_BOX_EM_WIDE_CHAR;
		}
	}
	const f32 fontSize = (f32)panorama::SnapToStep((i32)size, 0, 500);
	width = ems * fontSize + MHUD_EDIT_BOX_PADDING;
	height = fontSize * MHUD_EDIT_BOX_EM_HEIGHT + MHUD_EDIT_BOX_PADDING;
}

void KZHUDService::GetEditBox(MHUDElement element, f32 x, f32 y, f32 box[4])
{
	f32 width, height;
	this->GetEditBoxSize(element, width, height);
	width /= this->edit.unitsPerPctX;
	height /= MHUD_EDIT_UNITS_PER_PCT_Y;
	const MHUDAlign align = element == MHUDElement::Keys ? MHUDAlign::Center : this->GetOwnPrefs().elements[(i32)element].align;
	box[0] = align == MHUDAlign::Left ? x : (align == MHUDAlign::Right ? x - width : x - width * 0.5f);
	box[1] = box[0] + width;
	box[2] = IsMHUDElementTopAnchored(element) ? y : y - height * 0.5f;
	box[3] = box[2] + height;
}

void KZHUDService::ClampEditBox(MHUDElement element, f32 &x, f32 &y)
{
	f32 box[4];
	this->GetEditBox(element, x, y, box);
	// Percent offsets are from the screen centre, so the screen spans -50..50 on both axes.
	for (i32 axis = 0; axis < 2; axis++)
	{
		const f32 lo = box[axis * 2], hi = box[axis * 2 + 1];
		f32 &value = axis == 0 ? x : y;
		if (hi - lo >= 100.0f)
		{
			value -= (lo + hi) * 0.5f; // a box wider than the screen gets centred
		}
		else if (lo < -50.0f)
		{
			value += -50.0f - lo;
		}
		else if (hi > 50.0f)
		{
			value -= hi - 50.0f;
		}
	}
}

void KZHUDService::BuildEditTargets()
{
	// The screen centre and edges; percent positions put them at 0 and +-50 whatever the aspect ratio.
	for (i32 axis = 0; axis < 2; axis++)
	{
		EditTargets &targets = this->edit.targets[axis];
		targets.count = 0;
		targets.lines[targets.count++] = 0.0f;
		targets.lines[targets.count++] = -50.0f;
		targets.lines[targets.count++] = 50.0f;
	}
	const MHUDPrefs &prefs = this->GetOwnPrefs();
	for (i32 i = 0; i < (i32)MHUDElement::Count; i++)
	{
		if (i == (i32)this->edit.element || !prefs.elements[i].enabled)
		{
			continue;
		}
		f32 box[4];
		this->GetEditBox((MHUDElement)i, prefs.elements[i].x, prefs.elements[i].y, box);
		const f32 lines[2][3] = {{box[0], (box[0] + box[1]) * 0.5f, box[1]}, {box[2], (box[2] + box[3]) * 0.5f, box[3]}};
		for (i32 axis = 0; axis < 2; axis++)
		{
			EditTargets &targets = this->edit.targets[axis];
			for (i32 line = 0; line < 3 && targets.count < KZ_ARRAYSIZE(targets.lines); line++)
			{
				targets.lines[targets.count++] = lines[axis][line];
			}
		}
	}
}

// === Layout =========================================================================

void KZHUDService::UpdateEditLayout(CCSCustomHudLayout *layout, bool force)
{
	if (force)
	{
		this->layoutEdit = LayoutEditState();
	}
	const bool editing = this->IsEditingHud();
	const MHUDPrefs &prefs = this->GetOwnPrefs();
	for (i32 i = 0; i < (i32)MHUDElement::Count; i++)
	{
		const MHUDElementDef &def = MHUD_ELEMENTS[i];
		LayoutElementState &state = this->layoutElements[i];
		const bool shown = editing && prefs.elements[i].enabled && !state.hidden;
		if (state.hitShown != shown)
		{
			state.hitShown = shown;
			layout->SetHasClass(def.hitPanelId, "hidden", shown ? k_eHudPanelClassStatus_DoesNotHaveClass : k_eHudPanelClassStatus_HasClass);
		}
		if (!shown)
		{
			continue;
		}
		// Same anchor as the element; the wrapper they share carries the fine offset.
		this->SetLayoutClass(layout, def.hitPanelId, state.hitAlignClass, state.alignClass);
		this->SetLayoutValueClass(layout, def.hitPanelId, state.hitX, state.x, "x", true);
		this->SetLayoutValueClass(layout, def.hitPanelId, state.hitY, state.y, "y", true);
		f32 width, height;
		this->GetEditBoxSize((MHUDElement)i, width, height);
		SetBoxClass(layout, def.hitPanelId, state.hitWidth, width, "w", MHUD_EDIT_BOX_MAX_WIDTH);
		SetBoxClass(layout, def.hitPanelId, state.hitHeight, height, "h", MHUD_EDIT_BOX_MAX_HEIGHT);
		const bool dragging = this->edit.mode == EditMode::Dragging && this->edit.element == (MHUDElement)i;
		if (state.hitDragging != dragging)
		{
			state.hitDragging = dragging;
			layout->SetHasClass(def.hitPanelId, "dragging", dragging ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
		}
	}

	if (this->layoutEdit.active != editing)
	{
		this->layoutEdit.active = editing;
		layout->SetHasClass("mhud_edit_bar", "hidden", editing ? k_eHudPanelClassStatus_DoesNotHaveClass : k_eHudPanelClassStatus_HasClass);
	}
	if (editing)
	{
		for (i32 i = 0; i < KZ_ARRAYSIZE(EDIT_TEXTS); i++)
		{
			const std::string text = this->player->languageService->PrepareMessage(EDIT_TEXTS[i].phraseKey);
			if (this->layoutEdit.texts[i] != text)
			{
				this->layoutEdit.texts[i] = text;
				layout->SetDialogVariableString(EDIT_TEXTS[i].panelId, EDIT_TEXTS[i].variable, text.c_str());
			}
		}
		if (!this->layoutEdit.keysSet)
		{
			this->layoutEdit.keysSet = true;
			for (const auto &key : EDIT_KEYS)
			{
				layout->SetDialogVariableString(key[0], "editkey", key[1]);
			}
		}
		// Picking shows the pick hint and Done; dragging swaps them for the key row.
		const bool dragging = this->edit.mode == EditMode::Dragging;
		if (this->layoutEdit.dragging != dragging)
		{
			this->layoutEdit.dragging = dragging;
			const auto shownWhenDragging = dragging ? k_eHudPanelClassStatus_DoesNotHaveClass : k_eHudPanelClassStatus_HasClass;
			const auto hiddenWhenDragging = dragging ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass;
			layout->SetHasClass("mhud_edit_keys", "hidden", shownWhenDragging);
			layout->SetHasClass("mhud_edit_hint", "hidden", hiddenWhenDragging);
			layout->SetHasClass("mhud_edit_done", "hidden", hiddenWhenDragging);
		}
	}

	for (i32 axis = 0; axis < 2; axis++)
	{
		const bool shown = this->edit.mode == EditMode::Dragging && this->edit.guideShown[axis];
		if (this->layoutEdit.guideShown[axis] != shown)
		{
			this->layoutEdit.guideShown[axis] = shown;
			layout->SetHasClass(GUIDE_PANELS[axis], "hidden", shown ? k_eHudPanelClassStatus_DoesNotHaveClass : k_eHudPanelClassStatus_HasClass);
		}
		if (shown)
		{
			this->SetLayoutPosition(layout, GUIDE_PANELS[axis], GUIDE_POS_PANELS[axis], this->layoutEdit.guideCoarse[axis],
									this->layoutEdit.guideFine[axis], RoundFloatToInt(this->edit.guide[axis] * 10.0f), axis == 0 ? "x" : "y");
		}
	}
}
