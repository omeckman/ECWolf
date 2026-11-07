/*
** am_map.h
**
**---------------------------------------------------------------------------
** Copyright 2013 Braden Obrzut
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
**
*/

#ifndef __AM_MAP_H__
#define __AM_MAP_H__

#include "gamemap.h"
#include "tarray.h"
#include "vectors.h"

enum
{
	AMA_Off,
	AMA_Mini,
	AMA_Overlay,
	AMA_Normal
};

enum
{
	AMK_Full,
	AMK_Overlay,
	AMK_Minimap,

	NUM_AMKINDS
};

enum
{
	AMS_SecretWalls,
	AMS_Keys,
	AMS_Weapons,
	AMS_Ammo,
	AMS_Health,
	AMS_Treasure,
	AMS_ExtraLives,
	AMS_Corpses,

	NUM_AMSHOW,

	// Configured with modes instead of switches
	AMS_Enemies = NUM_AMSHOW,
	AMS_Obstacles,
	AMS_Decorations,
	AMS_None
};

enum
{
	AMTS_Large,
	AMTS_Tile
};

enum
{
	AMDM_Off,
	AMDM_Obstacles,
	AMDM_All
};

enum
{
	AMRM_Off,
	AMRM_Gray,
	AMRM_Lines,
	AMRM_Full
};

enum
{
	AMMS_Framed,
	AMMS_Floating
};

// Objects only use Explored and All
enum
{
	AMSM_Off,
	AMSM_InSight,
	AMSM_Explored,
	AMSM_All
};

extern unsigned automap;
extern bool am_cheat;
extern unsigned am_revealmap;
extern unsigned am_thingsize;
extern bool am_rotate;
extern bool am_overlayrotate;
extern bool am_minimaprotate;
extern bool am_drawtexturedwalls;
extern bool am_overlaytextured;
extern bool am_minimaptextured;
extern bool am_drawfloors;
extern bool am_overlayfloors;
extern bool am_minimapfloors;
extern unsigned am_overlay;
extern bool am_minimap;
extern unsigned am_minimapstyle;
#define AM_MINIMAPSIZE_MAX 23
extern int am_minimapsize;
#define AM_MINIMAPZOOM_MAX 20
extern int am_minimapzoom;
#define AM_MINIMAPTRANSLUCENCY_MAX 20
extern int am_minimaptranslucency;
extern bool am_pause;
extern bool am_showratios;
extern bool am_show[NUM_AMKINDS][NUM_AMSHOW];
extern unsigned am_enemymode[NUM_AMKINDS];
extern unsigned am_objectmode[NUM_AMKINDS];
extern unsigned am_decorationmode[NUM_AMKINDS];

void AM_ChangeResolution();
void AM_CheckKeys();
void AM_UpdateFlags();
void AM_Toggle();

void BasicOverhead();

struct AMVectorPoint;

class AutoMap
{
public:
	enum AMFlags
	{
		AMF_Rotate = 0x1,
		AMF_DrawTexturedWalls = 0x2,
		AMF_DrawFloor = 0x4,
		AMF_Overlay = 0x8,
		AMF_DispInfo = 0x10,
		AMF_DispRatios = 0x20,
		AMF_ShowThings = 0x40,
		AMF_Minimap = 0x80,
		AMF_EnemiesInSight = 0x100,
		AMF_EnemiesAll = 0x200,
		AMF_ObjectsAll = 0x400,
		// One bit per AMS_* type
		AMF_ShowFirst = 0x800,
		AMF_ShowMask = ((1<<AMS_None)-1)*AMF_ShowFirst
	};

	struct Color
	{
		uint32 color;
		byte palcolor;

		Color &operator=(int rgb);
	};

	AutoMap(unsigned int flags=0, fixed scale=FRACUNIT/4);
	~AutoMap();

	void CalculateDimensions(unsigned int x, unsigned int y, unsigned int width, unsigned int height);
	void Draw();
	fixed GetScale() const { return absscale; }
	fixed GetScreenScale() const;
	void SetFlags(unsigned int flags, bool set);
	void SetPanning(fixed x, fixed y, bool relative);
	void SetScale(fixed scale, bool relative);

protected:
	void ClipTile(TArray<FVector2> &points) const;
	void DrawActor(class AActor *actor, fixed x, fixed y, fixed scale, bool marked=false);
	void DrawClippedLine(int x0, int y0, int x1, int y1, int palcolor, uint32 realcolor) const;
	void DrawStats() const;
	void DrawTileEdges(MapSpot spot, fixed x, fixed y, const Color &c) const;
	void DrawVector(const AMVectorPoint *points, unsigned int numPoints, fixed x, fixed y, fixed scale, angle_t angle, const Color &c) const;
	FVector2 GetClipIntersection(const FVector2 &p1, const FVector2 &p2, unsigned edge) const;
	bool IsShown(unsigned int what, MapSpot spot) const;
	bool TransformTile(MapSpot spot, fixed x, fixed y, TArray<FVector2> &points) const;

private:
	double rottable[2][2];

	bool fullRefresh;
	unsigned int amFlags;
	int amsizex, amsizey, amx, amy;
	fixed ampanx, ampany;
	fixed amsin, amcos;
	fixed absscale;
	angle_t amangle;
	unsigned short minmaxSel;

	Color ArrowColor;
	Color BackgroundColor;
	Color FloorColor;
	Color WallColor;
	Color DoorColor;
	Color SecretWallColor;

	Color RevealedFloorColor;
	Color RevealedWallColor;
	Color RevealedDoorColor;
	Color RevealedWallLineColor;
	Color RevealedDoorLineColor;
};

extern AutoMap AM_Main;

#endif
