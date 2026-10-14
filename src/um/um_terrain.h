// Headless replacement for BaseHeightMapRenderObjClass: only the queries GameLogic's terrain code needs.
// Bodies of getHeightMapHeight / isClearLineOfSight / isCliffCell are copied from W3DDevice/GameClient/BaseHeightMap.cpp (EA, GPL-3).
#pragma once
#include "PreRTS.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
class W3DTerrainLogic;
class UmTerrain
{
public:
	UmTerrain() : m_map(NULL) {}
	WorldHeightMap *getMap() const { return m_map; }
	void setMap(WorldHeightMap *m) { m_map = m; }
	UnsignedByte getClipHeight(Int x, Int y) const
	{
		Int xextent = m_map->getXExtent() - 1, yextent = m_map->getYExtent() - 1;
		if (x < 0) x = 0; else if (x > xextent) x = xextent;
		if (y < 0) y = 0; else if (y > yextent) y = yextent;
		return m_map->getDataPtr()[x + y*m_map->getXExtent()];
	}
	Real getHeightMapHeight(Real x, Real y, Coord3D *normal) const;
	Bool isClearLineOfSight(const Coord3D &pos, const Coord3D &posOther) const;
	Bool isCliffCell(Real x, Real y);
	void loadRoadsAndBridges(W3DTerrainLogic *, Bool) {}   // TODO(bridges): needs bridge model data, see MODLOG
	Real getMaxHeight(void) const { return (Real)((pow(256.0, sizeof(UnsignedByte))-1.0)*MAP_HEIGHT_SCALE); }  // as in BaseHeightMap::initHeightData
	WorldHeightMap *m_map;
};
extern UmTerrain *TheTerrainRenderObject;
