#include "main.h"
#include "meta_init.h"
#include "misc_utils.h"
#include "meta_utils.h"
#include "private_api.h"
#include <set>
#include <map>
#include "Scheduler.h"
#include <vector>
#include "StartSound.h"
#include "meta_helper.h"
#include "TextMenu.h"
#include "temp_ents.h"

// TODO:
// was $f_lastPsTele ever used in other plugins?
// less laggy pushable tp logic

// Description of plugin
plugin_info_t Plugin_info = {
	META_INTERFACE_VERSION,	// ifvers
	"PortalSpawner",	// name
	"1.0",	// version
	__DATE__,	// date
	"w00tguy",	// author
	"https://github.com/wootguy/",	// url
	"PORTAL",	// logtag, all caps please
	PT_ANYTIME,	// (when) loadable
	PT_ANYPAUSE,	// (when) unloadable
};

// persistent-ish player data, organized by steam-id or username if on a LAN server, values are @PlayerState
vector<PlayerState> g_player_states;

const char* entrance_sprite = "sprites/enter1.spr";
const char* exit_sprite = "sprites/exit1.spr";
const char* bi_sprite = "sprites/b-tele1.spr";
const char* beam_sprite = "sprites/laserbeam.spr";

const char* spawn_sound = "debris/beamstart7.wav";
const char* remove_sound = "debris/beamstart11.wav";
const char* portal_targetname = "pspawner_asplugin_portal";
const char* portal_save_path = "svencoop/addons/metamod/store/PortalSpawner/";

vector<Portal> portals;

int MAX_MAP_PORTALS = 32;
float portalOffset = 18.0f;
float portalTouchRadius = 40.0f;
float portalEditRadius = 150.0f;
bool portalsEnabled = true;
bool super_spawn = false;

PlayerState& getPlayerState(edict_t* plr) {
	return g_player_states[ENTINDEX(plr)];
}

PlayerState& getPlayerState(CBasePlayer* plr) {
	return getPlayerState(plr->edict());
}

set<string> projectiles;
void initLists()
{
	projectiles.insert("grenade");
	projectiles.insert("playerhornet");
	projectiles.insert("rpg_rocket");
	projectiles.insert("bolt");
	projectiles.insert("grappletongue");
	projectiles.insert("displacer_portal");
	projectiles.insert("weaponbox");
	projectiles.insert("player");
	projectiles.insert("shock_beam");
	projectiles.insert("gonomespit");
	projectiles.insert("voltigoreshock");
	projectiles.insert("controller_energy_ball");
	projectiles.insert("controller_head_ball");
	projectiles.insert("hornet");
	projectiles.insert("squidspit");
	projectiles.insert("bmortar");
	projectiles.insert("garg_stomp");
	projectiles.insert("pitdronespike");
	projectiles.insert("hvr_rocket");
	projectiles.insert("kingpin_plasma_ball");
}

CBaseEntity* createPortal(Vector origin, int portalType, string soundFile, float zoffset, bool spawnSound = true)
{
	// create sprite at this location
	string spriteName = portal_targetname;
	map<string, string> keyvalues;
	keyvalues["targetname"] = spriteName;
	keyvalues["framerate"] = portalsEnabled ? "10" : "0";
	keyvalues["scale"] = "1";
	keyvalues["rendermode"] = "5";
	keyvalues["renderfx"] = "0";
	keyvalues["renderamt"] = portalsEnabled ? "200" : "100";
	keyvalues["spawnflags"] = "1";
	keyvalues["made_with_portal_spawner"] = "very_yes"; // TODO: This doesn't work. Learn about custom keyvalues
	keyvalues["origin"] = "" + to_string(origin.x) + " " + to_string(origin.y) + " " + to_string(origin.z + zoffset);

	switch (portalType)
	{
	case PORTAL_ENTER:
		keyvalues["model"] = string(entrance_sprite);
		keyvalues["rendercolor"] = "255 255 255";
		break;
	case PORTAL_EXIT:
		keyvalues["model"] = string(exit_sprite);
		keyvalues["rendercolor"] = "255 255 255";
		break;
	case PORTAL_BIDIR:
		keyvalues["model"] = string(bi_sprite);
		keyvalues["rendercolor"] = "255 255 255";
		break;
	}

	CBaseEntity* portal = (CBaseEntity*)CreateEntity("env_sprite", keyvalues, true)->pvPrivateData;

	// play sound so you feel like you've accomplished something
	if (spawnSound)
		PlaySound(portal->edict(), CHAN_STATIC, soundFile, 1.0f, 1.0f, 0, 100, 0, true, portal->pev->origin);

	return portal;
}

void removeAllPortals() {
	for (int i = 0; i < MAX_MAP_PORTALS; i++) {
		edict_t* portal = g_engfuncs.pfnFindEntityByString(NULL, "targetname", portal_targetname);
		if (isValidFindEnt(portal))
			g_engfuncs.pfnRemoveEntity(portal);
	}

	edict_t* beam = NULL;
	do {
		beam = g_engfuncs.pfnFindEntityByString(NULL, "targetname", "portalspawner_beam");
		if (isValidFindEnt(beam))
			g_engfuncs.pfnRemoveEntity(beam);
	} while (isValidFindEnt(beam));

	portals.resize(0);
}

bool isTooCloseToPortal(CBasePlayer* plr, int ignoreIdx)
{
	for (int i = 0; i < portals.size(); i++)
	{
		if (int(i) == ignoreIdx)
			continue;
		if (portals[i].ent.IsValid()) {
			CBaseEntity* e = portals[i].ent.GetEntity();
			float distance = Vector(e->pev->origin - plr->pev->origin).Length();
			if (distance <= portalTouchRadius * 2)
				return true;
		}

	}
	return false;
}

bool canAddPortal(CBasePlayer* plr) {
	if (int(portals.size()) == MAX_MAP_PORTALS)
	{
		ClientPrint(plr->edict(), HUD_PRINTTALK, ("Max portals reached (" + to_string(MAX_MAP_PORTALS) + ")\n").c_str());
		return false;
	}
	if (isTooCloseToPortal(plr, -1))
	{
		ClientPrint(plr->edict(), HUD_PRINTTALK, "Move farther away before spawning another portal.\n");
		return false;
	}
	return true;
}

void removePortal(int idx)
{
	if (idx < 0)
		return;
	if (idx >= int(portals.size())) {
		println("Bad portal index passed to removePortal(): " + idx);
		return;
	}
	portals[idx].killBeam();
	if (portals[idx].ent.IsValid()) {
		edict_t* e = portals[idx].ent;
		PlaySound(e, CHAN_STATIC, remove_sound, 0.5f, 1.0f, 0, 80);
		g_engfuncs.pfnRemoveEntity(e);
	}
	portals.erase(portals.begin() + idx);

	// adjust editing indexes for other players
	for (int i = 1; i <= 32; i++)
	{
		edict_t* plr = INDEXENT(i);

		PlayerState& state = g_player_states[i];
		if (state.editing == -1)
			continue;
		if (state.editing == idx) {
			state.editing = -1; // sorry, you can't edit this one anymore
			g_Scheduler.SetTimeout(openPortalMenu, 0, EHandle(plr));
		}
		else if (state.editing > idx) {
			state.editing -= 1;
		}
	}

	// update portal chains
	for (int i = 0; i < portals.size(); i++)
	{
		if (portals[i].target == idx)
			portals[i].target = -1;
		else if (portals[i].target > idx)
			portals[i].target -= 1;
	}
}

void blinkPortal(Portal* portal)
{
	if (portal && portal->ent.IsValid())
	{
		CBaseEntity* ent = portal->ent;
		if (ent->pev->rendercolor.x != 64)
			ent->pev->rendercolor = Vector(64, 64, 255);
		else
			ent->pev->rendercolor = Vector(255, 255, 255);
	}
}

int getClosestPortalIdx(CBasePlayer* plr)
{
	float minDist = 9999.0f;
	int closestPortal = -1;
	for (int i = 0; i < portals.size(); i++)
	{
		if (!portals[i].ent.IsValid())
			continue;
		CBaseEntity* ent = portals[i].ent;
		float distance = Vector(ent->pev->origin - plr->pev->origin).Length();
		if (distance <= portalEditRadius && distance < minDist)
		{
			minDist = distance;
			closestPortal = i;
		}
	}
	return closestPortal;
}

// Get the last portal entrance this player created
// set maxTime to a non-zero value to look further back in time
// filtering by PORTAL_BIDIR will get all portal types (so, no filtering is done)
int getLastPlacedPortalIdx(CBasePlayer* plr, float maxTime, int filterType, int skipId)
{
	string steamId = getPlayerUniqueId(plr->edict());

	int lastPortalIdx = -1;
	float mostRecent = 0;
	for (int i = 0; i < portals.size(); i++)
	{
		if (int(i) == skipId)
			continue;
		if (maxTime > 0 && portals[i].creationTime >= maxTime)
			continue;
		//if (steamId != portals[i].owner)
		//	continue;
		if (filterType != portals[i].type)
			continue;
		if (portals[i].creationTime > mostRecent)
		{
			mostRecent = portals[i].creationTime;
			lastPortalIdx = i;
		}
	}
	// no previous portals found. Get the most recent instead
	if (lastPortalIdx == -1)
	{
		for (int i = 0; i < portals.size(); i++)
		{
			if (int(i) == skipId)
				continue;
			//if (steamId != portals[i].owner)
			//	continue;
			if (filterType != portals[i].type)
				continue;
			if (portals[i].creationTime > mostRecent)
			{
				mostRecent = portals[i].creationTime;
				lastPortalIdx = i;
			}
		}
	}

	return lastPortalIdx;
}

bool isPlayerEditingPortal(int idx)
{
	for (int i = 1; i <= 32; i++) {
		PlayerState& state = g_player_states[i];
		if (state.editing == idx)
			return true;
	}
	return false;
}

void add_portal_action(CBasePlayer* plr, string action)
{
	PlayerState& state = getPlayerState(plr);
	Vector angles = plr->pev->v_angle;
	if (canAddPortal(plr)) {
		//print("Angles: " + angles.x + " " + angles.y + " " + angles.z);

		for (int x = 0; x < 8; x++)
		{
			for (int y = 0; y < 4; y++)
			{
				Vector offset = Vector(x * portalTouchRadius * 2, y * portalTouchRadius * 2, 0);
				int portalType;
				if (action == "add-enter")
					portalType = PORTAL_ENTER;
				else if (action == "add-exit")
					portalType = PORTAL_EXIT;
				else if (action == "add-bi")
					portalType = PORTAL_BIDIR;
				string spawnSound = super_spawn ? "" : spawn_sound;

				CBaseEntity* portalEnt = createPortal(plr->pev->origin, portalType, spawnSound, portalOffset);
				portalEnt->pev->origin = portalEnt->pev->origin + offset;
				Portal portal = Portal(portalEnt, plr, angles, portalType);
				portals.push_back(portal);

				state.touchingPortal = portals.size() - 1;
				if (!super_spawn)
					break;
			}
			if (!super_spawn)
				break;
		}
	}
}

void portalMenuCallback(TextMenu* menu, edict_t* plr, int itemNumber, TextMenuItem& item) {
	PlayerState& state = getPlayerState(plr);
	/*
	if (item is null)
	{
		if (state.editing != -1 and state.editing < int(portals.size()))
		{
			if (portalsEnabled)
				portals[state.editing].enable();
		}
		state.editing = -1;
		state.removeConfirm = false;
		state.deletePortal = -1;
		state.linkLast = -1;
		state.menuPage = 0;
		return;
	}
	*/
	if (!isValidPlayer(plr)) {
		return;
	}
	string action = item.data;


	if (action == "toggle-portals")
	{
		portalsEnabled = !portalsEnabled;
		if (portalsEnabled)
			ClientPrintAll(HUD_PRINTTALK, (string(STRING(plr->v.netname)) + " enabled all portals\n").c_str());
		else
			ClientPrintAll(HUD_PRINTTALK, (string(STRING(plr->v.netname)) + " disabled all portals\n").c_str());
		for (int i = 0; i < portals.size(); i++)
		{
			if (portalsEnabled && !isPlayerEditingPortal(i))
				portals[i].enable();
			else
				portals[i].disable();
		}
	}
	else if (action == "next-page")
	{
		state.menuPage += 1;
	}
	else if (action == "previous-page")
	{
		state.menuPage -= 1;
	}
	else if (action == "edit")
	{
		int closestPortal = getClosestPortalIdx((CBasePlayer*)plr->pvPrivateData);
		if (closestPortal == -1)
		{
			ClientPrint(plr, HUD_PRINTTALK, "Stand closer to a portal to edit.\n");
		}
		else {
			bool canEdit = true;
			for (int i = 1; i <= 32; i++) {
				PlayerState& s = g_player_states[i];
				if (s.editing == closestPortal)
				{
					edict_t* p = INDEXENT(i);
					ClientPrint(plr, HUD_PRINTTALK, (string("Wait for ") + STRING(p->v.netname) + " to finish editing this portal\n").c_str());
					canEdit = false;
					break;
				}
			}

			if (canEdit)
			{
				state.editing = closestPortal;
				Portal& p = portals[state.editing];
				p.disable();
				p.killBeam();
				if (p.target != -1) {
					p.drawBeamTo(portals[p.target].ent);
				}
			}
		}
	}
	else if (action == "edit-link-confirm")
	{
		if (state.editing == state.linkLast)
			ClientPrint(plr, HUD_PRINTTALK, "You can't link a portal to itself!\n");
		else
		{
			portals[state.editing].target = state.linkLast;
			portals[state.linkLast].target = state.editing;
			state.linkLast = -1;
		}

		Portal& p = portals[state.editing];
		p.killBeam();
		if (p.target != -1) {
			p.drawBeamTo(portals[p.target].ent);
		}
	}
	else if (action == "edit-link-last")
	{
		int desiredPortalType = PORTAL_EXIT;
		if (portals[state.editing].type == PORTAL_EXIT)
			desiredPortalType = PORTAL_ENTER;
		if (portals[state.editing].type == PORTAL_BIDIR)
			desiredPortalType = PORTAL_BIDIR;

		if (state.linkLast != -1) { // user wants one further back
			float maxTime = portals[state.linkLast].creationTime; // so getLast doesn't return this one again

			int oldLink = state.linkLast;
			state.linkLast = getLastPlacedPortalIdx((CBasePlayer*)plr->pvPrivateData, maxTime, desiredPortalType, state.editing);
			if (state.linkLast == -1 || state.linkLast == oldLink) // couldn't find an earlier portal
			{
				state.linkLast = oldLink;
				if (desiredPortalType == PORTAL_EXIT)
					ClientPrint(plr, HUD_PRINTTALK, "No other portal exits found.\n");
				else if (desiredPortalType == PORTAL_ENTER)
					ClientPrint(plr, HUD_PRINTTALK, "No other portal entrances found.\n");
				else
					ClientPrint(plr, HUD_PRINTTALK, "No other bidirectional portals found.\n");
			}
		}
		else {
			float maxTime = portals[state.editing].creationTime; // so getLast doesn't return this one

			state.linkLast = getLastPlacedPortalIdx((CBasePlayer*)plr->pvPrivateData, maxTime, desiredPortalType, state.editing);
			if (state.linkLast == -1) // couldn't find an earlier portal
			{
				state.linkLast = state.editing;
			}
		}
		if (state.linkLast != -1)
			portals[state.editing].drawBeamTo(portals[state.linkLast].ent);
	}
	else if (action == "edit-link-closest")
	{
		int desiredPortalType = PORTAL_EXIT;
		if (portals[state.editing].type == PORTAL_EXIT)
			desiredPortalType = PORTAL_ENTER;
		if (portals[state.editing].type == PORTAL_BIDIR)
			desiredPortalType = PORTAL_BIDIR;

		int closestPortal = getClosestPortalIdx((CBasePlayer*)plr->pvPrivateData);
		if (closestPortal == -1)
		{
			ClientPrint(plr, HUD_PRINTTALK, "Stand closer to a portal to set it as the target.\n");
		}
		else {
			if (state.editing == closestPortal)
				ClientPrint(plr, HUD_PRINTTALK, "You can't link a portal to itself!\n");
			else
			{
				if (portals[state.editing].type == PORTAL_ENTER && portals[closestPortal].type != PORTAL_EXIT ||
					portals[state.editing].type == PORTAL_EXIT && portals[closestPortal].type != PORTAL_ENTER)
					ClientPrint(plr, HUD_PRINTTALK, "Portal entrances can only be linked to portal exits\n");
				else if (portals[state.editing].type == PORTAL_BIDIR && portals[closestPortal].type != PORTAL_BIDIR)
					ClientPrint(plr, HUD_PRINTTALK, "Bidirectional portals can only be linked to other bidirectional portals\n");
				else
				{
					state.linkLast = closestPortal;
					portals[state.editing].drawBeamTo(portals[state.linkLast].ent);
				}
			}
		}
	}
	else if (action == "edit-link-random")
	{
		if (portals[state.editing].target != -1)
			portals[portals[state.editing].target].target = -1;
		portals[state.editing].killBeam();
		portals[state.editing].target = -1;
		state.linkLast = -1;
	}
	else if (action == "edit-link-cancel")
	{
		state.linkLast = -1;

		Portal& p = portals[state.editing];
		p.killBeam();
		if (p.target != -1) {
			p.drawBeamTo(portals[p.target].ent);
		}
	}
	else if (action == "delete")
	{
		state.deletePortal = getClosestPortalIdx((CBasePlayer*)plr->pvPrivateData);
		if (state.deletePortal == -1)
			ClientPrint(plr, HUD_PRINTTALK, "Stand closer to the portal you want to delete\n");
	}
	else if (action == "delete-yes")
	{
		removePortal(state.deletePortal);
		state.deletePortal = -1;
	}
	else if (action == "delete-no")
	{
		state.deletePortal = -1;
	}
	else if (action == "edit-cancel")
	{
		if (portalsEnabled)
			portals[state.editing].enable();
		state.editing = -1;
		state.linkLast = -1;
	}
	else if (action == "edit-speed")
	{
		portals[state.editing].exitSpeed = (portals[state.editing].exitSpeed + 1) % EXIT_SPEEDS;
	}
	else if (action == "edit-rotate")
	{
		portals[state.editing].rotateMode = (portals[state.editing].rotateMode + 1) % ROTATE_MODES;
	}
	else if (action == "edit-position")
	{
		if (isTooCloseToPortal((CBasePlayer*)plr->pvPrivateData, state.editing))
			ClientPrint(plr, HUD_PRINTTALK, "Can't move here - you're too close to another portal.\n");
		else if (portals[state.editing].ent.IsValid())
		{
			CBaseEntity* ent = portals[state.editing].ent;
			ent->pev->origin = plr->v.origin + Vector(0, 0, portalOffset);
		}
	}
	else if (action == "edit-angles")
	{
		portals[state.editing].angles = plr->v.v_angle;
	}
	else if (action == "kill-all-cancel") {
		state.removeConfirm = false;
	}
	else if (action == "kill-all") {
		if (state.removeConfirm) {
			removeAllPortals();
			ClientPrintAll(HUD_PRINTTALK, (string(STRING(plr->v.netname)) + " removed all the portals\n").c_str());
			state.removeConfirm = false;
			PlaySound(INDEXENT(0), CHAN_STATIC, remove_sound, 1.0f, 0, 0, 100);
		}
		else {
			state.removeConfirm = true;
		}
	}
	else if (action == "kill-all-owner") {
		string steamId = getPlayerUniqueId(plr);

		for (int i = 0; i < portals.size(); i++)
		{
			if (portals[i].owner == steamId)
			{
				removePortal(i);
				i--;
			}
		}
		ClientPrint(plr, HUD_PRINTTALK, "All of your portals were removed\n");
		state.removeConfirm = false;
	}
	else if (action.find("kill-all-owner-") == 0)
	{
		string owner = action.substr(string("kill-all-owner-").size());
		edict_t* ownerPlr = getPlayerByUniqueId(owner);
		string playername = ownerPlr ? string(STRING(ownerPlr->v.netname)) : "<unknown>";
		bool unowned = ownerPlr == NULL;

		for (int i = 0; i < portals.size(); i++)
		{
			if ((!unowned && portals[i].owner == owner) || (unowned && getPlayerByUniqueId(portals[i].owner) == NULL))
			{
				removePortal(i);
				i--;
			}
		}

		if (unowned)
			ClientPrint(plr, HUD_PRINTTALK, "All unowned portals were removed\n");
		else
			ClientPrint(plr, HUD_PRINTTALK, (string("All of ") + playername + "'s portals were removed\n").c_str());
		if (ownerPlr)
			ClientPrint(ownerPlr, HUD_PRINTTALK, (string("All of your portals were removed by ") + STRING(plr->v.netname) + "\n").c_str());
		state.removeConfirm = false;

	}
	else if (action == "save")
	{
		if (saveMapPortals())
			ClientPrintAll(HUD_PRINTTALK, (string(STRING(plr->v.netname)) + " saved map portals\n").c_str());
		else
			ClientPrint(plr, HUD_PRINTTALK, ("Save failed. Make sure the save path exists: " + string(portal_save_path) + "\n").c_str());
	}
	else if (action == "load")
	{
		ClientPrintAll(HUD_PRINTTALK, (string(STRING(plr->v.netname)) + " reloaded map portals\n").c_str());
		loadMapPortals();
	}
	else if (action.find("add-") == 0)
	{
		add_portal_action((CBasePlayer*)plr->pvPrivateData, action);
	}

	g_Scheduler.SetTimeout(openPortalMenu, 0, EHandle(plr));
}

int getRandomPortal(int portalType, int currentPortalId)
{
	vector<int> portalIds;
	for (int i = 0; i < portals.size(); i++) {
		if (portals[i].enabled && int(i) != currentPortalId &&
			portals[i].type == portalType && portals[i].ent.IsValid() && portals[i].target == -1)
			portalIds.push_back(i);
	}

	if (portalIds.size() > 0)
	{
		return portalIds[RANDOM_LONG(0, portalIds.size() - 1)];
	}
	return -1;
}

// adjust vector length
Vector resizeVector(Vector v, float length)
{
	float d = length / sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
	v.x *= d;
	v.y *= d;
	v.z *= d;
	return v;
}

void removeEnt(EHandle entHandle)
{
	g_engfuncs.pfnRemoveEntity(entHandle);
}

void allowMoreBeamsOnPortal(Portal* portal)
{
	if (portal)
		portal->canAddBeams = true;
}

int fxid = 0;

void checkMonsterTouching(EHandle entHandle)
{
	if (entHandle.IsValid())
	{
		CBaseEntity* ent = entHandle;
		if (ent->pev->oldbuttons - 1 >= 0) {
			// Check if player is still touching this portal
			Portal& p = portals[ent->pev->oldbuttons - 1];
			if (p.ent.IsValid()) {
				CBaseEntity* portalEnt = p.ent;
				Vector portalOri = portalEnt->pev->origin;
				portalOri.z -= 36; // account for monster origin on floor;
				float distance = Vector(portalOri - ent->pev->origin).Length();
				if (distance > portalTouchRadius*2)
					ent->pev->oldbuttons = -1; // monster moved far enough away
				else // monster still touching the portal
					g_Scheduler.SetTimeout(checkMonsterTouching, 0.5f, entHandle); // check again later
			}
			else {
				ent->pev->oldbuttons = -1; // can't touch a portal that no longer exists!
			}
		}
	}
}

void teleportEnt(CBaseEntity* ent, int portalIdx, Vector offset)
{
	Portal& portal = portals[portalIdx];
	if (ent->IsPlayer())
	{
		CBasePlayer* plr = (CBasePlayer*)ent;
		PlayerState& state = getPlayerState(plr);

		if (state.touchingPortal == portalIdx) // don't teleport until player stops touching the portal
			return;
		if (state.editing == portalIdx)
			return; // don't teleport if player is editing it
	}
	else
	{
		if (ent->pev->oldbuttons - 1 == portalIdx) // oldbuttons = last portal touched
			return;
	}

	int exitType = portal.type == PORTAL_BIDIR ? PORTAL_BIDIR : PORTAL_EXIT;
	int exitId = portal.target;
	if (exitId == -1 || exitId >= int(portals.size())) { // target is invalid
		portal.target = -1;
		exitId = getRandomPortal(exitType, portalIdx);
	}
	if (exitId != -1) {
		Portal& exit = portals[exitId];
		if (!exit.enabled || !exit.ent.IsValid())
			return;
		CBaseEntity* exitEnt = exit.ent;

		float zOffset = 0;
		string cname = STRING(ent->pev->classname);
		if (cname == "player" || cname.find("monster_") == 0) {
			if (cname == "monster_snark" || cname == "monster_satchel")
				zOffset = portalOffset;
			else
				zOffset = -portalOffset;
		}

		ent->pev->origin = exitEnt->pev->origin + offset + Vector(0, 0, zOffset);

		// Force player to duck just in case the exit is too low
		// This has no side effects if the portal is high enough and ducking isn't required
		if (ent->IsPlayer())
		{
			ent->pev->flDuckTime = 26;
			ent->pev->flags |= FL_DUCKING;

			// tell other plugins that a player was teleported
			//CustomKeyvalues@ pCustom = ent.GetCustomKeyvalues();
			//pCustom.SetKeyvalue("$f_lastPsTele", g_Engine.time);
		}

		if (exit.rotateMode != ROTATE_NO)
		{
			Vector oldAngles = ent->pev->angles; // used by monsters only (players use v_angle)
			if (exit.rotateMode == ROTATE_YES)
				ent->pev->angles = exit.angles;
			else if (exit.rotateMode == ROTATE_YAW_ONLY)
			{
				if (ent->IsPlayer())
					ent->pev->angles = ent->pev->v_angle;
				ent->pev->angles.y = exit.angles.y;
			}
			ent->pev->fixangle = FAM_FORCEVIEWANGLES;

			// Rotate the velocity vector to correct yaw
			Vector angleDiff = Vector(0, 0, 0);
			if (ent->IsPlayer())
				angleDiff.y = exit.angles.y - ent->pev->v_angle.y;
			else
				angleDiff.y = exit.angles.y - oldAngles.y;

			MAKE_VECTORS(exit.angles);
			
			ent->pev->velocity = gpGlobals->v_forward * ent->pev->velocity.Length();
		}
		int beamCount = 2;
		int pitch = 80;
		if (exit.exitSpeed != EXIT_MATCH_INPUT)
		{
			float newSpeed = 0;
			switch (exit.exitSpeed)
			{
			case(EXIT_ZERO): 	   newSpeed = 0; break;
			case(EXIT_SLOW): 	   pitch = 90; newSpeed = 512; break;
			case(EXIT_MEDIUM): 	   pitch = 110; beamCount = 3; newSpeed = 768; break;
			case(EXIT_FAST): 	   pitch = 130; beamCount = 4; newSpeed = 1024; break;
			case(EXIT_SUPERSONIC): pitch = 180; beamCount = 5; newSpeed = 4096; break;
			}

			MAKE_VECTORS(exit.angles);

			ent->pev->velocity = gpGlobals->v_forward*newSpeed;
		}

		// create portal effects
		if (exit.canAddBeams)
		{
			for (int k = 0; k < beamCount; k++)
			{						
				string spawnerName = "portal_special_fx" + fxid++;
				map<string, string> keyvalues;
				keyvalues["targetname"] = spawnerName;
				keyvalues["LightningStart"] = spawnerName;
				keyvalues["renderamt"] = "40";
				keyvalues["rendercolor"] = "64 255 64";
				keyvalues["Radius"] = "512";
				keyvalues["life"] = "0.1";
				keyvalues["BoltWidth"] = "20";
				keyvalues["NoiseAmplitude"] = "128";
				keyvalues["texture"] = "sprites/laserbeam.spr";
				keyvalues["TextureScroll"] = "35";
				keyvalues["StrikeTime"] = "0";
				keyvalues["spawnflags"] = "0";
				keyvalues["origin"] = to_string(exitEnt->pev->origin.x) + " " + to_string(exitEnt->pev->origin.y) + " " + to_string(exitEnt->pev->origin.z);
				CBaseEntity* specialfx = (CBaseEntity*)CreateEntity("env_beam", keyvalues, true)->pvPrivateData;
				gpGamedllFuncs->dllapi_table->pfnUse(specialfx->edict(), specialfx->edict());
				g_Scheduler.SetTimeout(removeEnt, 0.5f, EHandle(specialfx));
			}

			exit.canAddBeams = false;
			g_Scheduler.SetTimeout(allowMoreBeamsOnPortal, 0.7f, &exit);
		}

		PlaySound(exitEnt->edict(), CHAN_STATIC, spawn_sound, 0.5f, 1.0f, 0, pitch);

		if (exit.type == PORTAL_BIDIR)
		{
			if (ent->IsPlayer())
			{
				CBasePlayer* plr = (CBasePlayer*)ent;
				PlayerState& state = getPlayerState(plr);
				state.touchingPortal = exitId;
			}
			else
			{
				ent->pev->oldbuttons = exitId + 1;
				EHandle entHandle = ent;
				g_Scheduler.SetTimeout(checkMonsterTouching, 0.5f, entHandle);
			}
		}
	}
}

bool oddUpdate = false;

void portalBeamThink()
{
	for (int i = 0; i < portals.size(); i++) {
		if (portals[i].ent.IsValid() && portals[i].beamTarget.IsValid()) {
			te_beaments(portals[i].ent, portals[i].beamTarget, "sprites/laserbeam.spr", 0, 100, 1, 16, 1, WHITE);
		}
	}
}

// TODO: This is ineffecient
void portalThink()
{
	if (!portalsEnabled || portals.size() == 0)
		return;

	// split up the work into 2 frames for better performance. Fast moving entities might fly past some portals.
	int begin = 0;
	int end = portals.size() / 2;
	if (oddUpdate) {
		begin = end;
		end = portals.size();
	}
	oddUpdate = !oddUpdate;

	for (int i = begin; i < end; i++) {
		if (portals[i].type == PORTAL_EXIT)
			continue;
		if (!portals[i].ent.IsValid())
			continue;

		if (!portals[i].enabled)
			continue;
		CBaseEntity* portalEnt = portals[i].ent;

		// teleport players, point entites, and some small monsters 
		edict_t* ent = NULL;
		do {
			ent = g_engfuncs.pfnFindEntityInSphere(ent, portalEnt->pev->origin, portalTouchRadius);
			int gotit = ENTINDEX(ent);
			if (isValidFindEnt(ent))
			{
				if (projectiles.count(string(STRING(ent->v.classname))))
				{
					teleportEnt((CBaseEntity*)ent->pvPrivateData, int(i), Vector(0, 0, 0));
				}
				//else if (string(ent->pev->classname).Find("env_") != 0)
				//	println("FOUND: " + ent->pev->classname);
			}
		} while (ent && !ent->free && ENTINDEX(ent) != 0);

		// Teleport monsters that have origins on the floor (so, basically all of them)
		Vector monsterTeleOrigin = portalEnt->pev->origin;
		monsterTeleOrigin.z -= 36; // adjust portal height for floor origins

		ent = NULL;
		do {
			ent = g_engfuncs.pfnFindEntityInSphere(ent, monsterTeleOrigin, portalTouchRadius);
			if (isValidFindEnt(ent) && ((CBaseEntity*)ent->pvPrivateData)->IsMonster())
			{
				teleportEnt((CBaseEntity*)ent->pvPrivateData, int(i), Vector(0, 0, -36));
			}
		} while (ent && !ent->free && ENTINDEX(ent) != 0);
	}

	// special func_pushable handling
	/*
	CBaseEntity@ pushable = null;
	do {
		@pushable = g_EntityFuncs.FindEntityByClassname(pushable, "func_pushable");

		if (pushable !is null)
		{
			for (uint i = begin; i < end; i++) {
				if (portals[i].type == PORTAL_EXIT)
					continue;
				if (!portals[i].enabled or !portals[i].ent)
					continue;
				CBaseEntity@ portalEnt = portals[i].ent;
				Vector portalOri = portalEnt->pev->origin;

				// calculate origin of pushable ("origin" doesn't always give the correct position)
				Vector pushMiddle = pushable->pev->mins + (pushable->pev->maxs - pushable->pev->mins) / 2.0f;
				Vector pushOri = pushable->pev->origin + pushMiddle;

				float distance = Vector(portalOri - pushOri).size();
				if (distance < portalTouchRadius)
				{
					Vector offset = pushable->pev->origin - pushOri;
					teleportEnt(pushable, int(i), offset);
				}
			}
		}
	} while (pushable !is null);
	*/

	if (oddUpdate) return;

	// update player states
	for (int i = 1; i <= 32; i++)
	{
		edict_t* plr = INDEXENT(i);
		PlayerState& state = g_player_states[i];
		if (!isValidPlayer(plr))
			continue;

		if (state.editing != -1)
		{
			if (state.editing >= int(portals.size())) {
				state.editing = -1;
				state.linkLast = -1;
			}
			//if (portals[state.editing].enabled)
			//	portals[state.editing].disable();
			if (plr->v.deadflag != 0 && portalsEnabled) {
				portals[state.editing].enable();
				state.editing = -1;
				state.removeConfirm = false;
				state.deletePortal = -1;
				state.linkLast = -1;
				state.menuPage = 0;
			}
		}

		if (state.touchingPortal == -1)
			continue;
		if (state.touchingPortal >= int(portals.size()))
		{
			state.touchingPortal = -1;
			continue;
		}

		// Check if player is still touching this portal
		Portal& p = portals[state.touchingPortal];
		if (p.ent.IsValid()) {
			CBaseEntity* portalEnt = p.ent;
			float distance = Vector(portalEnt->pev->origin - plr->v.origin).Length();
			if (distance > portalTouchRadius*2.0f)
				state.touchingPortal = -1; // player moved far enough away
		}
		else {
			state.touchingPortal = -1; // can't touch a portal that no longer exists!
		}
	}
}

string getPortalOwnerName(CBasePlayer* caller, Portal& portal)
{
	string portalOwnerId = portal.owner;
	string callerId = getPlayerUniqueId(caller->edict());
	if (callerId == portalOwnerId)
		return "your";

	for (int i = 1; i <= 32; i++)
	{
		edict_t* plr = INDEXENT(i);
		PlayerState& state = g_player_states[i];
		if (!isValidPlayer(plr))
			continue;

		string steamId = getPlayerUniqueId(plr);

		if (steamId == portalOwnerId)
			return string(STRING(plr->v.netname)) + "'s";
	}
	return "unowned";
}

bool saveMapPortals()
{
	string path = string(portal_save_path) + STRING(gpGlobals->mapname) + ".txt";
	FILE* file = fopen(path.c_str(), "w");
	if (!file)
	{
		println("[PortalSpawner] Failed to open file for writing: %s", path.c_str());
		println("[PortalSpawner] Make sure the directory path exists.");
		return false;
	}

	if (portals.size() > 0)
	{
		int totalSz = 0;
		for (int i = 0; i < portals.size(); i++) {
			string dat = portals[i].serialize();
			totalSz += dat.size();
			fwrite(dat.c_str(), 1, dat.size(), file);
		}

		println("[PortalSpawner] Wrote '%s' (%d bytes)", path.c_str(), totalSz);
	}
	else
	{
		remove(path.c_str());
		println("[PortalSpawner] Deleted " + path);
	}

	fclose(file);

	return true;
}

void loadMapPortals()
{
	// reset player states
	for (int i = 1; i <= 32; i++)
	{
		edict_t* plr = INDEXENT(i);
		PlayerState& state = g_player_states[i];
		state.editing = -1;
		state.touchingPortal = -1;
		state.editing = -1;
		state.linkLast = -1;
		state.menuPage = 0;
	}

	string path = string(portal_save_path) + STRING(gpGlobals->mapname) + ".txt";
	FILE* file = fopen(path.c_str(), "r");
	if (!file)
	{
		println("[PortalSpawner] No portal data found for this map");
		return;
	}

	removeAllPortals();

	string line;

	Portal temp;
	Vector tempOri;
	Vector tempAngles;
	int numPortals = 0;

	while (cgetline(file, line)) {
		if (line.empty()) {
			continue;
		}
		if (line == "{") {
			temp = Portal();
			tempOri = Vector(0, 0, 0);
			tempAngles = Vector(0, 0, 0);
			continue;
		}
		if (line == "}") {
			CBaseEntity* ent = createPortal(tempOri, temp.type, spawn_sound, 0, false);
			temp.ent = ent;
			portals.push_back(temp);
			continue;
		}

		int eq = line.find("=");
		
		if (eq == -1) {
			continue;
		}

		string name = line.substr(0, eq);
		string value = line.substr(eq + 1);

		if (name == "x") {
			tempOri.x = atof(value.c_str());
		}
		else if (name == "y") {
			tempOri.y = atof(value.c_str());
		}
		else if (name == "z") {
			tempOri.z = atof(value.c_str());
		}
		else if (name == "ax") {
			tempAngles.x = atof(value.c_str());
		}
		else if (name == "ay") {
			tempAngles.y = atof(value.c_str());
		}
		else if (name == "az") {
			tempAngles.z = atof(value.c_str());
		}
		else if (name == "owner") {
			temp.owner = value;
		}
		else if (name == "time") {
			temp.creationTime = atof(value.c_str());
		}
		else if (name == "type") {
			temp.type = atoi(value.c_str());
		}
		else if (name == "speed") {
			temp.exitSpeed = atoi(value.c_str());
		}
		else if (name == "rot") {
			temp.rotateMode = atoi(value.c_str());
		}
		else if (name == "target") {
			temp.target = atoi(value.c_str());
		}
	}

	println("[PortalSpawner] Loaded %d portals", portals.size());
}

void openPortalMenu(EHandle h_plr)
{
	CBasePlayer* plr = (CBasePlayer*)h_plr.GetEntity();
	if (!plr) {
		return;
	}

	PlayerState& state = getPlayerState(plr);
	TextMenu& menu = initMenuForPlayer(plr->edict(), portalMenuCallback);

	string title = "Portal Menu";
	if (state.editing != -1) {
		title += " - Edit Portal";
	}
	if (state.linkLast != -1)
		title = "Link with this portal?";
	else if (state.removeConfirm)
		title = "Remove all portals?";
	else if (state.deletePortal >= 0)
	{
		if (state.deletePortal >= int(portals.size()))
		{
			state.deletePortal = -1;
		}
		else
		{
			Portal& p = portals[state.deletePortal];
			title = "Remove " + getPortalOwnerName(plr, p) + " portal?";
		}

	}
	else title = title + ":";
	menu.SetTitle(title + "\n");

	if (state.deletePortal != -1)
	{
		menu.AddItem("Yes", "delete-yes");
		menu.AddItem("No", "delete-no");
	}
	else if (state.removeConfirm)
	{
		menu.AddItem("Yes", "kill-all");
		menu.AddItem("No", "kill-all-cancel");
		menu.AddItem("Only my portals", "kill-all-owner");

		set<string> names;
		names.insert(getPlayerUniqueId(plr->edict()));
		for (int i = 0; i < portals.size(); i++)
		{
			string ownerFormatted = getPortalOwnerName(plr, portals[i]);
			string owner = portals[i].owner;
			if (names.count(owner)) {
				continue;
			}
			names.insert(owner);

			menu.AddItem("Only " + ownerFormatted + " portals", string("kill-all-owner-") + owner);
		}

	}
	else if (state.editing != -1 && state.editing < int(portals.size())) // editing a portal
	{
		Portal& portal = portals[state.editing];

		if (state.linkLast != -1)
		{
			menu.AddItem("Yes", "edit-link-confirm");
			menu.AddItem("No, the one before it", "edit-link-last");
			menu.AddItem("No, the one closest to me", "edit-link-closest");
			menu.AddItem("Unlink current target", "edit-link-random");
			menu.AddItem("", "");
			menu.AddItem("\n", "");
			menu.AddItem("Cancel", "edit-link-cancel");
		}
		else
		{
			string target = portal.target == -1 ? "Random" : "portal #" + to_string(portal.target);
			menu.AddItem("Target: " + target + "", "edit-link-last");
			menu.AddItem("Update position", "edit-position");

			if (portal.type == PORTAL_EXIT || portal.type == PORTAL_BIDIR)
			{
				string exitSpeed;
				switch (portal.exitSpeed)
				{
				case(EXIT_MATCH_INPUT): exitSpeed = "Same as input"; break;
				case(EXIT_ZERO): 		exitSpeed = "Stopped"; break;
				case(EXIT_SLOW): 		exitSpeed = "Slow"; break;
				case(EXIT_MEDIUM): 		exitSpeed = "Medium"; break;
				case(EXIT_FAST): 		exitSpeed = "Fast"; break;
				case(EXIT_SUPERSONIC):  exitSpeed = "Supersonic"; break;
				}
				if (portal.type == PORTAL_ENTER)
					exitSpeed = "N/A";

				string rotateMode;
				switch (portal.rotateMode)
				{
				case(ROTATE_NO): rotateMode = "No"; break;
				case(ROTATE_YES): rotateMode = "Yes"; break;
				case(ROTATE_YAW_ONLY): rotateMode = "Yaw only"; break;
				}

				menu.AddItem("Update angles: yaw = " + to_string(int(portal.angles.y)) + "\tpitch = " + to_string(int(portal.angles.x)) + "", "edit-angles");
				menu.AddItem("Exit speed: " + exitSpeed + "", "edit-speed");
				menu.AddItem("Rotate entities: " + rotateMode + "", "edit-rotate");
			}

			if (portal.type == PORTAL_ENTER) {
				menu.AddItem("", "");
				menu.AddItem("", "");
				menu.AddItem("", "");
			}
			menu.AddItem("\n", "");

			menu.AddItem("Done ", "edit-cancel");
		}
	}
	else
	{
		if (state.menuPage == 0)
		{
			menu.AddItem("Create entrance", "add-enter");
			menu.AddItem("Create exit", "add-exit");
			menu.AddItem("Create bidirectional", "add-bi");

			menu.AddItem("Edit portal", "edit");

			menu.AddItem("Delete portal", "delete");

			if (portalsEnabled)
				menu.AddItem("Disable portals\n", "toggle-portals");
			else
				menu.AddItem("Enable portals\n", "toggle-portals");

			menu.AddItem("More", "next-page");
		}
		else
		{
			menu.AddItem("Remove all portals", "kill-all");
			menu.AddItem("", "");
			menu.AddItem("Save portals", "save");
			menu.AddItem("", "");
			menu.AddItem("Load portals", "load");
			menu.AddItem("\n", "");
			menu.AddItem("Back", "previous-page");
		}

	}

	menu.Open(0, 0, plr->edict());
}

void ClientLeave(edict_t* leaver)
{
	string steamId = getPlayerUniqueId(leaver);

	PlayerState& state = g_player_states[ENTINDEX(leaver)];

	if (state.editing != -1 && state.editing < int(portals.size()))
		portals[state.editing].enable();

	state.editing = -1;
	state.touchingPortal = -1;
	state.editing = -1;
	state.linkLast = -1;
	state.menuPage = 0;

	RETURN_META(MRES_IGNORED);
}

bool doCommand(edict_t* plr) {
	PlayerState& state = getPlayerState(plr);
	bool isAdmin = AdminLevel(plr) >= ADMIN_YES;

	CommandArgs args = CommandArgs();
	args.loadArgs();

	string lowerArg = toLowerCase(args.ArgV(0));

	if (args.ArgC() > 0)
	{
		if (args.ArgV(0) == ".ps")
		{
			//println("GOT CMD: " + args[0] + " " + args.ArgC());
			if (args.ArgC() == 1) // open menu if no args
			{
				if (!isAdmin)
				{
					ClientPrint(plr, HUD_PRINTTALK, "You don't have access to that command, peasant.\n");
					return true;
				}

				state.editing = -1;
				state.linkLast = -1;
				state.menuPage = 0;

				openPortalMenu(plr);

				return true;
			}
			else if (args.ArgC() > 1)
			{
				if (!isAdmin)
				{
					ClientPrint(plr, HUD_PRINTTALK, "You don't have access to that command, peasant.\n");
					return true;
				}

				string portalType = "enter";
				if (args.ArgC() > 1)
				{
					if (args.ArgV(1).find("en") == 0) {
						portalType = "en";
					}
					else if (args.ArgV(1).find("ex") == 0) {
						portalType = "exit";
					}
					else if (args.ArgV(1).find("bi") == 0) {
						portalType = "bi";
					}
				}

				add_portal_action((CBasePlayer*)plr->pvPrivateData, "add-" + portalType);

				return true;
			}
		}
	}
	return false;
}

void MapInit(edict_t* pEdictList, int edictCount, int maxClients) {
	g_player_states.resize(0);
	g_player_states.resize(33);

	g_engfuncs.pfnPrecacheModel((char*)entrance_sprite);
	g_engfuncs.pfnPrecacheModel((char*)exit_sprite);
	g_engfuncs.pfnPrecacheModel((char*)bi_sprite);
	g_engfuncs.pfnPrecacheModel((char*)beam_sprite);

	PrecacheSound(spawn_sound);
	PrecacheSound(remove_sound);

	g_Scheduler.SetTimeout(loadMapPortals, 2);

	RETURN_META(MRES_IGNORED);
}

void MapInit_post(edict_t* pEdictList, int edictCount, int maxClients) {
	loadSoundCacheFile();
}

void StartFrame() {
	g_Scheduler.Think();
	portalThink();
	RETURN_META(MRES_IGNORED);
}

// called before angelscript hooks
void ClientCommand(edict_t* pEntity) {
	TextMenuClientCommandHook(pEntity);
	META_RES ret = doCommand(pEntity) ? MRES_SUPERCEDE : MRES_IGNORED;
	RETURN_META(ret);
}

void MessageBegin(int msg_dest, int msg_type, const float* pOrigin, edict_t* ed) {
	TextMenuMessageBeginHook(msg_dest, msg_type, pOrigin, ed);
	RETURN_META(MRES_IGNORED);
}

void PluginInit() {
	g_player_states.resize(33);

	g_engine_hooks.pfnMessageBegin = MessageBegin;
	g_dll_hooks.pfnClientCommand = ClientCommand;
	g_dll_hooks.pfnClientDisconnect = ClientLeave;
	g_dll_hooks.pfnServerActivate = MapInit;
	g_dll_hooks_post.pfnServerActivate = MapInit_post;
	g_dll_hooks.pfnStartFrame = StartFrame;

	initLists();
	LoadAdminList();
	
	removeAllPortals();
	g_Scheduler.SetTimeout(loadMapPortals, 1);
	g_Scheduler.SetInterval(portalBeamThink, 0.1f, -1);

	if (gpGlobals->time > 4) {
		loadSoundCacheFile();
	}
}

void PluginExit() {}