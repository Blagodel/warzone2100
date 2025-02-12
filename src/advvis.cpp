/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2020  Warzone 2100 Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/**
 * @file advvis.c
 *
 * Makes smooth transitions for terrain visibility.
 */

#include "lib/framework/frame.h"

#include "advvis.h"
#include "profiling.h"
#include "map.h"

// ------------------------------------------------------------------------------------
#define FADE_IN_TIME	(GAME_TICKS_PER_SEC/10)
#define	START_DIVIDE	(8)
#define MIN_ILLUM       (45.0f)

/// Whether unexplored tiles should be shown as just darker fog. Left here as a future option
/// for scripts, since campaign may still want total darkness on unexplored tiles.
static bool bRevealActive = true;

// For display only (*NOT* for use in game state calculations)
inline float getTileIllumination(const MAPTILE *psTile)
{
	switch (terrainShaderType)
	{
		case TerrainShaderType::SINGLE_PASS:
			return psTile->ambientOcclusion; // sunlight is handled by shaders so only AO needed for lightmap
		case TerrainShaderType::FALLBACK:
			return psTile->illumination;
	}
	return psTile->illumination; // silence GCC warning
}

// ------------------------------------------------------------------------------------
void	avUpdateTiles()
{
	WZ_PROFILE_SCOPE(avUpdateTiles);
	const int len = mapHeight * mapWidth;
	const int playermask = 1 << selectedPlayer;
	UDWORD i = 0;
	float maxLevel, increment = graphicsTimeAdjustedIncrement(FADE_IN_TIME);	// call once per frame
	MAPTILE *psTile;

	PlayerMask playerAllianceBits = (selectedPlayer < MAX_PLAYER_SLOTS) ? alliancebits[selectedPlayer] : 0;

	/* Go through the tiles */
	for (; i < len; i++)
	{
		psTile = &psMapTiles[i];
		maxLevel = getTileIllumination(psTile);

		if (psTile->level > MIN_ILLUM || psTile->tileExploredBits & playermask)	// seen
		{
			// If we are not omniscient, and we are not seeing the tile, and none of our allies see the tile...
			if (!godMode && !(playerAllianceBits & (satuplinkbits | psTile->sensorBits)))
			{
				maxLevel /= 2;
			}
			if (psTile->level > maxLevel)
			{
				psTile->level = MAX(psTile->level - increment, maxLevel);
			}
			else if (psTile->level < maxLevel)
			{
				psTile->level = MIN(psTile->level + increment, maxLevel);
			}
		}
	}
}

// ------------------------------------------------------------------------------------
UDWORD	avGetObjLightLevel(BASE_OBJECT *psObj, UDWORD origLevel)
{
	float div = (float)psObj->visibleForLocalDisplay() / 255.f;
	unsigned int lowest = origLevel / START_DIVIDE;
	unsigned int newLevel = static_cast<unsigned int>(div * origLevel);

	if (newLevel < lowest)
	{
		newLevel = lowest;
	}

	return newLevel;
}

// ------------------------------------------------------------------------------------
bool	getRevealStatus()
{
	return bRevealActive;
}

// ------------------------------------------------------------------------------------
void	setRevealStatus(bool val)
{
	debug(LOG_FOG, "avSetRevealStatus: Setting reveal to %s", val ? "ON" : "OFF");
	bRevealActive = val;
}

// ------------------------------------------------------------------------------------
void	preProcessVisibility()
{
	for (int i = 0; i < mapWidth; i++)
	{
		for (int j = 0; j < mapHeight; j++)
		{
			MAPTILE *psTile = mapTile(i, j);
			psTile->level = bRevealActive ? MIN(MIN_ILLUM, getTileIllumination(psTile) / 4.0f) : 0;

			if (TEST_TILE_VISIBLE_TO_SELECTEDPLAYER(psTile))
			{
				psTile->level = getTileIllumination(psTile);
			}
		}
	}
}
// ------------------------------------------------------------------------------------
