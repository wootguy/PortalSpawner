#include "Portal.h"
#include "main.h"

using namespace std;

Portal::Portal() {}

Portal::Portal(CBaseEntity* portalEnt, CBasePlayer* creator, Vector portalAngles, int portalType)
{
	owner = getPlayerUniqueId(creator->edict());
	ent = portalEnt;
	angles = portalAngles;
	type = portalType;
	init();
}

Portal::Portal(CBaseEntity* portalEnt, string creator, Vector portalAngles, int portalType)
{
	owner = creator;
	ent = portalEnt;
	angles = portalAngles;
	type = portalType;
	init();
}

void Portal::init()
{
	exitSpeed = EXIT_MATCH_INPUT;
	rotateMode = ROTATE_NO;
	enabled = true;
	target = -1;
	canAddBeams = true;
	creationTime = g_engfuncs.pfnTime();
}

void Portal::disable()
{
	enabled = false;
	if (ent.IsValid()) {
		CBaseEntity* e = ent;
		e->pev->framerate = 0;
		e->pev->renderamt = 100;
	}
}

void Portal::enable()
{
	enabled = true;
	if (ent.IsValid()) {
		CBaseEntity* e = ent;
		e->pev->framerate = 10;
		e->pev->renderamt = 200;
	}

	killBeam();
}

void Portal::killBeam()
{
	beamTarget = EHandle();
	println("OKIE KILL THE BEAM");
}

void Portal::drawBeamTo(CBaseEntity* otherEnt)
{
	beamTarget = otherEnt;
	println("DRAW BEAM TO IT %d", beamTarget.IsValid());
}

string Portal::serialize()
{
	if (!ent.IsValid())
		return "";

	CBaseEntity* e = ent;
	string writeDat;

	writeDat += "{";
	writeDat += "\nx=" + to_string(e->pev->origin.x);
	writeDat += "\ny=" + to_string(e->pev->origin.y);
	writeDat += "\nz=" + to_string(e->pev->origin.z);
	writeDat += "\nax=" + to_string(e->pev->angles.x);
	writeDat += "\nay=" + to_string(e->pev->angles.y);
	writeDat += "\naz=" + to_string(e->pev->angles.z);
	writeDat += "\nowner=" + owner;
	writeDat += "\ntime=" + to_string(creationTime);
	writeDat += "\ntype=" + to_string(type);
	writeDat += "\nspeed=" + to_string(exitSpeed);
	writeDat += "\nrot=" + to_string(rotateMode);
	writeDat += "\ntarget=" + to_string(target);
	writeDat += "\n}\n";

	return writeDat;
}