//-----------------------------------------------------------------------------
//
// Skulltag Source
// Copyright (C) 2008 Braden Obrzut
// Copyright (C) 2008-2012 Skulltag Development Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the Skulltag Development Team nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
// 4. Redistributions in any form must be accompanied by information on how to
//    obtain complete source code for the software and any accompanying
//    software that uses the software. The source code must either be included
//    in the distribution or be available for no more than the cost of
//    distribution plus a nominal fee, and must be freely redistributable
//    under reasonable conditions. For an executable file, complete source
//    code means the source code for all modules it contains. It does not
//    include source code for modules or files that typically accompany the
//    major components of the operating system on which the executable file
//    runs.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//
//
// Filename: domination.cpp
//
// Description:
//
//-----------------------------------------------------------------------------

#include "gamemode.h"

#include "doomtype.h"
#include "doomstat.h"
#include "v_font.h"
#include "v_palette.h"
#include "v_video.h"
#include "v_text.h"
#include "chat.h"
#include "st_stuff.h"
#include "domination.h"

#include "g_level.h"
#include "network.h"
#include "r_defs.h"
#include "sv_commands.h"
#include "team.h"
#include "sectinfo.h"
#include "cl_demo.h"
#include "p_acs.h"

#define SCORERATE	(TICRATE*3)

// These default the default fade for points.  I hope no one has a grey team...
#define POINT_DEFAULT_R	0x7F
#define POINT_DEFAULT_G	0x7F
#define POINT_DEFAULT_B	0x7F

EXTERN_CVAR(Bool, domination)

//CREATE_GAMEMODE(domination, DOMINATION, "Domination", "DOM", "F1_DOM", GMF_TEAMGAME|GMF_PLAYERSEARNPOINTS|GMF_PLAYERSONTEAMS)

unsigned int *PointOwners;
unsigned int NumPoints;

bool finished;

unsigned int DOMINATION_NumPoints(void) { return NumPoints; }
unsigned int* DOMINATION_PointOwners(void) { return PointOwners; }

void DOMINATION_LoadInit(unsigned int numpoints, unsigned int* pointowners)
{
	if(!domination)
		return;

	finished = false;
	NumPoints = numpoints;
	if ( PointOwners )
		delete[] PointOwners;
	PointOwners = pointowners;
}

void DOMINATION_SendState(ULONG ulPlayerExtra)
{
	if(!domination)
		return;

	if(SERVER_IsValidClient(ulPlayerExtra) == false)
		return;

	SERVER_CheckClientBuffer(ulPlayerExtra, NumPoints + 4, true);
	SERVER_GetClient(ulPlayerExtra)->PacketBuffer.ByteStream.WriteLong(NumPoints);
	for(unsigned int i = 0;i < NumPoints;i++)
	{
		//one byte should be enough to hold the value of the team.
		SERVER_GetClient( ulPlayerExtra )->PacketBuffer.ByteStream.WriteByte(PointOwners[i]);
	}
}

void DOMINATION_Reset(void)
{
	if(!domination)
		return;

	for(unsigned int i = 0;i < level.info->SectorInfo.Points.Size();i++)
	{
		PointOwners[i] = 255;
		for(unsigned int j = 0;j < level.info->SectorInfo.Points[i]->Size();j++)
		{
			if(j < static_cast<unsigned> (numsectors))
				sectors[(*level.info->SectorInfo.Points[i])[0]].SetFade(POINT_DEFAULT_R, POINT_DEFAULT_G, POINT_DEFAULT_B);
		}
	}
}

void DOMINATION_Init(void)
{
	if(!domination)
		return;

	finished = false;
	if(PointOwners != NULL)
		delete[] PointOwners;
	PointOwners = new unsigned int[level.info->SectorInfo.Points.Size()];
	NumPoints = level.info->SectorInfo.Points.Size();

	DOMINATION_Reset();
}

void DOMINATION_Tick(void)
{
	if(!domination)
		return;

	if(finished)
		return;

	// [BB] Scoring is server-side.
	if ( NETWORK_InClientMode() )
		return;

	if(!(level.maptime % SCORERATE))
	{
		for(unsigned int i = 0;i < NumPoints;i++)
		{
			if(PointOwners[i] != 255)
			{
				// [AK] Trigger an event script when this team gets a point from a point sector.
				// The first argument is the team that owns the sector and the second argument is the name
				// of the sector. Don't let event scripts change the result value to anything less than zero.
				LONG lResult = MAX<LONG>( GAMEMODE_HandleEvent( GAMEEVENT_DOMINATION_POINT, NULL, PointOwners[i], ACS_PushAndReturnDynamicString( level.info->SectorInfo.PointNames[i]->GetChars( )), true ), 0 );
				
				if ( lResult != 0 )
					TEAM_SetPointCount( PointOwners[i], TEAM_GetPointCount( PointOwners[i] ) + lResult, false );

				if( pointlimit && (TEAM_GetPointCount(PointOwners[i]) >= pointlimit) )
				{
					DOMINATION_WinSequence(0);
					break;
				}
			}
		}
	}
}

void DOMINATION_WinSequence(unsigned int winner)
{
	if(!domination)
		return;

	finished = true;
}

void DOMINATION_SetOwnership(unsigned int point, player_t *toucher)
{
	if(!domination)
		return;

	if(point >= NumPoints)
		return;

	if(!toucher->bOnTeam) //The toucher must be on a team
		return;

	unsigned int team = toucher->Team;

	PointOwners[point] = team;
	Printf ( "%s has taken control of %s\n", toucher->userinfo.GetName(), (*level.info->SectorInfo.PointNames[point]).GetChars() );
	for(unsigned int i = 0;i < level.info->SectorInfo.Points[point]->Size();i++)
	{
		unsigned int secnum = (*level.info->SectorInfo.Points[point])[i];

		int color = TEAM_GetColor ( team );
		sectors[secnum].SetFade( RPART(color), GPART(color), BPART(color) );
	}
}

void DOMINATION_EnterSector(player_t *toucher)
{
	if(!domination)
		return;

	// [BB] This is server side.
	if ( NETWORK_InClientMode() )
	{
		return;
	}

	if(!toucher->bOnTeam) //The toucher must be on a team
		return;

	assert(PointOwners != NULL);
	for(unsigned int point = 0;point < level.info->SectorInfo.Points.Size();point++)
	{
		for(unsigned int i = 0;i < level.info->SectorInfo.Points[point]->Size();i++)
		{
			if(toucher->mo->Sector->sectornum != static_cast<signed> ((*level.info->SectorInfo.Points[point])[i]))
				continue;

			// [BB] The team already owns the point, nothing to do.
			if ( toucher->Team == PointOwners[point] )
				continue;

			// [AK] Trigger an event script when the player takes ownership of a point sector. This
			// must be called before DOMINATION_SetOwnership so that the original owner of the sector
			// is sent as the first argument. The second argument is the name of the sector.
			GAMEMODE_HandleEvent( GAMEEVENT_DOMINATION_CONTROL, toucher->mo, PointOwners[point], ACS_PushAndReturnDynamicString( level.info->SectorInfo.PointNames[point]->GetChars( )));

			DOMINATION_SetOwnership(point, toucher);

			// [BB] Let the clients know about the point ownership change.
			if( NETWORK_GetState() == NETSTATE_SERVER )
				SERVERCOMMANDS_SetDominationPointOwnership ( point, static_cast<ULONG> ( toucher - players ) );
		}
	}
}

//END_GAMEMODE(DOMINATION)
