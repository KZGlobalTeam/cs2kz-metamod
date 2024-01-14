#include "usermessages.pb.h"

#include "common.h"
#include "utils/utils.h"
#include "kz.h"
#include "utils/simplecmds.h"
#include "public/networksystem/inetworkmessages.h"

#include "checkpoint/kz_checkpoint.h"
#include "jumpstats/kz_jumpstats.h"
#include "quiet/kz_quiet.h"
#include "mode/kz_mode.h"

#include "tier0/memdbgon.h"
#include <utils/recipientfilters.h>

internal SCMD_CALLBACK(Command_KzNoclip)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->ToggleNoclip();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzHidelegs)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->ToggleHideLegs();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzCheckpoint)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->SetCheckpoint();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzTeleport)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToCheckpoint();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzPrevcp)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToPrevCp();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzNextcp)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->checkpointService->TpToNextCp();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzHide)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->quietService->ToggleHide();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzJSAlways)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->ToggleJSAlways();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzRestart)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	CALL_VIRTUAL(void, offsets::Respawn, player->GetPawn());
	return MRES_SUPERCEDE;
}

// v: a vector in 3D space
// k: a unit vector describing the axis of rotation
// theta: the angle (in radians) that v rotates around k
internal Vector RotateVector(Vector v, Vector k, f32 theta)
{
	f32 cos_theta = cosf(theta);
	f32 sin_theta = sinf(theta);

	//glm::dvec3 rotated = (v * cos_theta) + (glm::cross(k, v) * sin_theta) + (k * glm::dot(k, v)) * (1 - cos_theta);

	Vector cross;
	CrossProduct(k, v, cross);
	f32 dot = k.Dot(v);

	Vector result;
	for (i32 i = 0; i < 3; i++)
	{
		result[i] = (v[i] * cos_theta) + (cross[i] * sin_theta) + (k[i] * dot) * (1.0f - cos_theta);
	}

	return result;
}

#define MAX_PIC_WIDTH	1920
#define MAX_PIC_HEIGHT	1080

struct Picture
{
	f32 pixels[MAX_PIC_WIDTH * MAX_PIC_HEIGHT * 3];
};

internal i32 GenerateColours(Picture *picture, i32 pixel, Vector normal, Vector endPos)
{
	// colour
	picture->pixels[pixel++] = (normal[0] + 1.0f) / 2.0f;
	picture->pixels[pixel++] = (normal[1] + 1.0f) / 2.0f;
	picture->pixels[pixel++] = (normal[2] + 1.0f) / 2.0f;

	// grid
	for (i32 i = 0; i < 3; i++)
	{
		if ((i32)floorf(endPos[i]) % 16 == 0)
		{
			f32 reduction = (1.0f - fabsf(normal[i])) * 0.125f;
			picture->pixels[pixel - 3] -= reduction;
			picture->pixels[pixel - 2] -= reduction;
			picture->pixels[pixel - 1] -= reduction;
		}

		if ((i32)floorf(endPos[i] * 16.0) % 16 == 0)
		{
			f32 reduction = (1.0f - fabsf(normal[i])) * 0.0625f;
			picture->pixels[pixel - 3] -= reduction;
			picture->pixels[pixel - 2] -= reduction;
			picture->pixels[pixel - 1] -= reduction;
		}
	}
	return pixel;
}

internal i32 TracePixel(Picture *picture, Vector position, Vector pos2, bbox_t hull, CTraceFilterPlayerMovementCS *filter, i32 pixel, bool customTrace)
{
	trace_t_s2 pm;
	if (customTrace)
	{
		utils::TracePlayerBBox(position, pos2, hull, filter, pm);
	}
	else
	{
		utils::TracePlayerBBox(position, pos2, hull, filter, pm);
	}
	if (pm.fraction == 1)
	{
		picture->pixels[pixel++] = 0;
		picture->pixels[pixel++] = 0;
		picture->pixels[pixel++] = 0;
		return pixel;
	}
	return GenerateColours(picture, pixel, pm.planeNormal, pm.endpos);
}

internal f32 ConvertFov(f32 fovDegrees)
{
	return RAD2DEG(2.0f * atanf(tanf(DEG2RAD(fovDegrees) / 2.0f) * (4.0f / 3.0f)));
}

internal void TraceRayImage(Picture *picture, CCSPlayerPawn *pawn, Vector position, QAngle angles, i32 traceType, f32 width, f32 height, f32 fovIn, char *fileName, bool customTrace)
{
	// PrintToServer("Pos { %f, %f, %f } Ang { %f, %f, %f }", position[0], position[1], position[2], angles[0], angles[1], angles[2]);

	f32 invWidth = 1.0 / width;
	f32 invHeight = 1.0 / height;

	// angle diff
	f32 fov = ConvertFov(fovIn);

	f32 angle = tanf(3.1415926535f * 0.5f * fov / 180.0f);

	f32 aspectRatio = width / height;

	i32 pixel = 0;

	CTraceFilterPlayerMovementCS filter;
	utils::InitPlayerMovementTraceFilter(filter, pawn, pawn->m_Collision().m_collisionAttribute().m_nInteractsWith(), COLLISION_GROUP_PLAYER_MOVEMENT);

	bbox_t playerHull;
	playerHull.mins = { -16, -16, 0 };
	playerHull.maxs = { 16, 16, 54 };

	bbox_t pointHull;
	pointHull.mins = { -0.00001f, -0.00001f, -0.00001f };
	pointHull.maxs = { 0.00001f, 0.00001f, 0.00001f };

	// y is for yaw
	for (i32 y = 0; y < height; y++)
	{
		// x is for pitch
		for (i32 x = 0; x < width; x++)
		{
			f32 xx = (2.0 * (x * invWidth) - 1.0) * angle;
			f32 yy = (1.0 - 2.0 * (y * invHeight)) * angle / aspectRatio;

			Vector pos2(xx, yy, -1.0f);

			//NormalizeVector(pos2, pos2);

			// x axis aka pitch
			pos2 = RotateVector(pos2, Vector(1, 0, 0), DEG2RAD(90.0 - angles[PITCH]));

			// z axis aka yaw
			pos2 = RotateVector(pos2, Vector(0, 0, 1), DEG2RAD(angles[YAW] - 90.0));
			pos2 *= 10000.0f;
			pos2 += position;

			switch (traceType)
			{
				case 1:
				{
					pixel = TracePixel(picture, position, pos2, playerHull, &filter, pixel, customTrace);
				} break;
				default:
				{
					pixel = TracePixel(picture, position, pos2, pointHull, &filter, pixel, customTrace);
				} break;
			}
		}
	}
}

internal SCMD_CALLBACK(Command_KzRaytrace)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);

	f32 width = 1280;
	f32 height = 720;

	f32 fov = 90.0f;

	bool hull = false;
	bool customTrace = false;
	if (args->ArgC() > 1)
	{
		fov = atof(args->Arg(1));
		fov = MIN(MAX(fov, 0.1f), 179.0f);
		if (args->ArgC() > 2)
		{
			if (args->Arg(2)[0] != '0')
			{
				hull = true;
			}
		}
		if (args->ArgC() > 3)
		{
			if (args->Arg(3)[0] != '0')
			{
				customTrace = true;
			}
		}
	}

	char *fileName = "tracetest.tga";
	local_persist Picture picture = {};
	Vector origin;
	QAngle angles;
	player->GetOrigin(&origin);
	player->GetAngles(&angles);
	origin.z += 64.0f;
	TraceRayImage(&picture, player->GetPawn(), origin, angles, hull, width, height, fov, fileName, customTrace);

	FILE *file = fopen(fileName, "wb");
	if (file)
	{
		/*
		fprintf(file, "PF\n%.0f %.0f\n1.0\n", width, height);
		for (i32 i = 0; i < (i32)width * (i32)height * 3; i += 3)
		{
			fprintf(file, "%.5f %.5f %.5f\n", picture.pixels[i], picture.pixels[i + 1], picture.pixels[i + 2]);
		}
		// */

#pragma pack(push, 1)
		struct TgaHeader
		{
			u8  identsize;          // size of ID field that follows 18 byte header (0 usually)
			u8  colourmaptype;      // type of colour map 0=none, 1=has palette
			u8  imagetype;          // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

			i16 colourmapstart;     // first colour map entry in palette
			i16 colourmaplength;    // number of colours in palette
			u8  colourmapbits;      // number of bits per palette entry 15,16,24,32

			i16 xstart;             // image x origin
			i16 ystart;             // image y origin
			i16 width;              // image width in pixels
			i16 height;             // image height in pixels
			u8  bits;               // image bits per pixel 8,16,24,32
			u8  descriptor;         // image descriptor bits (vh flip bits)
			// pixel data follows header
		};
#pragma pack(pop)
		TgaHeader tga = {};
		tga.imagetype = 2;
		tga.width = (i16)width;
		tga.height = (i16)height;
		tga.bits = 24;
		tga.descriptor = 32;

		fwrite(&tga, sizeof(tga), 1, file);
		for (i32 i = 0; i < (i32)width * (i32)height * 3; i++)
		{
			u8 value = MIN(MAX(picture.pixels[i] * 255.0f, 0), 255);
			fwrite(&value, sizeof(value), 1, file);
		}

		fclose(file);
	}

	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzGetPos)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	Vector velocity, origin;
	QAngle angles;
	player->GetOrigin(&origin);
	player->GetAngles(&angles);
	player->GetVelocity(&velocity);
	utils::PrintConsole(player->GetController(), "setpos %f %f %f; setang %f %f %f; setvelocity %f %f %f",
		origin.x, origin.y, origin.z, angles.x, angles.y, angles.z, velocity.x, velocity.y, velocity.z);
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzSetVel)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	Vector velocity;
	for (int i = 0; i < 3; i++)
	{
		velocity[i] = atof((*args)[i + 1]);
	}
	player->SetVelocity(velocity);
	return MRES_SUPERCEDE;
}
// TODO: move command registration to the service class?
void KZ::misc::RegisterCommands()
{
	scmd::RegisterCmd("kz_noclip", Command_KzNoclip, "Toggle noclip.");
	scmd::RegisterCmd("kz_hidelegs", Command_KzHidelegs, "Hide your legs in first person.");
	scmd::RegisterCmd("kz_checkpoint", Command_KzCheckpoint, "Make a checkpoint on ground or on a ladder.");
	scmd::RegisterCmd("kz_cp", Command_KzCheckpoint, "Make a checkpoint on ground or on a ladder.");
	scmd::RegisterCmd("kz_teleport", Command_KzTeleport, "Teleport to the current checkpoint.");
	scmd::RegisterCmd("kz_tp", Command_KzTeleport, "Teleport to the current checkpoint.");
	scmd::RegisterCmd("kz_prevcp", Command_KzPrevcp, "Teleport to the last checkpoint.");
	scmd::RegisterCmd("kz_nextcp", Command_KzNextcp, "Teleport to the next checkpoint.");
	scmd::RegisterCmd("kz_hide", Command_KzHide, "Hide other players.");
	scmd::RegisterCmd("kz_jsalways", Command_KzJSAlways, "Print jumpstats for invalid jumps.");
	scmd::RegisterCmd("kz_respawn", Command_KzRestart, "Respawn.");
	scmd::RegisterCmd("kz_getpos", Command_KzGetPos);
	scmd::RegisterCmd("setvelocity", Command_KzSetVel);
	KZ::mode::RegisterCommands();
}

void KZ::misc::OnClientPutInServer(CPlayerSlot slot)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	player->Reset();
}
