#pragma once
#include "platform.h"
#include "stdint.h"
#include "utils/module.h"
#include "utlstring.h"


struct Signature {
	const char *data = nullptr;
	size_t length = 0;

	template<size_t N>
	Signature(const char(&str)[N]) {
		data = str;
		length = N - 1;
	}
};
#define DECLARE_SIG(name, sig) inline const Signature name = Signature(sig);

namespace modules
{
	inline CModule *engine;
	inline CModule *tier0;
	inline CModule *server;
	inline CModule *schemasystem;
	void Initialize();
}

namespace offsets
{
#ifdef _WIN32
	inline constexpr int GameEntitySystem = 0x58;
	inline constexpr int IsEntityPawn = 152;
	inline constexpr int IsEntityController = 153;
	inline constexpr int SetMoveType = 77;
	inline constexpr int CollisionRulesChanged = 173;
	// 5 functions after one with "Physics_SimulateEntity" "Server Game"
	inline constexpr int Teleport = 148;

#else
	inline constexpr int GameEntitySystem = 0x50;
	inline constexpr int IsEntityPawn = 151;
	inline constexpr int IsEntityController = 152;
	inline constexpr int SetMoveType = 76;
	inline constexpr int CollisionRulesChanged = 172;
	inline constexpr int Teleport = 147;
#endif
}

namespace sigs
{
#ifdef _WIN32
	/* Miscellaneous */
	
	DECLARE_SIG(CEntitySystem_ctor, "\x48\x89\x5C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x20\x48\x8B\xD9\xE8\x2A\x2A\x2A\x2A\x33\xFF\xC7");
	DECLARE_SIG(CEntitySystem_CreateEntity, "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x56\x57\x41\x56\x48\x83\xEC\x40\x4D");
	DECLARE_SIG(CBaseTrigger_StartTouch, "\x41\x56\x41\x57\x48\x83\xEC\x58\x48\x8B\x01");
	DECLARE_SIG(CBaseTrigger_EndTouch, "\x40\x53\x57\x41\x55\x48\x83\xEC\x40");

	DECLARE_SIG(NetworkStateChanged, "\x4C\x8B\xC9\x48\x8B\x09\x48\x85\xC9\x74\x2A\x48\x8B\x41\x10");

	DECLARE_SIG(StateChanged, "\x48\x89\x54\x24\x10\x55\x53\x57\x41\x55");

	// TODO
	DECLARE_SIG(UTIL_ClientPrintFilter, "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x18\x56\x57\x41\x56\x48\x81\xEC\x90\x00\x00\x00\x49\x8B\xF0");
	
	// search for the string "\"Console<0>\" say_team \"%s\"\n"
	DECLARE_SIG(Host_Say, "\x44\x89\x4C\x24\x20\x44\x88");


	// "Cannot create an entity because entity class is NULL %d\n"
	DECLARE_SIG(CreateEntity, "\x48\x89\x5C\x24\x08\x48\x89\x6C\x24\x10\x56\x57\x41\x56\x48\x83\xEC\x40\x4D");

	// "Radial using: %s\n"
	DECLARE_SIG(FindUseEntity, "\x48\x89\x5C\x24\x10\x55\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xAC\x24\x00\xEB\xFF\xFF");
	
	// search for "CCSGameRules", it's the one with like 7 lines of code in the decompiler window.
	DECLARE_SIG(CCSGameRules_ctor, "\x48\x8B\xC4\x48\x89\x48\x08\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\xA8\x68");



	/* Trace related stuff */

	// TODO
	DECLARE_SIG(InitPlayerMovementTraceFilter, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x20\x0F\xB6\x41\x37\x48\x8B\xD9");

	// TODO
	DECLARE_SIG(TracePlayerBBoxForGround, "\x48\x8B\xC4\x48\x89\x58\x10\x48\x89\x70\x18\x48\x89\x78\x20\x48\x89\x48\x08\x55\x41\x54");

	// dq offset sub_1800DFA60 ; STR 1# "npc_vphysics"
	// dq offset sub_1800DEA00 <- open this then search the jump function that this function calls
	DECLARE_SIG(InitGameTrace, "\x48\x89\x5C\x24\x08\x57\x48\x83\xEC\x20\x48\x8B\xD9\x33\xFF\x48\x8B\x0D\x2A\x2A\x2A\x2A\x48\x85\xC9");




	/* Movement related functions */

	//*(a2 + 200) = sub_7FFE4786BD30(a1->pawn); <- this function
	//   v23 = a1->pawn;
	//   a1->m_bInStuckTest = 0;
	//   v24 = v23->m_pCameraServices;
	//   v25 = sub_7FFE47482320(v23);
	//   v26 = v25;
	//   if ( v25 )
	//   {
	//     EntIndex(v25, &v35);
	//     sub_7FFE4799A200(a1, "following entity %d", v35);
	//   }
	DECLARE_SIG(GetMaxSpeed, "\x48\x89\x5C\x24\x18\x57\x48\x83\xEC\x30\x80\xB9\xC2\x02\x00\x00\x00");

	// "pa start %f"
	DECLARE_SIG(ProcessMovement, "\x40\x56\x57\x48\x81\xEC\xA8\x00\x00\x00\x4C\x8B\x49\x30");

	DECLARE_SIG(PlayerMoveNew, "\x48\x89\x5C\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x2A\x48\x8B\xF9\x48\x8B\xDA");

	// Second thing called during PlayerMoveNew
	DECLARE_SIG(CheckParameters, "\x48\x8B\xC4\x48\x89\x58\x10\x48\x89\x70\x18\x55\x57\x41\x54\x41\x56\x41\x57\x48\x8D\x68\xA1\x48\x81\xEC\xD0\x00\x00\x00");

	// dev_cs_force_disable_move
	DECLARE_SIG(CanMove, "\x40\x53\x48\x83\xEC\x30\x48\x8B\xD9\xBA\xFF\xFF\xFF\xFF\x48\x8D\x0D\x2A\x2A\x2A\x2A\xE8\x2A\x2A\x2A\x2A\x48\x85\xC0\x75\x2A\x48\x8B\x05\x2A\x2A\x2A\x2A\x48\x8B\x40\x08\x80\x38\x00\x0F\x85");

	DECLARE_SIG(FullWalkMove, "\x40\x53\x57\x48\x83\xEC\x68\x48\x8B\xFA\x48\x8B\xD9");

	// "ShouldApplyGravity" string
	DECLARE_SIG(MoveInit, "\x40\x53\x57\x41\x56\x48\x83\xEC\x50\x48\x8B\xD9");

	// TODO
	DECLARE_SIG(CheckWater, "\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x78\x10\x55\x48\x8D\xA8\x68\xFF\xFF\xFF");

	// TODO
	DECLARE_SIG(CheckVelocity, "\x48\x8B\xC4\x4C\x89\x40\x18\x48\x89\x50\x10\x55\x53\x56\x57\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8D\x68\xA1");

	// TODO
	DECLARE_SIG(Duck, "\x48\x8B\xC4\x48\x89\x58\x20\x55\x56\x57\x41\x56\x41\x57\x48\x8D\xA8\xD8\xFE\xFF\xFF");

	// sv_ladder_dampen
	DECLARE_SIG(LadderMove, "\x40\x55\x56\x57\x41\x54\x41\x55\x48\x8D\xAC\x24\xE0\xFC\xFF\xFF");

	// Look for sv_jump_spam_penalty_time
	DECLARE_SIG(CheckJumpButton, "\x48\x89\x5C\x24\x18\x56\x48\x83\xEC\x40\x48\x8B\xF2");

	// "player_jump"
	DECLARE_SIG(OnJump, "\x40\x53\x57\x48\x81\xEC\x98\x00\x00\x00\x48\x8B\xD9\x48\x8B\xFA");

	// Look for sv_air_max_wishspeed
	DECLARE_SIG(AirAccelerate, "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x48\x89\x7C\x24\x18\x55\x48\x8D\x6C\x24\xB1");

	// sub_7FFE47869020(a1, a2); <- this function
	// sub_7FFE47863820(a1, a2, "FullWalkMovePreMove"); 
	// sub_7FFE4787B4C0(a1, a2); <- the walkmove function
	DECLARE_SIG(Friction, "\x48\x89\x74\x24\x18\x57\x48\x83\xEC\x60\x48\x8B\x41\x30");

	// sub_7FFE47863820(a1, a2, "FullWalkMovePreMove");
	// sub_7FFE4787B4C0(a1, a2); <- this function
	DECLARE_SIG(WalkMove, "\x48\x8B\xC4\x48\x89\x58\x18\x48\x89\x70\x20\x55\x57\x41\x54\x48\x8D");

	// "CCSPlayer_MovementServices::TryPlayerMove() Trace ended stuck"
	DECLARE_SIG(TryPlayerMove, "\x48\x8B\xC4\x4C\x89\x48\x20\x4C\x89\x40\x18\x48\x89\x50\x10\x55\x53\x56\x57\x41\x54\x41\x55");

	// sub_18061AF90(a1, a2, *(*(a1 + 48) + 864i64) & 1); <- this one
	// sub_18061D5B0(a1, a2, "PlayerMove_PostMove");
	DECLARE_SIG(CategorizePosition, "\x40\x55\x56\x57\x41\x57\x48\x8D\xAC\x24\x28\xFE\xFF\xFF");

	// TODO
	DECLARE_SIG(FinishGravity, "\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x40\x4C\x8B\x41\x30");

	// sv_staminalandcost only ref
	DECLARE_SIG(CheckFalling, "\x48\x89\x5C\x24\x10\x57\x48\x83\xEC\x60\xF3\x0F\x10\x81\x20\x02\x00\x00");

	// TODO
	DECLARE_SIG(PlayerMovePost, "\x40\x53\x56\x48\x83\xEC\x58\x48\x8B\x59\x30");

	// [%.0f %.0f %.0f] relocated from [%.0f %.0f %.0f] in %0.1f sec (v2d = %.0f u/s)\n" "health=%d, armor=%d, helmet=%d
	DECLARE_SIG(PostThink, "\x48\x8B\xC4\x48\x89\x48\x08\x55\x53\x56\x57\x41\x56\x48\x8D\xA8\xD8\xFE\xFF\xFF");
#else
#endif
}