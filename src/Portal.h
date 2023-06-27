#pragma once
#include "meta_init.h"
#include "misc_utils.h"
#include "private_api.h"

class Portal {
public:
	EHandle ent; // portal env_sprite
	std::string owner; // steamid or username of the player who created this portal
	float creationTime = 0; // server time that portal was created
	Vector angles = Vector(0,0,0);
	int type = 0;
	int exitSpeed = 0;
	int rotateMode = 0;
	bool enabled = true;
	int target = -1; // -1 = any portal
	bool canAddBeams = true; // prevents beam spam
	EHandle beamTarget; // used for identifying targets (not the same thing as the special effects beams on teleports)

	Portal();

	Portal(CBaseEntity* portalEnt, CBasePlayer* creator, Vector portalAngles, int portalType);

	Portal(CBaseEntity* portalEnt, std::string creator, Vector portalAngles, int portalType);

	void init();

	void disable();

	void enable();

	void killBeam();

	void drawBeamTo(CBaseEntity* otherEnt);

	std::string serialize();
};