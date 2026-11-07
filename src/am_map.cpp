/*
** am_map.cpp
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

#include "wl_def.h"
#include "am_map.h"
#include "colormatcher.h"
#include "id_ca.h"
#include "id_in.h"
#include "id_vl.h"
#include "id_vh.h"
#include "g_mapinfo.h"
#include "gamemap.h"
#include "lnspec.h"
#include "r_data/colormaps.h"
#include "r_sprites.h"
#include "v_font.h"
#include "v_video.h"
#include "wl_agent.h"
#include "wl_draw.h"
#include "wl_game.h"
#include "wl_main.h"
#include "wl_net.h"
#include "wl_play.h"
#include "g_shared/a_inventory.h"
#include "thingdef/thingdef.h"

AutoMap::Color &AutoMap::Color::operator=(int rgb)
{
	color = rgb;
	palcolor = ColorMatcher.Pick(RPART(rgb), GPART(rgb), BPART(rgb));
	return *this;
}

AutoMap AM_Main;
AutoMap AM_Overlay;
AutoMap AM_Mini;

enum
{
	AMO_Off,
	AMO_On,
	AMO_Both
};

unsigned automap = 0;
bool am_cheat = false;
unsigned am_revealmap = AMRM_Off;
unsigned am_thingsize = AMTS_Large;
bool am_rotate = false;
bool am_overlayrotate = false;
bool am_minimaprotate = false;
bool am_drawtexturedwalls = true;
bool am_overlaytextured = false;
bool am_minimaptextured = false;
bool am_drawfloors = false;
bool am_overlayfloors = false;
bool am_minimapfloors = false;
unsigned am_overlay = 0;
bool am_minimap = false;
unsigned am_minimapstyle = AMMS_Framed;
int am_minimapsize = 6;
int am_minimapzoom = 6;
int am_minimaptranslucency = 0;
bool am_pause = true;
bool am_showratios = false;
bool am_show[NUM_AMKINDS][NUM_AMSHOW];
unsigned am_enemymode[NUM_AMKINDS] = { AMSM_Off, AMSM_Off, AMSM_Off };
unsigned am_objectmode[NUM_AMKINDS] = { AMSM_Explored, AMSM_Explored, AMSM_Explored };
unsigned am_decorationmode[NUM_AMKINDS] = { AMDM_Off, AMDM_Off, AMDM_Off };
bool am_needsrecalc = false;

static bool AM_RevealAll()
{
	return am_cheat || am_revealmap == AMRM_Full;
}

static bool AM_ShouldPauseGame()
{
	return am_pause && Net::InitVars.mode == Net::MODE_SinglePlayer;
}

void AM_ChangeResolution()
{
	if(StatusBar == NULL)
	{
		am_needsrecalc = true;
		return;
	}

	am_needsrecalc = false;

	unsigned int y = statusbary1;
	unsigned int height = statusbary2 - statusbary1;

	AM_Main.CalculateDimensions(0, y, screenWidth, height);
	AM_Overlay.CalculateDimensions(viewscreenx, viewscreeny, viewwidth, viewheight);

	// Minimap is a square in the top left corner of the view
	const unsigned int margin = MAX<unsigned int>(viewheight/50, 2);
	const unsigned int percent = 8 + clamp(am_minimapsize, 0, AM_MINIMAPSIZE_MAX)*92/AM_MINIMAPSIZE_MAX;
	const unsigned int size = MIN<unsigned int>(viewheight*percent/100, MIN<unsigned int>(viewheight, viewwidth) - margin*2);
	AM_Mini.CalculateDimensions(viewscreenx + margin, viewscreeny + margin, size, size);
}

void AM_CheckKeys()
{
	if(control[ConsolePlayer].ambuttonstate[bt_zoomin])
	{
		AM_Overlay.SetScale(FRACUNIT*135/128, true);
		AM_Main.SetScale(FRACUNIT*135/128, true);
	}
	if(control[ConsolePlayer].ambuttonstate[bt_zoomout])
	{
		AM_Overlay.SetScale(FRACUNIT*122/128, true);
		AM_Main.SetScale(FRACUNIT*122/128, true);
	}

	if(AM_ShouldPauseGame())
	{
		const fixed PAN_AMOUNT = FixedDiv(FRACUNIT*10, AM_Main.GetScreenScale());
		const fixed PAN_ANALOG_MULTIPLIER = PAN_AMOUNT/100;
		fixed panx = 0, pany = 0;

		if(control[ConsolePlayer].ambuttonstate[bt_panleft])
			panx += PAN_AMOUNT;
		if(control[ConsolePlayer].ambuttonstate[bt_panright])
			panx -= PAN_AMOUNT;
		if(control[ConsolePlayer].ambuttonstate[bt_panup])
			pany += PAN_AMOUNT;
		if(control[ConsolePlayer].ambuttonstate[bt_pandown])
			pany -= PAN_AMOUNT;

		if(control[ConsolePlayer].controlpanx != 0)
			panx += control[ConsolePlayer].controlpanx * (PAN_ANALOG_MULTIPLIER * (panxadjustment+1));

		if(control[ConsolePlayer].controlpany != 0)
			pany += control[ConsolePlayer].controlpany * (PAN_ANALOG_MULTIPLIER * (panxadjustment+1));

		AM_Main.SetPanning(panx, pany, true);
	}
}

static unsigned int AM_ShowFlags(unsigned int kind)
{
	unsigned int flags = 0;
	for(unsigned int i = 0;i < NUM_AMSHOW;++i)
	{
		if(am_show[kind][i])
			flags |= AutoMap::AMF_ShowFirst<<i;
	}

	if(am_enemymode[kind] != AMSM_Off)
		flags |= AutoMap::AMF_ShowFirst<<AMS_Enemies;
	if(am_enemymode[kind] == AMSM_InSight)
		flags |= AutoMap::AMF_EnemiesInSight;
	else if(am_enemymode[kind] == AMSM_All)
		flags |= AutoMap::AMF_EnemiesAll;

	if(am_objectmode[kind] == AMSM_All)
		flags |= AutoMap::AMF_ObjectsAll;

	if(am_decorationmode[kind] != AMDM_Off)
		flags |= AutoMap::AMF_ShowFirst<<AMS_Obstacles;
	if(am_decorationmode[kind] == AMDM_All)
		flags |= AutoMap::AMF_ShowFirst<<AMS_Decorations;
	return flags;
}

void AM_UpdateFlags()
{
	// Release pause if we appear to have unset am_pause
	if(!am_pause && (Paused&2)) Paused &= ~2;

	unsigned int flags = AM_ShowFlags(AMK_Full);
	unsigned int ovFlags = AutoMap::AMF_Overlay|AM_ShowFlags(AMK_Overlay);
	unsigned int miniFlags = AutoMap::AMF_Minimap|AM_ShowFlags(AMK_Minimap);

	if(am_rotate) flags |= AutoMap::AMF_Rotate;
	if(am_drawtexturedwalls) flags |= AutoMap::AMF_DrawTexturedWalls;
	if(am_drawfloors) flags |= AutoMap::AMF_DrawFloor;
	if(am_showratios) flags |= AutoMap::AMF_DispRatios;

	if(am_overlayrotate) ovFlags |= AutoMap::AMF_Rotate;
	if(am_overlaytextured) ovFlags |= AutoMap::AMF_DrawTexturedWalls;
	if(am_overlayfloors) ovFlags |= AutoMap::AMF_DrawFloor;

	if(am_minimaprotate) miniFlags |= AutoMap::AMF_Rotate;
	if(am_minimaptextured) miniFlags |= AutoMap::AMF_DrawTexturedWalls;
	if(am_minimapfloors) miniFlags |= AutoMap::AMF_DrawFloor;

	if(!am_minimap && automap == AMA_Mini)
		automap = AMA_Off;

	AM_Main.SetFlags(~flags, false);
	AM_Overlay.SetFlags(~ovFlags, false);
	AM_Mini.SetFlags(~miniFlags, false);
	AM_Main.SetFlags(flags|AutoMap::AMF_DispInfo|AutoMap::AMF_ShowThings, true);
	AM_Overlay.SetFlags(ovFlags, true);
	AM_Mini.SetFlags(miniFlags, true);

	// Spread the minimap zoom levels evenly over the range of the zoom keys
	AM_Mini.SetScale(fixed(FRACUNIT/50*pow(50., double(clamp(am_minimapzoom, 0, AM_MINIMAPZOOM_MAX))/AM_MINIMAPZOOM_MAX)), false);
}

static bool AM_ModeEnabled(unsigned int mode)
{
	switch(mode)
	{
		case AMA_Mini: return am_minimap;
		case AMA_Overlay: return am_overlay != AMO_Off;
		case AMA_Normal: return am_overlay != AMO_On;
		default: return true;
	}
}

// Cycles Off -> Minimap -> Overlay -> Normal skipping disabled modes
void AM_Toggle()
{
	do
	{
		if(++automap > AMA_Normal)
			automap = AMA_Off;
	}
	while(!AM_ModeEnabled(automap));

	if(automap == AMA_Off)
	{
		AM_Main.SetPanning(0, 0, false);
		DrawPlayScreen();
	}

	if(AM_ShouldPauseGame())
	{
		if(automap == AMA_Normal)
			Paused |= 2;
		else
			Paused &= ~2;
	}
}

// Culling table
static const struct
{
	const struct
	{
		const unsigned short Min, Max;
	} X, Y;
} AM_MinMax[4] = {
	{ {3, 1}, {0, 2} },
	{ {2, 0}, {3, 1} },
	{ {1, 3}, {2, 0} },
	{ {0, 2}, {1, 3} }
};

// #FF9200
struct AMVectorPoint
{
	fixed X, Y;
};
static const AMVectorPoint AM_Arrow[] =
{
	{0, -FRACUNIT/2},
	{FRACUNIT/2, 0},
	{FRACUNIT/4, 0},
	{FRACUNIT/4, FRACUNIT*7/16},
	{-FRACUNIT/4, FRACUNIT*7/16},
	{-FRACUNIT/4, 0},
	{-FRACUNIT/2, 0},
	{0, -FRACUNIT/2},
};

AutoMap::AutoMap(unsigned int flags, fixed scale) :
	fullRefresh(true), amFlags(flags),
	ampanx(0), ampany(0)
{
	amangle = 0;
	minmaxSel = 0;
	amsin = 0;
	amcos = FRACUNIT;
	absscale = scale;
	rottable[0][0] = 1.0;
	rottable[0][1] = 0.0;
	rottable[1][0] = 1.0;
	rottable[1][1] = 1.0;
}

AutoMap::~AutoMap()
{
}

static int AM_Lighten(int rgb)
{
	return MAKERGB((RPART(rgb)*3 + 255*2)/5, (GPART(rgb)*3 + 255*2)/5, (BPART(rgb)*3 + 255*2)/5);
}

static int AM_Gray(int rgb)
{
	const int gray = (((RPART(rgb)*77 + GPART(rgb)*150 + BPART(rgb)*29)>>8) + 255)/2;
	return MAKERGB(gray, gray, gray);
}

// Floors need to stand out from the background
static int AM_FloorColor(int floor, int background)
{
	if(floor != background)
		return floor;
	return MAKERGB(RPART(floor) + (255-RPART(floor))*3/40, GPART(floor) + (255-GPART(floor))*3/40, BPART(floor) + (255-BPART(floor))*3/40);
}

// About as dark as revealed floor textures
static int AM_DarkGray(int rgb)
{
	const int gray = ((RPART(rgb)*77 + GPART(rgb)*150 + BPART(rgb)*29)>>8)*3/8;
	return MAKERGB(gray, gray, gray);
}

static FDynamicColormap *AM_GetGrayLight()
{
	static FDynamicColormap *grayLight = NULL;
	if(!grayLight)
		grayLight = GetSpecialLights(NormalLight.Color, NormalLight.Fade, 255);
	return grayLight;
}

void AutoMap::CalculateDimensions(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	fullRefresh = true;
	amsizex = width;
	amsizey = height;
	amx = x;
	amy = y;

	// Since the simple polygon fill function seems to be off by one in the y
	// direction, lets shift this up!
	--amy;

	// TODO: Find a better spot for this
	const int floorColor = AM_FloorColor(gameinfo.automap.FloorColor, gameinfo.automap.Background);
	ArrowColor = gameinfo.automap.YourColor;
	BackgroundColor = gameinfo.automap.Background;
	FloorColor = floorColor;
	WallColor = gameinfo.automap.WallColor;
	DoorColor = gameinfo.automap.DoorColor;
	SecretWallColor = 0xFF00FF;
	RevealedFloorColor = AM_DarkGray(floorColor);
	RevealedWallColor = AM_Gray(gameinfo.automap.WallColor);
	RevealedDoorColor = AM_Gray(gameinfo.automap.DoorColor);
	RevealedWallLineColor = AM_Lighten(gameinfo.automap.WallColor);
	RevealedDoorLineColor = AM_Lighten(gameinfo.automap.DoorColor);
}

// Sutherland–Hodgman algorithm
void AutoMap::ClipTile(TArray<FVector2> &points) const
{
	for(int i = 0;i < 4;++i)
	{
		TArray<FVector2> input(points);
		points.Clear();
		FVector2 *s = &input[0];
		for(unsigned j = input.Size();j-- > 0;)
		{
			FVector2 &e = input[j];

			bool Ein, Sin;
			switch(i)
			{
				default:
				case 0: // Left
					Ein = e.X > amx;
					Sin = s->X > amx;
					break;
				case 1: // Top
					Ein = e.Y > amy;
					Sin = s->Y > amy;
					break;
				case 2: // Right
					Ein = e.X < amx+amsizex;
					Sin = s->X < amx+amsizex;
					break;
				case 3: // Bottom
					Ein = e.Y < amy+amsizey;
					Sin = s->Y < amy+amsizey;
					break;
			}
			if(Ein)
			{
				if(!Sin)
					points.Push(GetClipIntersection(e, *s, i));
				points.Push(e);
			}
			else if(Sin)
				points.Push(GetClipIntersection(e, *s, i));
			s = &e;
		}
	}
}

// Spots reachable without moving pushwalls so revealing doesn't give away secrets
static TArray<BYTE> AM_Reachable;

static bool AM_IsReachable(MapSpot spot)
{
	return AM_Reachable[spot->GetY()*map->GetHeader().width + spot->GetX()] != 0;
}

static void AM_FindReachable()
{
	static const MapTile::Side sides[4] = { MapTile::East, MapTile::North, MapTile::West, MapTile::South };
	const unsigned int width = map->GetHeader().width;

	AM_Reachable.Resize(width*map->GetHeader().height);
	memset(&AM_Reachable[0], 0, AM_Reachable.Size());

	TArray<MapSpot> todo;
	MapSpot spot = map->GetSpot(players[ConsolePlayer].mo->tilex, players[ConsolePlayer].mo->tiley, 0);
	AM_Reachable[spot->GetY()*width + spot->GetX()] = 1;
	todo.Push(spot);
	while(todo.Pop(spot))
	{
		for(unsigned int i = 0;i < 4;++i)
		{
			MapSpot adjacent = spot->GetAdjacent(sides[i]);
			if(!adjacent || AM_IsReachable(adjacent))
				continue;

			if(adjacent->tile && !(adjacent->tile->offsetHorizontal || adjacent->tile->offsetVertical))
				continue;

			AM_Reachable[adjacent->GetY()*width + adjacent->GetX()] = 1;
			todo.Push(adjacent);
		}
	}
}

static bool AM_HasReachableSide(MapSpot spot)
{
	static const MapTile::Side sides[4] = { MapTile::East, MapTile::North, MapTile::West, MapTile::South };
	for(unsigned int i = 0;i < 4;++i)
	{
		MapSpot adjacent = spot->GetAdjacent(sides[i]);
		if(adjacent && !adjacent->tile && AM_IsReachable(adjacent))
			return true;
	}
	return false;
}

static bool AM_IsSecretWall(MapSpot spot)
{
	if(!spot->tile || spot->pushAmount)
		return false;

	for(unsigned int i = 0;i < spot->triggers.Size();++i)
	{
		const MapTrigger &trig = spot->triggers[i];
		if(trig.active && (trig.action == Specials::Pushwall_Move || trig.action == Specials::Pushwall_MoveNoStop))
			return true;
	}
	return false;
}

bool __AF_A_GiveExtraMan(AActor *, AActor *, const Frame * const, const CallArguments &, struct ActionResult *);

static unsigned int AM_GetItemCategory(AActor *actor)
{
	static const struct
	{
		const char *name;
		unsigned int what;
	} itemClasses[] = {
		{ "Key", AMS_Keys },
		{ "KeyGiver", AMS_Keys },
		{ "Weapon", AMS_Weapons },
		{ "Ammo", AMS_Ammo },
		{ "BackpackItem", AMS_Ammo },
		{ "Health", AMS_Health },
		{ "ScoreItem", AMS_Treasure },
		{ "ExtraLifeItem", AMS_Treasure }
	};
	static const ClassDef *classes[countof(itemClasses)];
	static bool classesResolved = false;
	if(!classesResolved)
	{
		for(unsigned int i = 0;i < countof(itemClasses);++i)
			classes[i] = ClassDef::FindClass(itemClasses[i].name);
		classesResolved = true;
	}

	for(unsigned int i = 0;i < countof(itemClasses);++i)
	{
		if(classes[i] && actor->IsKindOf(classes[i]))
			return itemClasses[i].what;
	}

	// Extra lives are CustomInventory calling A_GiveExtraMan
	const Frame *frame = actor->FindState(NAME_Pickup);
	for(unsigned int i = 0;frame && i < 16;++i, frame = frame->next)
	{
		if(frame->action.pointer == __AF_A_GiveExtraMan)
			return AMS_ExtraLives;
	}
	return AMS_None;
}

static unsigned int AM_GetThingCategory(AActor *actor)
{
	if(actor->flags & FL_ISMONSTER)
	{
		if((actor->flags & FL_SHOOTABLE) && actor->health > 0)
			return AMS_Enemies;

		return actor->sprite != SPR_NONE ? AMS_Corpses : AMS_None;
	}
	else if(actor->IsKindOf(NATIVE_CLASS(Inventory)))
	{
		if(static_cast<AInventory *>(actor)->owner)
			return AMS_None;
		return AM_GetItemCategory(actor);
	}
	else if(!(actor->flags & FL_MISSILE) && actor->GetClass()->Meta.GetMetaInt(AMETA_Damage, -1) >= 0)
	{
		// Things which hurt on touch (Pac-Man ghosts)
		return AMS_Enemies;
	}

	if(actor->player || (actor->flags & FL_MISSILE) || actor->sprite == SPR_NONE)
		return AMS_None;
	return (actor->flags & FL_SOLID) ? AMS_Obstacles : AMS_Decorations;
}

bool AutoMap::IsShown(unsigned int what, MapSpot spot) const
{
	if(!(amFlags & (AMF_ShowFirst<<what)))
		return false;
	if(AM_RevealAll() || (amFlags & (what == AMS_Enemies ? AMF_EnemiesAll : AMF_ObjectsAll)))
		return true;

	if(what == AMS_Enemies && (amFlags & AMF_EnemiesInSight))
	{
		if(spot->visible)
			return true;

		// Actors stick out into the spots around them
		static const MapTile::Side sides[4] = { MapTile::East, MapTile::North, MapTile::West, MapTile::South };
		for(unsigned int i = 0;i < 4;++i)
		{
			MapSpot adjacent = spot->GetAdjacent(sides[i]);
			if(adjacent && adjacent->visible && !adjacent->tile)
				return true;
		}
		return false;
	}

	return (spot->amFlags & AM_Visible) != 0;
}

static const float AM_MINIMAP_DIM = 0.85f;

static TArray<BYTE> AM_MinimapUnderlay;

static bool AM_ClipToScreen(int &x, int &y, int &w, int &h)
{
	if(x < 0) { w += x; x = 0; }
	if(y < 0) { h += y; y = 0; }
	w = MIN(w, screen->GetWidth() - x);
	h = MIN(h, screen->GetHeight() - y);
	return w > 0 && h > 0;
}

static void AM_SaveUnderlay(int x, int y, int w, int h)
{
	if(!AM_ClipToScreen(x, y, w, h))
		return;

	AM_MinimapUnderlay.Resize(w*h);
	const BYTE *src = screen->GetBuffer() + y*screen->GetPitch() + x;
	for(int row = 0;row < h;++row, src += screen->GetPitch())
		memcpy(&AM_MinimapUnderlay[row*w], src, w);
}

// Only allow shades of either color or grays since the closest palette color
// to a mix can have an unrelated hue
static const BYTE *AM_GetBlendTable(int alpha)
{
	static TArray<BYTE> shades[256];
	static TArray<BYTE> grays;
	if(grays.Size() == 0)
	{
		for(int c = 0;c < 256;++c)
		{
			bool seen[256] = {false};
			for(int i = 0;i < NUMCOLORMAPS;++i)
			{
				const BYTE shade = NormalLight.Maps[i*256 + c];
				if(!seen[shade])
				{
					seen[shade] = true;
					shades[c].Push(shade);
				}
			}

			const PalEntry &color = GPalette.BaseColors[c];
			if(color.r == color.g && color.g == color.b)
				grays.Push(c);
		}
	}

	static TArray<BYTE> table;
	static int tableAlpha = -1;
	if(tableAlpha != alpha)
	{
		table.Resize(256*256);
		for(int fg = 0;fg < 256;++fg)
		{
			const PalEntry &f = GPalette.BaseColors[fg];
			for(int bg = 0;bg < 256;++bg)
			{
				const PalEntry &b = GPalette.BaseColors[bg];
				const int r = (f.r*alpha + b.r*(64-alpha))>>6;
				const int g = (f.g*alpha + b.g*(64-alpha))>>6;
				const int bl = (f.b*alpha + b.b*(64-alpha))>>6;

				const TArray<BYTE> *candidates[3] = { &shades[fg], &shades[bg], &grays };
				int bestDist = INT_MAX;
				BYTE best = fg;
				for(unsigned int set = 0;set < 3;++set)
				{
					for(unsigned int i = 0;i < candidates[set]->Size();++i)
					{
						const BYTE c = (*candidates[set])[i];
						const PalEntry &p = GPalette.BaseColors[c];
						const int dist = (p.r-r)*(p.r-r) + (p.g-g)*(p.g-g) + (p.b-bl)*(p.b-bl);
						if(dist < bestDist)
						{
							bestDist = dist;
							best = c;
						}
					}
				}
				table[(fg<<8)|bg] = best;
			}
		}
		tableAlpha = alpha;
	}
	return &table[0];
}

static void AM_BlendUnderlay(int x, int y, int w, int h, int alpha)
{
	if(!AM_ClipToScreen(x, y, w, h) || AM_MinimapUnderlay.Size() != (unsigned)(w*h))
		return;

	const BYTE *table = AM_GetBlendTable(alpha);
	const BYTE *under = &AM_MinimapUnderlay[0];
	BYTE *dest = screen->GetBuffer() + y*screen->GetPitch() + x;
	for(int row = 0;row < h;++row, dest += screen->GetPitch() - w)
	{
		for(int col = 0;col < w;++col, ++dest, ++under)
		{
			if(*dest != *under)
				*dest = table[(*dest<<8)|*under];
		}
	}
}

struct AMPWall
{
public:
	TArray<FVector2> points;
	FTextureID texid;
	float shiftx, shifty;
};
void AutoMap::Draw()
{
	TArray<AMPWall> pwalls;
	TArray<FVector2> points;
	const unsigned int mapwidth = map->GetHeader().width;
	const unsigned int mapheight = map->GetHeader().height;

	const fixed scale = GetScreenScale();

	const bool floating = (amFlags & AMF_Minimap) && am_minimapstyle == AMMS_Floating;
	const BYTE *shadow = NULL;
	const int alpha = (amFlags & AMF_Minimap) ? 64 - clamp(am_minimaptranslucency, 0, AM_MINIMAPTRANSLUCENCY_MAX)*58/AM_MINIMAPTRANSLUCENCY_MAX : 64;
	if(amFlags & AMF_Minimap)
	{
		if(alpha < 64)
			AM_SaveUnderlay(amx, amy+1, amsizex, amsizey);

		if(floating)
			shadow = NormalLight.Maps + xs_RoundToInt(AM_MINIMAP_DIM*(NUMCOLORMAPS-1))*256;
		else
			screen->Dim(0, AM_MINIMAP_DIM, amx, amy+1, amsizex, amsizey);
	}
	else if(!(amFlags & AMF_Overlay))
		screen->Clear(amx, amy+1, amx+amsizex, amy+amsizey+1, BackgroundColor.palcolor, BackgroundColor.color);

	const fixed playerx = players[ConsolePlayer].mo->x;
	const fixed playery = players[ConsolePlayer].mo->y;

	if(fullRefresh || amangle != ((amFlags & AMF_Rotate) ? players[ConsolePlayer].mo->angle-ANGLE_90 : 0))
	{
		amangle = (amFlags & AMF_Rotate) ? players[ConsolePlayer].mo->angle-ANGLE_90 : 0;
		minmaxSel = amangle/ANGLE_90;
		amsin = finesine[amangle>>ANGLETOFINESHIFT];
		amcos = finecosine[amangle>>ANGLETOFINESHIFT];

		// For rotating the tiles, this table includes the point offsets based on the current scale
		rottable[0][0] = FIXED2FLOAT(FixedMul(scale, amcos)); rottable[0][1] = FIXED2FLOAT(FixedMul(scale, amsin));
		rottable[1][0] = FIXED2FLOAT(FixedMul(scale, amcos) - FixedMul(scale, amsin)); rottable[1][1] = FIXED2FLOAT(FixedMul(scale, amsin) + FixedMul(scale, amcos));

		fullRefresh = false;
	}

	const fixed pany = FixedMul(ampany, amcos) - FixedMul(ampanx, amsin);
	const fixed panx = FixedMul(ampany, amsin) + FixedMul(ampanx, amcos);
	const fixed ofsx = playerx - panx;
	const fixed ofsy = playery - pany;

	const double originx = (amx+amsizex/2) - (FIXED2FLOAT(FixedMul(FixedMul(scale, ofsx&0xFFFF), amcos) - FixedMul(FixedMul(scale, ofsy&0xFFFF), amsin)));
	const double originy = (amy+amsizey/2) - (FIXED2FLOAT(FixedMul(FixedMul(scale, ofsx&0xFFFF), amsin) + FixedMul(FixedMul(scale, ofsy&0xFFFF), amcos)));
	const double origTexScale = FIXED2FLOAT(scale>>6);

	const bool revealAll = AM_RevealAll();
	if(am_revealmap == AMRM_Gray || am_revealmap == AMRM_Lines)
		AM_FindReachable();

	for(unsigned int my = 0;my < mapheight;++my)
	{
		MapSpot spot = map->GetSpot(0, my, 0);
		for(unsigned int mx = 0;mx < mapwidth;++mx, ++spot)
		{
			const bool secretWall = IsShown(AMS_SecretWalls, spot) && AM_IsSecretWall(spot);
			const bool revealedOnly = !((spot->amFlags & AM_Visible) || revealAll || gamestate.fullmap || secretWall);
			if((revealedOnly && (am_revealmap == AMRM_Off || !(spot->tile ? AM_HasReachableSide(spot) : AM_IsReachable(spot)))) ||
				((amFlags & AMF_Overlay) && (spot->amFlags & AM_DontOverlay)))
				continue;

			if(revealedOnly && am_revealmap == AMRM_Lines)
			{
				if(spot->tile)
				{
					const bool door = spot->tile->offsetHorizontal || spot->tile->offsetVertical;
					DrawTileEdges(spot, FixedMul((mx<<FRACBITS)-ofsx, scale), FixedMul((my<<FRACBITS)-ofsy, scale), door ? RevealedDoorLineColor : RevealedWallLineColor);
				}
				continue;
			}

			if(TransformTile(spot, FixedMul((mx<<FRACBITS)-ofsx, scale), FixedMul((my<<FRACBITS)-ofsy, scale), points))
			{
				double texScale = origTexScale;

				if(shadow)
					screen->FillSimplePoly(NULL, &points[0], points.Size(), originx, originy, texScale, texScale, ~amangle, &NormalLight, 256, 0, 0, shadow);

				FTexture *tex;
				Color *color = NULL;
				int brightness;
				if(spot->tile && !spot->pushAmount && !spot->pushReceptor)
				{
					brightness = 256;
					if(secretWall)
					{
						tex = NULL;
						color = &SecretWallColor;
					}
					// Textures get too dark when grayed out
					else if((amFlags & AMF_DrawTexturedWalls) && !revealedOnly)
					{
						if(spot->tile->overhead.isValid())
							tex = TexMan(spot->tile->overhead);
						else if(spot->tile->offsetHorizontal)
							tex = TexMan(spot->texture[MapTile::North]);
						else
							tex = TexMan(spot->texture[MapTile::East]);
					}
					else
					{
						tex = NULL;
						if(spot->tile->offsetHorizontal || spot->tile->offsetVertical)
							color = revealedOnly ? &RevealedDoorColor : &DoorColor;
						else
							color = revealedOnly ? &RevealedWallColor : &WallColor;
					}
				}
				else if(spot->sector && (amFlags & (AMF_Overlay|AMF_DrawFloor)) != AMF_Overlay)
				{
					if(amFlags & AMF_DrawFloor)
					{
						// Graying out alone doesn't set gray floor textures apart
						if(revealedOnly)
							brightness = 128;
						else
							brightness = (amFlags & (AMF_Overlay|AMF_Minimap)) ? 192 : 256;
						tex = TexMan(spot->sector->texture[MapSector::Floor]);
					}
					else
					{
						brightness = 256;
						tex = NULL;
						if(FloorColor.color != BackgroundColor.color)
							color = revealedOnly ? &RevealedFloorColor : &FloorColor;
					}
				}
				else
				{
					tex = NULL;
				}

				if(tex)
				{
					// As a special case, since Noah's Ark stores the Automap
					// graphics in the TILE8, we need to override the scaling.
					if(tex->UseType == FTexture::TEX_FontChar)
						texScale *= 8;
					screen->FillSimplePoly(tex, &points[0], points.Size(), originx, originy, texScale, texScale, ~amangle, revealedOnly ? AM_GetGrayLight() : &NormalLight, brightness);
				}
				else if(color)
					screen->FillSimplePoly(NULL, &points[0], points.Size(), originx, originy, texScale, texScale, ~amangle, &NormalLight, brightness, color->palcolor, color->color);
			}

			// We need to check this even if the origin tile isn't visible since
			// the destination spot may be!
			if(spot->pushAmount)
			{
				fixed tx = (mx<<FRACBITS), ty = my<<FRACBITS;
				switch(spot->pushDirection)
				{
					default:
					case MapTile::East:
						tx += (spot->pushAmount<<10);
						break;
					case MapTile::West:
						tx -= (spot->pushAmount<<10);
						break;
					case MapTile::South:
						ty += (spot->pushAmount<<10);
						break;
					case MapTile::North:
						ty -= (spot->pushAmount<<10);
						break;
				}
				if(TransformTile(spot, FixedMul(tx-ofsx, scale), FixedMul(ty-ofsy, scale), points))
				{
					AMPWall pwall;
					pwall.points = points;
					pwall.texid = spot->tile->overhead.isValid() ? spot->tile->overhead : spot->texture[0];
					pwall.shiftx = (float)(FIXED2FLOAT(FixedMul(FixedMul(scale, tx&0xFFFF), amcos) - FixedMul(FixedMul(scale, ty&0xFFFF), amsin)));
					pwall.shifty = (float)(FIXED2FLOAT(FixedMul(FixedMul(scale, tx&0xFFFF), amsin) + FixedMul(FixedMul(scale, ty&0xFFFF), amcos)));
					pwalls.Push(pwall);
				}
			}
		}
	}

	for(unsigned int pw = pwalls.Size();pw-- > 0;)
	{
		AMPWall &pwall = pwalls[pw];
		if((amFlags & AMF_DrawTexturedWalls))
		{
			double texScale = origTexScale;
			FTexture *tex = TexMan(pwall.texid);
			if(tex)
			{
				// Noah's ark TILE8
				if(tex->UseType == FTexture::TEX_FontChar)
					texScale *= 8;
				screen->FillSimplePoly(tex, &pwall.points[0], pwall.points.Size(), originx + pwall.shiftx, originy + pwall.shifty, texScale, texScale, ~amangle, &NormalLight, 256);
			}
		}
		else
			screen->FillSimplePoly(NULL, &pwall.points[0], pwall.points.Size(), originx + pwall.shiftx, originy + pwall.shifty, origTexScale, origTexScale, ~amangle, &NormalLight, 256, WallColor.palcolor, WallColor.color);
	}

	DrawVector(AM_Arrow, 8, FixedMul(playerx - ofsx, scale), FixedMul(playery - ofsy, scale), scale, (amFlags & AMF_Rotate) ? 0 : ANGLE_90-players[ConsolePlayer].mo->angle, ArrowColor);

	const bool cheatThings = (amFlags & AMF_ShowThings) && (revealAll || gamestate.fullmap);
	if(cheatThings || (amFlags & AMF_ShowMask))
	{
		// Corpses and decorations first so dropped items are drawn on top
		for(unsigned int pass = 0;pass < 2;++pass)
		{
			for(AActor::Iterator iter = AActor::GetIterator();iter.Next();)
			{
				const unsigned int what = (amFlags & AMF_ShowMask) ? AM_GetThingCategory(iter) : (unsigned int)AMS_None;
				const bool underneath = what == AMS_Corpses || what == AMS_Obstacles || what == AMS_Decorations;
				if(underneath != (pass == 0))
					continue;

				if(what != AMS_None && IsShown(what, map->GetSpot(iter->tilex, iter->tiley, 0)))
					DrawActor(iter, FixedMul(iter->x - ofsx, scale), FixedMul(iter->y - ofsy, scale), scale, true);
				else if(cheatThings && (revealAll || (iter->flags & FL_PLOTONAUTOMAP)))
					DrawActor(iter, FixedMul(iter->x - ofsx, scale), FixedMul(iter->y - ofsy, scale), scale);
			}
		}
	}

	if((amFlags & AMF_Minimap) && !floating)
	{
		const int border = MAX(amsizey/100, 1);
		screen->Clear(amx, amy+1, amx+amsizex, amy+1+border, BackgroundColor.palcolor, BackgroundColor.color);
		screen->Clear(amx, amy+amsizey+1-border, amx+amsizex, amy+amsizey+1, BackgroundColor.palcolor, BackgroundColor.color);
		screen->Clear(amx, amy+1+border, amx+border, amy+amsizey+1-border, BackgroundColor.palcolor, BackgroundColor.color);
		screen->Clear(amx+amsizex-border, amy+1+border, amx+amsizex, amy+amsizey+1-border, BackgroundColor.palcolor, BackgroundColor.color);
	}

	if(alpha < 64)
		AM_BlendUnderlay(amx, amy+1, amsizex, amsizey, alpha);

	DrawStats();
}

struct AMSpriteBounds
{
	int left, top, right, bottom;
};

static AMSpriteBounds AM_GetSpriteBounds(FTexture *tex)
{
	static TMap<FTexture *, AMSpriteBounds> cache;
	if(AMSpriteBounds *cached = cache.CheckKey(tex))
		return *cached;

	AMSpriteBounds bounds = { tex->GetWidth(), tex->GetHeight(), 0, 0 };
	for(int x = 0;x < tex->GetWidth();++x)
	{
		const FTexture::Span *spans;
		tex->GetColumn(x, &spans);
		for(;spans->Length;++spans)
		{
			bounds.left = MIN(bounds.left, x);
			bounds.right = MAX(bounds.right, x+1);
			bounds.top = MIN<int>(bounds.top, spans->TopOffset);
			bounds.bottom = MAX<int>(bounds.bottom, spans->TopOffset + spans->Length);
		}
	}

	if(bounds.right <= bounds.left)
	{
		bounds.left = bounds.top = 0;
		bounds.right = tex->GetWidth();
		bounds.bottom = tex->GetHeight();
	}

	cache[tex] = bounds;
	return bounds;
}

void AutoMap::DrawActor(AActor *actor, fixed x, fixed y, fixed scale, bool marked)
{
	const bool fitToTile = marked && am_thingsize == AMTS_Tile;
	const fixed spriteScale = marked && !fitToTile ? scale*2 : scale;

	{
		fixed tmp = ((FixedMul(x, amcos) - FixedMul(y, amsin) + (amsizex<<(FRACBITS-1)))>>FRACBITS) + amx;
		y = ((FixedMul(x, amsin) + FixedMul(y, amcos) + (amsizey<<(FRACBITS-1)))>>FRACBITS) + amy;
		x = tmp;

		int adiameter = FixedMul(actor->radius, spriteScale)>>(FRACBITS-1);
		int aheight = spriteScale>>FRACBITS;

		// Cull out actors that are obviously out of bounds
		if(x + adiameter < amx || x - adiameter >= amx+amsizex || y + aheight < amy || y - aheight >= amy+amsizey)
			return;
	}

	bool flip;
	FTexture *tex;
	if(actor->overheadIcon.isValid())
	{
		tex = actor->sprite != SPR_NONE ? TexMan(actor->overheadIcon) : NULL;
		flip = false;
	}
	else
		tex = R_GetAMSprite(actor, amangle, flip);

	if(!tex)
		return;

	if(fitToTile && tex->UseType != FTexture::TEX_FontChar)
	{
		const AMSpriteBounds bounds = AM_GetSpriteBounds(tex);
		const double xscale = tex->GetScaledWidthDouble()/tex->GetWidth();
		const double yscale = tex->GetScaledHeightDouble()/tex->GetHeight();
		const double fit = FIXED2FLOAT(scale)/MAX((bounds.right - bounds.left)*xscale, (bounds.bottom - bounds.top)*yscale);
		const double centerx = (bounds.left + bounds.right)*xscale/2;
		// DTA_FlipX doesn't mirror the offset
		screen->DrawTexture(tex, x, y,
			DTA_DestWidthF, tex->GetScaledWidthDouble()*fit,
			DTA_DestHeightF, tex->GetScaledHeightDouble()*fit,
			DTA_LeftOffsetF, flip ? tex->GetScaledWidthDouble() - centerx : centerx,
			DTA_TopOffsetF, (bounds.top + bounds.bottom)*yscale/2,
			DTA_ClipLeft, amx,
			DTA_ClipRight, amx+amsizex,
			DTA_ClipTop, amy,
			DTA_ClipBottom, amy+amsizey,
			DTA_FlipX, flip,
			TAG_DONE);
	}
	else if(tex->UseType != FTexture::TEX_FontChar)
	{
		double width = tex->GetScaledWidthDouble()*FIXED2FLOAT(spriteScale>>6);
		double height = tex->GetScaledHeightDouble()*FIXED2FLOAT(spriteScale>>6);
		screen->DrawTexture(tex, x, y,
			DTA_DestWidthF, width,
			DTA_DestHeightF, height,
			DTA_ClipLeft, amx,
			DTA_ClipRight, amx+amsizex,
			DTA_ClipTop, amy,
			DTA_ClipBottom, amy+amsizey,
			DTA_FlipX, flip,
			TAG_DONE);
	}
	else
	{
		screen->DrawTexture(tex, x - (scale>>(FRACBITS+1)), y - (scale>>(FRACBITS+1)),
			DTA_DestWidthF, FIXED2FLOAT(scale),
			DTA_DestHeightF, FIXED2FLOAT(scale),
			DTA_ClipLeft, amx,
			DTA_ClipRight, amx+amsizex,
			DTA_ClipTop, amy,
			DTA_ClipBottom, amy+amsizey,
			DTA_FlipX, flip,
			TAG_DONE);
	}
}

void AutoMap::DrawClippedLine(int x0, int y0, int x1, int y1, int palcolor, uint32 realcolor) const
{
	// Let us make an assumption that point 0 is always left of point 1
	if(x0 > x1)
	{
		swapvalues(x0, x1);
		swapvalues(y0, y1);
	}

	const int dx = x1 - x0, dy = y1 - y0;
	int ymin = MIN(y0, y1);
	int ymax = MAX(y0, y1);
	const bool inc = ymax == y0;
	bool clipped = false;

	// There will inevitably be precision issues so we may need to run the
	// clipper a few times.
	do
	{
		// Trivial culling
		if(x1 < amx || ymax < amy || x0 >= amx+amsizex || ymin >= amy+amsizey)
			return;

		if(x0 < amx) // Clip left
		{
			clipped = true;
			y0 += dy*(amx-x0)/dx;
			x0 = amx;
		}
		if(x1 >= amx+amsizex) // Clip right
		{
			clipped = true;
			y1 += dy*(amx+amsizex-1-x1)/dx;
			x1 = amx+amsizex-1;
		}
		if(ymin < amy) // Clip top
		{
			clipped = true;
			if(inc)
			{
				x1 += dx*(amy-y1)/dy;
				y1 = amy;
			}
			else
			{
				x0 += dx*(amy-y0)/dy;
				y0 = amy;
			}
		}
		if(ymax >= amy+amsizey) // Clip bottom
		{
			clipped = true;
			if(inc)
			{
				x0 += dx*(amy+amsizey-1-y0)/dy;
				y0 = amy+amsizey-1;
			}
			else
			{
				x1 += dx*(amy+amsizey-1-y1)/dy;
				y1 = amy+amsizey-1;
			}
		}

		if(!clipped)
			break;
		clipped = false;

		// Fix ymin/max for the next iteration
		if(inc)
		{
			ymin = y1;
			ymax = y0;
		}
		else
		{
			ymin = y0;
			ymax = y1;
		}
	}
	while(true);

	screen->DrawLine(x0, y0+1, x1, y1+1, palcolor, realcolor);
}

void AutoMap::DrawTileEdges(MapSpot spot, fixed x, fixed y, const Color &c) const
{
	const fixed rotx = FixedMul(x, amcos) - FixedMul(y, amsin) + (amsizex<<(FRACBITS-1));
	const fixed roty = FixedMul(x, amsin) + FixedMul(y, amcos) + (amsizey<<(FRACBITS-1));
	const double x1 = amx + FIXED2FLOAT(rotx);
	const double y1 = amy + FIXED2FLOAT(roty);

	// Same order as TransformTile so the edges are north, east, south, west
	const double cornerx[4] = { x1, x1 + rottable[0][0], x1 + rottable[1][0], x1 - rottable[0][1] };
	const double cornery[4] = { y1, y1 + rottable[0][1], y1 + rottable[1][1], y1 + rottable[0][0] };
	static const MapTile::Side sides[4] = { MapTile::North, MapTile::East, MapTile::South, MapTile::West };
	const double centerx = (cornerx[0] + cornerx[2])/2;
	const double centery = (cornery[0] + cornery[2])/2;

	const int thickness = MAX<int>(1, xs_RoundToInt(FIXED2FLOAT(GetScreenScale())/14));

	for(unsigned int i = 0;i < 4;++i)
	{
		MapSpot adjacent = spot->GetAdjacent(sides[i]);
		if(!adjacent || adjacent->tile || !AM_IsReachable(adjacent))
			continue;

		const unsigned int j = (i+1)&3;
		int x0 = xs_RoundToInt(cornerx[i]), y0 = xs_RoundToInt(cornery[i]);
		int x1 = xs_RoundToInt(cornerx[j]), y1 = xs_RoundToInt(cornery[j]);

		int stepx = 0, stepy = 0;
		if(abs(x1 - x0) >= abs(y1 - y0))
			stepy = centery > (y0 + y1)/2 ? 1 : -1;
		else
			stepx = centerx > (x0 + x1)/2 ? 1 : -1;

		for(int t = 0;t < thickness;++t)
			DrawClippedLine(x0 + stepx*t, y0 + stepy*t, x1 + stepx*t, y1 + stepy*t, c.palcolor, c.color);
	}
}

void AutoMap::DrawStats() const
{
	if(!(amFlags & (AMF_DispInfo|AMF_DispRatios)))
		return;

	FString statString;
	unsigned int infHeight = 0;

	if(amFlags & AMF_DispInfo)
	{
		infHeight = SmallFont->GetHeight()+2;
		screen->Dim(GPalette.BlackIndex, 0.5f, 0, 0, screenWidth, infHeight*CleanYfac);

		screen->DrawText(SmallFont, gameinfo.automap.FontColor, 2*CleanXfac, CleanYfac, levelInfo->GetName(map),
			DTA_CleanNoMove, true,
			TAG_DONE);

		unsigned int seconds = gamestate.TimeCount/70;
		statString.Format("%02d:%02d:%02d", seconds/3600, (seconds%3600)/60, seconds%60);
		screen->DrawText(SmallFont, gameinfo.automap.FontColor,
			screenWidth - (SmallFont->GetCharWidth('0')*6 + SmallFont->GetCharWidth(':')*2 + 2)*CleanXfac, CleanYfac,
			statString,
			DTA_CleanNoMove, true,
			TAG_DONE);
	}

	if(amFlags & AMF_DispRatios)
	{
		statString.Format("K: %d/%d\nS: %d/%d\nT: %d/%d",
			gamestate.killcount, gamestate.killtotal,
			gamestate.secretcount, gamestate.secrettotal,
			gamestate.treasurecount, gamestate.treasuretotal);

		word sw, sh;
		VW_MeasurePropString(SmallFont, statString, sw, sh);
		screen->Dim(GPalette.BlackIndex, 0.5f, 0, infHeight*CleanYfac, (sw+3)*CleanXfac, (sh+2)*CleanYfac);

		screen->DrawText(SmallFont, gameinfo.automap.FontColor, 2*CleanXfac, infHeight*CleanYfac, statString,
			DTA_CleanNoMove, true,
			TAG_DONE);
	}
}

void AutoMap::DrawVector(const AMVectorPoint *points, unsigned int numPoints, fixed x, fixed y, fixed scale, angle_t angle, const Color &c) const
{
	int x1, y1, x2, y2;

	fixed tmp = ((FixedMul(x, amcos) - FixedMul(y, amsin) + (amsizex<<(FRACBITS-1)))>>FRACBITS) + amx;
	y = ((FixedMul(x, amsin) + FixedMul(y, amcos) + (amsizey<<(FRACBITS-1)))>>FRACBITS) + amy;
	x = tmp;

	x1 = FixedMul(points[numPoints-1].X, scale)>>FRACBITS;
	y1 = FixedMul(points[numPoints-1].Y, scale)>>FRACBITS;
	if(angle)
	{
		const fixed rsin = finesine[angle>>ANGLETOFINESHIFT];
		const fixed rcos = finecosine[angle>>ANGLETOFINESHIFT];
		int tmp;
		tmp = FixedMul(x1, rcos) - FixedMul(y1, rsin);
		y1 = FixedMul(x1, rsin) + FixedMul(y1, rcos);
		x1 = tmp;

		for(unsigned int i = numPoints-1;i-- > 0;x1 = x2, y1 = y2)
		{
			x2 = FixedMul(points[i].X, scale)>>FRACBITS;
			y2 = FixedMul(points[i].Y, scale)>>FRACBITS;

			tmp = FixedMul(x2, rcos) - FixedMul(y2, rsin);
			y2 = FixedMul(x2, rsin) + FixedMul(y2, rcos);
			x2 = tmp;

			DrawClippedLine(x + x1, y + y1, x + x2, y + y2, c.palcolor, c.color);
		}
	}
	else
	{
		for(unsigned int i = numPoints-1;i-- > 0;x1 = x2, y1 = y2)
		{
			x2 = FixedMul(points[i].X, scale)>>FRACBITS;
			y2 = FixedMul(points[i].Y, scale)>>FRACBITS;

			DrawClippedLine(x + x1, y + y1, x + x2, y + y2, c.palcolor, c.color);
		}
	}
}

FVector2 AutoMap::GetClipIntersection(const FVector2 &p1, const FVector2 &p2, unsigned edge) const
{
	// If we are clipping a vertical line, it should be because our angle is
	// 90 degrees and it's clip against the top or bottom
	if((edge&1) == 1 && ((amangle>>ANGLETOFINESHIFT)&(ANG90-1)) == 0)
	{
		if(edge == 1)
			return FVector2(p1.X, amy);
		return FVector2(p1.X, amy+amsizey);
	}
	else
	{
		const float A = p2.Y - p1.Y;
		const float B = p1.X - p2.X;
		const float C = A*p1.X + B*p1.Y;
		switch(edge)
		{
			default:
			case 0: // Left
				return FVector2(amx, (C - A*amx)/B);
			case 1: // Top
				return FVector2((C - B*amy)/A, amy);
			case 2: // Right
				return FVector2(amx+amsizex, (C - A*(amx+amsizex))/B);
			case 3: // Bottom
				return FVector2((C - B*(amy+amsizey))/A, amy+amsizey);
		}
	}
}

// Returns size of a tile in pixels
fixed AutoMap::GetScreenScale() const
{
	// Some magic, min scale is approximately small enough so that a rotated automap will fit on screen (22/32 ~ 1/sqrt(2))
	const fixed minscale = ((screenHeight*22)<<(FRACBITS-5))/map->GetHeader().height;
	return minscale + FixedMul(absscale, (screenHeight<<(FRACBITS-3)) - minscale);
}

void AutoMap::SetFlags(unsigned int flags, bool set)
{
	if(set)
		amFlags |= flags;
	else
		amFlags &= ~flags;
}

void AutoMap::SetPanning(fixed x, fixed y, bool relative)
{
	if(relative)
	{
		// TODO: Make panning absolute instead of relative to the player so this isn't weird
		fixed posx, posy, maxx, maxy;
		if(amangle)
		{
			const int sizex = map->GetHeader().width;
			const int sizey = map->GetHeader().height;
			maxx = abs(sizex*amcos) + abs(sizey*amsin);
			maxy = abs(sizex*amsin) + abs(sizey*amcos);
			posx = players[ConsolePlayer].mo->x - (sizex<<(FRACBITS-1));
			posy = players[ConsolePlayer].mo->y - (sizey<<(FRACBITS-1));
			fixed tmp = (FixedMul(posx, amcos) - FixedMul(posy, amsin)) + (maxx>>1);
			posy = (FixedMul(posy, amcos) + FixedMul(posx, amsin)) + (maxy>>1);
			posx = tmp;
		}
		else
		{
			maxx = map->GetHeader().width<<FRACBITS;
			maxy = map->GetHeader().height<<FRACBITS;
			posx = players[ConsolePlayer].mo->x;
			posy = players[ConsolePlayer].mo->y;
		}
		ampanx = clamp<fixed>(ampanx+x, posx - maxx, posx);
		ampany = clamp<fixed>(ampany+y, posy - maxy, posy);
	}
	else
	{
		ampanx = x;
		ampany = y;
	}
}

void AutoMap::SetScale(fixed scale, bool relative)
{
	if(relative)
		absscale = FixedMul(absscale, scale);
	else
		absscale = scale;
	absscale = clamp<fixed>(absscale, FRACUNIT/50, FRACUNIT);

	fullRefresh = true;
}

bool AutoMap::TransformTile(MapSpot spot, fixed x, fixed y, TArray<FVector2> &points) const
{
	fixed rotx = FixedMul(x, amcos) - FixedMul(y, amsin) + (amsizex<<(FRACBITS-1));
	fixed roty = FixedMul(x, amsin) + FixedMul(y, amcos) + (amsizey<<(FRACBITS-1));
	double x1 = amx + FIXED2FLOAT(rotx);
	double y1 = amy + FIXED2FLOAT(roty);
	points.Resize(4);
	points[0] = FVector2(x1, y1);
	points[1] = FVector2(x1 + rottable[0][0], y1 + rottable[0][1]);
	points[2] = FVector2(x1 + rottable[1][0], y1 + rottable[1][1]);
	points[3] = FVector2(x1 - rottable[0][1], y1 + rottable[0][0]);


	const float xmin = points[AM_MinMax[minmaxSel].X.Min].X;
	const float xmax = points[AM_MinMax[minmaxSel].X.Max].X;
	const float ymin = points[AM_MinMax[minmaxSel].Y.Min].Y;
	const float ymax = points[AM_MinMax[minmaxSel].Y.Max].Y;

	// Cull out tiles which never touch the automap area
	if((xmax < amx || xmin > amx+amsizex) || (ymax < amy || ymin > amy+amsizey))
		return false;

	// If the tile is partially in the automap area, clip
	if((ymax > amy+amsizey || ymin < amy) || (xmax > amx+amsizex || xmin < amx))
		ClipTile(points);
	return true;
}

void BasicOverhead()
{
	if(am_needsrecalc)
		AM_ChangeResolution();

	switch(automap)
	{
		case AMA_Mini:
			AM_Mini.Draw();
			break;
		case AMA_Overlay:
			AM_Overlay.Draw();
			break;
		case AMA_Normal:
		{
			int oldviewsize = viewsize;
			viewsize = 20;
			DrawPlayScreen();
			viewsize = oldviewsize;
			AM_Main.Draw();
			break;
		}
		default: break;
	}
}
