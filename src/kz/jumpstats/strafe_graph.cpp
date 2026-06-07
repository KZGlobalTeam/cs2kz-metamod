/*
 * Console strafe/mouse graph helpers.
 * Ported from GameChaos's distbugfix.
 */

#include "kz_jumpstats.h"
#include "utils/utils.h"

enum GraphJumpDir
{
	GraphJumpDir_Forwards,
	GraphJumpDir_Backwards,
	GraphJumpDir_Left,
	GraphJumpDir_Right,
};

enum GraphStrafeType : u8
{
	GraphStrafeType_Overlap,
	GraphStrafeType_None,
	GraphStrafeType_Left,
	GraphStrafeType_OverlapLeft,
	GraphStrafeType_NoneLeft,
	GraphStrafeType_Right,
	GraphStrafeType_OverlapRight,
	GraphStrafeType_NoneRight,
};

static_global constexpr const char *strafeTypeSymbols[] = {
	"$", // GraphStrafeType_Overlap
	".", // GraphStrafeType_None
	"|", // GraphStrafeType_Left
	"#", // GraphStrafeType_OverlapLeft
	"H", // GraphStrafeType_NoneLeft
	"|", // GraphStrafeType_Right
	"#", // GraphStrafeType_OverlapRight
	"H"  // GraphStrafeType_NoneRight
};

static_global constexpr i32 GRAPH_STRAFE_TYPE_COUNT = KZ_ARRAYSIZE(strafeTypeSymbols);
static_global constexpr i32 JS_CONSOLE_GRAPH_MAX_FRAMES = 150;

static_function i32 GetTotalAACallCount(const Jump &jump)
{
	i32 totalCalls = 0;
	FOR_EACH_VEC(jump.strafes, i)
	{
		totalCalls += jump.strafes[i].aaCalls.Count();
	}
	return totalCalls;
}

static_function GraphJumpDir GetGraphJumpDir(const Jump &jump, f32 firstYaw)
{
	if (jump.originalJumpType == JumpType_LadderJump || jump.takeoffVelocity.Length2D() <= 50.0f)
	{
		return GraphJumpDir_Forwards;
	}

	f32 velocityYaw = RAD2DEG(atan2f(jump.takeoffVelocity.y, jump.takeoffVelocity.x));
	f32 yawDelta = utils::NormalizeDeg(firstYaw - velocityYaw);

	if (yawDelta >= 45.0f && yawDelta <= 135.0f)
	{
		return GraphJumpDir_Right;
	}
	if (yawDelta >= -135.0f && yawDelta <= -45.0f)
	{
		return GraphJumpDir_Left;
	}
	if (yawDelta > 135.0f || yawDelta < -135.0f)
	{
		return GraphJumpDir_Backwards;
	}
	return GraphJumpDir_Forwards;
}

static_function u64 GetLeftGraphButton(GraphJumpDir jumpDir)
{
	switch (jumpDir)
	{
		case GraphJumpDir_Forwards:
		{
			return IN_MOVELEFT;
		}
		case GraphJumpDir_Backwards:
		{
			return IN_MOVERIGHT;
		}
		case GraphJumpDir_Left:
		{
			return IN_BACK;
		}
		case GraphJumpDir_Right:
		{
			return IN_FORWARD;
		}
	}
	return IN_MOVELEFT;
}

static_function u64 GetRightGraphButton(GraphJumpDir jumpDir)
{
	switch (jumpDir)
	{
		case GraphJumpDir_Forwards:
		{
			return IN_MOVERIGHT;
		}
		case GraphJumpDir_Backwards:
		{
			return IN_MOVELEFT;
		}
		case GraphJumpDir_Left:
		{
			return IN_FORWARD;
		}
		case GraphJumpDir_Right:
		{
			return IN_BACK;
		}
	}
	return IN_MOVERIGHT;
}

static_function bool IsGraphWishspeedMovingLeft(f32 forwardspeed, f32 sidespeed, GraphJumpDir jumpDir)
{
	switch (jumpDir)
	{
		case GraphJumpDir_Forwards:
		{
			return sidespeed > 0.03125f;
		}
		case GraphJumpDir_Backwards:
		{
			return sidespeed < -0.03125f;
		}
		case GraphJumpDir_Left:
		{
			return forwardspeed < -0.03125f;
		}
		case GraphJumpDir_Right:
		{
			return forwardspeed > 0.03125f;
		}
	}
	return false;
}

static_function bool IsGraphWishspeedMovingRight(f32 forwardspeed, f32 sidespeed, GraphJumpDir jumpDir)
{
	switch (jumpDir)
	{
		case GraphJumpDir_Forwards:
		{
			return sidespeed < -0.03125f;
		}
		case GraphJumpDir_Backwards:
		{
			return sidespeed > 0.03125f;
		}
		case GraphJumpDir_Left:
		{
			return forwardspeed > 0.03125f;
		}
		case GraphJumpDir_Right:
		{
			return forwardspeed < -0.03125f;
		}
	}
	return false;
}

static_function void GetWishspeedComponents(const AACall &call, f32 &forwardspeed, f32 &sidespeed)
{
	if (call.wishspeed == 0.0f)
	{
		forwardspeed = 0.0f;
		sidespeed = 0.0f;
		return;
	}

	QAngle wishAngles = vec3_angle;
	wishAngles.y = call.currentYaw;

	Vector forward, right;
	AngleVectors(wishAngles, &forward, &right, nullptr);

	forwardspeed = DotProduct(call.wishdir, forward) * call.wishspeed;
	sidespeed = DotProduct(call.wishdir, right) * call.wishspeed;
}

static_function GraphStrafeType GetGraphStrafeType(const AACall &call, GraphJumpDir jumpDir)
{
	bool moveLeft = (call.buttons[0] & GetLeftGraphButton(jumpDir)) != 0;
	bool moveRight = (call.buttons[0] & GetRightGraphButton(jumpDir)) != 0;

	f32 forwardspeed = 0.0f;
	f32 sidespeed = 0.0f;
	GetWishspeedComponents(call, forwardspeed, sidespeed);

	bool velLeft = IsGraphWishspeedMovingLeft(forwardspeed, sidespeed, jumpDir);
	bool velRight = IsGraphWishspeedMovingRight(forwardspeed, sidespeed, jumpDir);
	bool velIsZero = !velLeft && !velRight;

	if (moveLeft && !moveRight && velLeft)
	{
		return GraphStrafeType_Left;
	}
	if (moveRight && !moveLeft && velRight)
	{
		return GraphStrafeType_Right;
	}
	if (moveLeft && !moveRight && velRight)
	{
		return GraphStrafeType_Left;
	}
	if (moveRight && !moveLeft && velLeft)
	{
		return GraphStrafeType_Right;
	}
	if (moveRight && moveLeft && velIsZero)
	{
		return GraphStrafeType_Overlap;
	}
	if (moveRight && moveLeft && velLeft)
	{
		return GraphStrafeType_OverlapLeft;
	}
	if (moveRight && moveLeft && velRight)
	{
		return GraphStrafeType_OverlapRight;
	}
	if (!moveRight && !moveLeft && velIsZero)
	{
		return GraphStrafeType_None;
	}
	if (!moveRight && !moveLeft && velLeft)
	{
		return GraphStrafeType_NoneLeft;
	}
	if (!moveRight && !moveLeft && velRight)
	{
		return GraphStrafeType_NoneRight;
	}
	return GraphStrafeType_None;
}

static_function GraphStrafeType GetGraphLeftType(GraphStrafeType type)
{
	if (type == GraphStrafeType_Right || type == GraphStrafeType_NoneRight || type == GraphStrafeType_OverlapRight)
	{
		return GraphStrafeType_None;
	}
	return type;
}

static_function GraphStrafeType GetGraphRightType(GraphStrafeType type)
{
	if (type == GraphStrafeType_Left || type == GraphStrafeType_NoneLeft || type == GraphStrafeType_OverlapLeft)
	{
		return GraphStrafeType_None;
	}
	return type;
}

static_function const char *GetGraphStrafeSymbol(GraphStrafeType type)
{
	return strafeTypeSymbols[static_cast<u8>(type)];
}

bool Jump::BuildConsoleStrafeMouseGraph(std::string &strafeLeft, std::string &strafeRight, std::string &mouseLeft, std::string &mouseRight) const
{
	strafeLeft.clear();
	strafeRight.clear();
	mouseLeft.clear();
	mouseRight.clear();

	i32 totalCallCount = GetTotalAACallCount(*this);
	i32 graphCallCount = Min(totalCallCount, JS_CONSOLE_GRAPH_MAX_FRAMES);
	if (this->IsFailstat() && this->failstatGraphCallCount > 0)
	{
		graphCallCount = Min(graphCallCount, this->failstatGraphCallCount);
	}

	if (graphCallCount <= 0)
	{
		return false;
	}

	std::vector<const AACall *> calls;
	calls.reserve(graphCallCount);
	FOR_EACH_VEC(this->strafes, i)
	{
		FOR_EACH_VEC(this->strafes[i].aaCalls, j)
		{
			if ((i32)calls.size() >= graphCallCount)
			{
				break;
			}
			calls.push_back(&this->strafes[i].aaCalls[j]);
		}
		if ((i32)calls.size() >= graphCallCount)
		{
			break;
		}
	}

	if (calls.empty())
	{
		return false;
	}

	GraphJumpDir jumpDir = GetGraphJumpDir(*this, calls[0]->prevYaw);
	const f32 durationEpsilon = 0.000001f;

	struct TickGraphSample
	{
		GraphStrafeType strafeType;
		f32 yawDiff;
	};

	std::vector<TickGraphSample> tickSamples;
	tickSamples.reserve(JS_CONSOLE_GRAPH_MAX_FRAMES);

	f32 tickAccumulatedDuration = 0.0f;
	f32 tickStartYaw = 0.0f;
	f32 tickEndYaw = 0.0f;
	f32 typeDurations[GRAPH_STRAFE_TYPE_COUNT] = {0.0f};
	GraphStrafeType lastTickType = GraphStrafeType_None;
	bool tickHasData = false;

	auto FinalizeTick = [&]()
	{
		if (!tickHasData)
		{
			return;
		}

		u8 dominantType = static_cast<u8>(lastTickType);
		f32 bestDuration = -1.0f;
		for (u8 i = 0; i < (u8)GRAPH_STRAFE_TYPE_COUNT; i++)
		{
			if (typeDurations[i] > bestDuration + durationEpsilon)
			{
				bestDuration = typeDurations[i];
				dominantType = i;
			}
		}

		tickSamples.push_back({
			(GraphStrafeType)dominantType,
			utils::GetAngleDifference(tickEndYaw, tickStartYaw, 180.0f, true),
		});

		tickAccumulatedDuration = 0.0f;
		tickStartYaw = 0.0f;
		tickEndYaw = 0.0f;
		for (u8 i = 0; i < (u8)GRAPH_STRAFE_TYPE_COUNT; i++)
		{
			typeDurations[i] = 0.0f;
		}
		tickHasData = false;
	};

	for (const AACall *call : calls)
	{
		if ((i32)tickSamples.size() >= JS_CONSOLE_GRAPH_MAX_FRAMES)
		{
			break;
		}

		GraphStrafeType callType = GetGraphStrafeType(*call, jumpDir);
		f32 callDuration = MAX(call->duration, 0.0f);
		if (callDuration <= durationEpsilon)
		{
			continue;
		}

		if (!tickHasData)
		{
			tickStartYaw = call->prevYaw;
			tickHasData = true;
		}

		tickEndYaw = call->currentYaw;
		typeDurations[static_cast<u8>(callType)] += callDuration;
		lastTickType = callType;
		tickAccumulatedDuration += callDuration;

		if (tickAccumulatedDuration >= ENGINE_FIXED_TICK_INTERVAL - durationEpsilon)
		{
			FinalizeTick();
		}
	}

	if (tickHasData && (i32)tickSamples.size() < JS_CONSOLE_GRAPH_MAX_FRAMES)
	{
		FinalizeTick();
	}

	if (tickSamples.empty())
	{
		return false;
	}

	std::vector<GraphStrafeType> strafeTypes;
	std::vector<f32> yawDiffs;
	strafeTypes.reserve(tickSamples.size());
	yawDiffs.reserve(tickSamples.size());

	for (const TickGraphSample &sample : tickSamples)
	{
		strafeTypes.push_back(sample.strafeType);
		yawDiffs.push_back(sample.yawDiff);
	}

	strafeLeft.reserve(tickSamples.size() * 3);
	strafeRight.reserve(tickSamples.size() * 3);
	mouseLeft.reserve(tickSamples.size() * 3);
	mouseRight.reserve(tickSamples.size() * 3);

	for (size_t i = 0; i < tickSamples.size(); i++)
	{
		GraphStrafeType leftType = GetGraphLeftType(strafeTypes[i]);
		GraphStrafeType rightType = GetGraphRightType(strafeTypes[i]);

		strafeLeft += GetGraphStrafeSymbol(leftType);
		strafeRight += GetGraphStrafeSymbol(rightType);

		f32 mouseDiff = (i + 1 < yawDiffs.size()) ? yawDiffs[i + 1] : 0.0f;
		if (mouseDiff == 0.0f)
		{
			mouseLeft += ".";
			mouseRight += ".";
		}
		else if (mouseDiff > 0.0f)
		{
			mouseLeft += ".";
			mouseRight += "|";
		}
		else
		{
			mouseLeft += "|";
			mouseRight += ".";
		}
	}

	return true;
}
