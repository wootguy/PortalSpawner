
class PlayerState
{
	EHandle plr;
	CTextMenu@ menu;
	int touchingPortal;   // portal ID player is currently touching (-1 if not touching any)
	int editing; 		  // portalId currently being edited
	bool removeConfirm;   // player should confirm if he wants ALL portals removed
	int deletePortal;     // player should confirm if he wants to delete another player's portal
	int linkLast; 		  // player is choosing a previous portal to link to
	
	void initMenu(CBasePlayer@ plr, TextMenuPlayerSlotCallback@ callback)
	{
		CTextMenu temp(@callback);
		@menu = @temp;
	}
	
	void openMenu(CBasePlayer@ plr) 
	{
		if ( menu.Register() == false ) {
			g_Game.AlertMessage( at_console, "Oh dear menu registration failed\n");
		}
		menu.Open(0, 0, plr);
	}
}

enum portal_types 
{
	PORTAL_ENTER,
	PORTAL_EXIT,
	PORTAL_BIDIR,
}

enum exit_speeds
{
	EXIT_MATCH_INPUT,
	EXIT_ZERO,
	EXIT_SLOW,
	EXIT_MEDIUM,
	EXIT_FAST,
	EXIT_SUPERSONIC,
	EXIT_SPEEDS
}

enum rotate_modes
{
	ROTATE_NO,
	ROTATE_YES,
	ROTATE_YAW_ONLY,
	ROTATE_MODES
}

class Portal
{
	EHandle ent; // portal env_sprite
	string owner; // steamid or username of the player who created this portal
	float creationTime; // server time that portal was created
	Vector angles;
	int type;
	int exitSpeed;
	int rotateMode;
	bool enabled;
	int target; // -1 = any portal
	bool canAddBeams; // prevents beam spam
	CBeam@ beam; // used for identifying targets (not the same thing as the special effects beams on teleports)
	
	Portal(CBaseEntity@ portalEnt, CBasePlayer@ creator, Vector portalAngles, int portalType)
	{
		string steamId = g_EngineFuncs.GetPlayerAuthId( creator.edict() );
		if (steamId == 'STEAM_ID_LAN') {
			steamId = creator.pev.netname;
		}
		owner = steamId;
		ent = portalEnt;
		angles = portalAngles;
		type = portalType;
		exitSpeed = EXIT_MATCH_INPUT;
		rotateMode = ROTATE_NO;
		enabled = true;
		target = -1;
		canAddBeams = true;
		creationTime = g_EngineFuncs.Time();
	}
	
	void disable()
	{
		enabled = false;
		if (ent) {
			CBaseEntity@ e = ent;
			e.pev.framerate = 0;
			e.pev.renderamt = 100;
		}
	}
	
	void enable()
	{
		enabled = true;
		if (ent) {
			CBaseEntity@ e = ent;
			e.pev.framerate = 10;
			e.pev.renderamt = 200;
		}
		
		killBeam();
	}
	
	void killBeam()
	{
		if (beam !is null) {
			g_EntityFuncs.Remove( beam );
			@beam = null;
		}
	}
	
	void drawBeamTo(CBaseEntity@ otherEnt)
	{
		if (beam !is null) {
			g_EntityFuncs.Remove( beam );
		}
		
		@beam = g_EntityFuncs.CreateBeam( beam_sprite, 16 );
		beam.EntsInit( ent, otherEnt );
		beam.pev.targetname = "portalspawner_beam";
		beam.SetFlags( BEAM_FSINE );
	}
}

// persistent-ish player data, organized by steam-id or username if on a LAN server, values are @PlayerState
dictionary player_states;

string entrance_sprite = 'sprites/enter1.spr';
string exit_sprite = 'sprites/exit1.spr';
string bi_sprite = 'sprites/b-tele1.spr';
string beam_sprite = 'sprites/laserbeam.spr';

string spawn_sound = 'debris/beamstart7.wav';
string remove_sound = 'debris/beamstart11.wav';
string portal_targetname = "pspawner_asplugin_portal";

array<Portal@> portals;

int MAX_MAP_PORTALS = 64;
float portalOffset = 18.0f;
float portalTouchRadius = 40.0f;
float portalEditRadius = 150.0f;
bool portalsEnabled = true;
bool super_spawn = false;

CClientCommand portal_menu( "portal_menu", "Open the portal_spawner menu", @PortalMenu );

void PluginInit()
{
	g_Module.ScriptInfo.SetAuthor( "w00tguy" );
	g_Module.ScriptInfo.SetContactInfo( "w00tguy123 - forums.svencoop.com" );
	g_Hooks.RegisterHook( Hooks::Player::ClientSay, @ClientSay );	
	g_Hooks.RegisterHook( Hooks::Player::ClientDisconnect, @ClientLeave );
	g_Hooks.RegisterHook( Hooks::Game::MapChange, @MapChange );
	
	g_Scheduler.SetInterval("portalThink", 0);
}

void MapInit()
{
	g_Game.PrecacheModel(entrance_sprite);
	g_Game.PrecacheModel(exit_sprite);
	g_Game.PrecacheModel(bi_sprite);
	g_Game.PrecacheModel(beam_sprite);
	
	g_SoundSystem.PrecacheSound(spawn_sound);
	g_SoundSystem.PrecacheSound(remove_sound);
	
	removeAllPortals();
}

HookReturnCode MapChange()
{
	// set all menus to null. Apparently this fixes crashes for some people:
	// http://forums.svencoop.com/showthread.php/43310-Need-help-with-text-menu#post515087
	array<string>@ stateKeys = player_states.getKeys();
	for (uint i = 0; i < stateKeys.length(); i++)
	{
		PlayerState@ state = cast<PlayerState@>( player_states[stateKeys[i]] );
		if (state.menu !is null)
			@state.menu = null;
	}
	player_states.deleteAll();
	return HOOK_CONTINUE;
}

void print(string text) { g_Game.AlertMessage( at_console, text); }
void println(string text) { print(text + "\n"); }

// Will create a new state if the requested one does not exit
PlayerState@ getPlayerState(CBasePlayer@ plr)
{
	string steamId = g_EngineFuncs.GetPlayerAuthId( plr.edict() );
	if (steamId == 'STEAM_ID_LAN') {
		steamId = plr.pev.netname;
	}
	
	if ( !player_states.exists(steamId) )
	{
		PlayerState state;
		state.plr = plr;
		state.touchingPortal = -1;
		state.editing = -1;
		state.linkLast = -1;
		state.deletePortal = -1;
		player_states[steamId] = state;
	}
	return cast<PlayerState@>( player_states[steamId] );
}

CBaseEntity@ createPortal(CBasePlayer@ plr, string spriteFile, string soundFile)
{
	Vector origin = plr.pev.origin;
	
	// create sprite at this location
	string spriteName = portal_targetname;
	dictionary keyvalues;
	keyvalues["targetname"] = spriteName;
	keyvalues["model"] = spriteFile;
	keyvalues["framerate"] = portalsEnabled ? "10" : "0";
	keyvalues["scale"] = "1";
	keyvalues["rendermode"] = "5";
	keyvalues["renderfx"] = "0";
	keyvalues["rendercolor"] = "255 255 255";
	keyvalues["renderamt"] = portalsEnabled ? "200" : "100";
	keyvalues["spawnflags"] = "1";
	keyvalues["made_with_portal_spawner"] = "very_yes"; // TODO: This doesn't work. Learn about custom keyvalues
	keyvalues["origin"] = "" + origin.x + " " + origin.y + " " + (origin.z+portalOffset);
	
	CBaseEntity@ portal = g_EntityFuncs.CreateEntity( "env_sprite", keyvalues, true );
	
	// play sound so you feel like you've accomplished something
	g_SoundSystem.PlaySound(portal.edict(), CHAN_STATIC, soundFile, 1.0f, 1.0f, 0, 100);
		
	return portal;
}

void removeAllPortals() {
	for (int i = 0; i < MAX_MAP_PORTALS; i++) {
		CBaseEntity@ portal = g_EntityFuncs.FindEntityByTargetname(null, portal_targetname);
		if (portal !is null)
			g_EntityFuncs.Remove(portal);
	}
	
	CBaseEntity@ beam = null;
	do {
		@beam = g_EntityFuncs.FindEntityByTargetname(null, "portalspawner_beam");
		if (beam !is null)
			g_EntityFuncs.Remove(beam);
	} while(beam !is null);
	
	portals.resize(0);
}

bool isTooCloseToPortal(CBasePlayer@ plr, int ignoreIdx)
{
	for (uint i = 0; i < portals.length(); i++)
	{
		if (int(i) == ignoreIdx)
			continue;
		if (portals[i].ent) {
			CBaseEntity@ e = portals[i].ent;
			float distance = Vector(e.pev.origin - plr.pev.origin).Length();
			if (distance <= portalTouchRadius*2)
				return true;
		}
		
	}
	return false;
}

bool canAddPortal(CBasePlayer@ plr) {
	if (int(portals.length()) == MAX_MAP_PORTALS)
	{
		g_PlayerFuncs.SayText(plr, "Max portals reached (" + MAX_MAP_PORTALS + ")\n");
		return false;
	}
	if (isTooCloseToPortal(plr, -1))
	{
		g_PlayerFuncs.SayText(plr, "Move farther away before spawning another portal.\n");
		return false;
	}
	return true;
}

void removePortal(int idx)
{
	if (idx < 0)
		return;
	if (idx >= int(portals.length())) {
		println("Bad portal index passed to removePortal(): " + idx);
		return;
	}
	portals[idx].killBeam();
	if (portals[idx].ent) {
		CBaseEntity@ e = portals[idx].ent;
		g_SoundSystem.PlaySound(e.edict(), CHAN_STATIC, remove_sound, 0.5f, 1.0f, 0, 80);
		g_EntityFuncs.Remove( e );
	}
	portals.removeAt(idx);
			
	// adjust editing indexes for other players
	array<string>@ stateKeys = player_states.getKeys();
	for (uint i = 0; i < stateKeys.length(); i++)
	{
		PlayerState@ state = cast<PlayerState@>( player_states[stateKeys[i]] );
		if (state.editing == -1)
			continue;
		if (state.editing == idx) {
			state.editing = -1; // sorry, you can't edit this one anymore
			g_Scheduler.SetTimeout("openPortalMenu", 0, state.plr);
		}
		else if (state.editing > idx) {
			state.editing -= 1;
		}
	}

	// update portal chains
	for (uint i = 0; i < portals.length(); i++)
	{
		if (portals[i].target == idx)
			portals[i].target = -1;
		else if (portals[i].target > idx)
			portals[i].target -= 1;
	}
}

void blinkPortal(Portal@ portal)
{
	if (portal !is null and portal.ent)
	{
		CBaseEntity@ ent = portal.ent;
		if (ent.pev.rendercolor.x != 64)
			ent.pev.rendercolor = Vector(64, 64, 255);
		else
			ent.pev.rendercolor = Vector(255, 255, 255);
	}
}

int getClosestPortalIdx(CBasePlayer@ plr)
{
	float minDist = 9999.0f;
	int closestPortal = -1;
	for (uint i = 0; i < portals.length(); i++)
	{
		if (!portals[i].ent)
			continue;
		CBaseEntity@ ent = portals[i].ent;
		float distance = Vector(ent.pev.origin - plr.pev.origin).Length();
		if (distance <= portalEditRadius and distance < minDist)
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
int getLastPlacedPortalIdx(CBasePlayer@ plr, float maxTime, int filterType, int skipId)
{
	string steamId = g_EngineFuncs.GetPlayerAuthId( plr.edict() );
	if (steamId == 'STEAM_ID_LAN') {
		steamId = plr.pev.netname;
	}
	
	int lastPortalIdx = -1;
	float mostRecent = 0;
	for (uint i = 0; i < portals.length(); i++)
	{
		if (int(i) == skipId)
			continue;
		if (maxTime > 0 and portals[i].creationTime >= maxTime)
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
		for (uint i = 0; i < portals.length(); i++)
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
	array<string>@ stateKeys = player_states.getKeys();
	for (uint i = 0; i < stateKeys.length(); i++)
	{
		PlayerState@ state = cast<PlayerState@>( player_states[stateKeys[i]] );
		if (state.editing == idx)
			return true;
	}
	return false;
}

void portalMenuCallback(CTextMenu@ menu, CBasePlayer@ plr, int page, const CTextMenuItem@ item)
{
	PlayerState@ state = getPlayerState(plr);
	if (item is null)
	{
		if (state.editing != -1 and state.editing < int(portals.length()))
		{
			if (portalsEnabled)
				portals[state.editing].enable();
		}
		state.editing = -1;
		return;
	}
	string action = item.szUserData;
	
	
	if (action == "toggle-portals")
	{
		portalsEnabled = !portalsEnabled;
		if (portalsEnabled)
			g_PlayerFuncs.SayTextAll(plr, "" + plr.pev.netname + " enabled all portals\n");
		else
			g_PlayerFuncs.SayTextAll(plr, "" + plr.pev.netname + " disabled all portals\n");
		for (uint i = 0; i < portals.length(); i++)
		{
			if (portalsEnabled and !isPlayerEditingPortal(i))
				portals[i].enable();
			else
				portals[i].disable();
		}
	}
	else if (action == "edit")
	{
		int closestPortal = getClosestPortalIdx(plr);
		if (closestPortal == -1)
		{
			g_PlayerFuncs.SayText(plr, "Stand closer to a portal to edit.\n");
		} 
		else {
			bool canEdit = true;
			array<string>@ stateKeys = player_states.getKeys();
			for (uint i = 0; i < stateKeys.length(); i++)
			{
				PlayerState@ s = cast<PlayerState@>( player_states[stateKeys[i]] );
				if (s.editing == closestPortal)
				{
					CBaseEntity@ p = s.plr;
					g_PlayerFuncs.SayText(plr, "Wait for " + p.pev.netname + " to finish editing this portal\n");
					canEdit = false;
					break;
				}
			}
	
			if (canEdit)
			{
				state.editing = closestPortal;
				Portal@ p = portals[state.editing];
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
			g_PlayerFuncs.SayText(plr, "You can't link a portal to itself!\n");
		else
		{
			portals[state.editing].target = state.linkLast;
			portals[state.linkLast].target = state.editing;
			state.linkLast = -1;
		}
		
		Portal@ p = portals[state.editing];
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
			state.linkLast = getLastPlacedPortalIdx(plr, maxTime, desiredPortalType, state.editing);
			if (state.linkLast == -1 or state.linkLast == oldLink) // couldn't find an earlier portal
			{
				state.linkLast = oldLink;
				if (desiredPortalType == PORTAL_EXIT)
					g_PlayerFuncs.SayText(plr, "No other portal exits found.\n");
				else if (desiredPortalType == PORTAL_ENTER)
					g_PlayerFuncs.SayText(plr, "No other portal entrances found.\n");
				else
					g_PlayerFuncs.SayText(plr, "No other bidirectional portals found.\n");
			}
		} else {
			float maxTime = portals[state.editing].creationTime; // so getLast doesn't return this one
				
			state.linkLast = getLastPlacedPortalIdx(plr, maxTime, desiredPortalType, state.editing);
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
			
		int closestPortal = getClosestPortalIdx(plr);
		if (closestPortal == -1)
		{
			g_PlayerFuncs.SayText(plr, "Stand closer to a portal to set it as the target.\n");
		} 
		else {
			if (state.editing == closestPortal)
				g_PlayerFuncs.SayText(plr, "You can't link a portal to itself!\n");
			else 
			{
				if (portals[state.editing].type == PORTAL_ENTER and portals[closestPortal].type != PORTAL_EXIT or
					portals[state.editing].type == PORTAL_EXIT and portals[closestPortal].type != PORTAL_ENTER)
					g_PlayerFuncs.SayText(plr, "Portal entrances can only be linked to portal exits\n");
				else if (portals[state.editing].type == PORTAL_BIDIR and portals[closestPortal].type != PORTAL_BIDIR)
					g_PlayerFuncs.SayText(plr, "Bidirectional portals can only be linked to other bidirectional portals\n");
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
			portals[ portals[state.editing].target ].target = -1;
		portals[state.editing].killBeam();
		portals[state.editing].target = -1;
		state.linkLast = -1;
	}
	else if (action == "edit-link-cancel")
	{
		state.linkLast = -1;
		
		Portal@ p = portals[state.editing];
		p.killBeam();
		if (p.target != -1) {
			p.drawBeamTo(portals[p.target].ent);
		}
	}
	else if (action == "delete")
	{
		state.deletePortal = getClosestPortalIdx(plr);
		if (state.deletePortal == -1)
			g_PlayerFuncs.SayText(plr, "Stand closer to the portal you want to delete\n");
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
		if (isTooCloseToPortal(plr, state.editing))
			g_PlayerFuncs.SayText(plr, "Can't move here - you're too close to another portal.\n");
		else if (portals[state.editing].ent)
		{
			CBaseEntity@ ent = portals[state.editing].ent;
			ent.pev.origin = plr.pev.origin + Vector(0,0,portalOffset);
		}
	}
	else if (action == "edit-angles")
	{
		portals[state.editing].angles = plr.pev.v_angle;
	}
	else if (action == "kill-all-cancel") {
		state.removeConfirm = false;
	}
	else if (action == "kill-all") {
		if (state.removeConfirm) {
			removeAllPortals();
			g_PlayerFuncs.SayTextAll(plr, "" + plr.pev.netname + " removed all the portals\n");
			state.removeConfirm = false;
			
			// play global sound
			string ambientName = "ps__" + remove_sound;
			dictionary keyvalues;
			keyvalues["targetname"] = ambientName;
			keyvalues["message"] = remove_sound;
			keyvalues["pitch"] = "100";
			keyvalues["spawnflags"] = "49";
			keyvalues["playmode"] = "1";
			keyvalues["health"] = "10";

			CBaseEntity@ ambient = g_EntityFuncs.CreateEntity( "ambient_generic", keyvalues, true );
			
			g_EntityFuncs.FireTargets(ambientName, null, null, USE_ON);

			// delete the entity we just created
			if (ambient !is null)
				g_EntityFuncs.Remove(ambient);
			
		} else {
			state.removeConfirm = true;
		}
	}
	else if (action == "kill-all-owner") {
	
		string steamId = g_EngineFuncs.GetPlayerAuthId( plr.edict() );
		if (steamId == 'STEAM_ID_LAN') {
			steamId = plr.pev.netname;
		}
	
		for (uint i = 0; i < portals.length(); i++)
		{
			if (portals[i].owner == steamId)
			{
				removePortal(i);
				i--;
			}
		}
		g_PlayerFuncs.SayText(plr, "All of your portals were removed\n");
		state.removeConfirm = false;
	}
	else if (action.Find("add-") == 0)
	{
		Vector angles = plr.pev.v_angle;
		if (canAddPortal(plr)) {
			//print("Angles: " + angles.x + " " + angles.y + " " + angles.z);
			
			for (int x = 0; x < 8; x++)
			{
				for (int y = 0; y < 8; y++)
				{
					Vector offset = Vector(x*portalTouchRadius*2, y*portalTouchRadius*2, 0);
					int portalType;
					string spriteType;
					if (action == "add-enter") {
						portalType = PORTAL_ENTER;
						spriteType = entrance_sprite;
					}
					else if (action == "add-exit") {
						portalType = PORTAL_EXIT;
						spriteType = exit_sprite;
					}
					else if (action == "add-bi") {
						portalType = PORTAL_BIDIR;
						spriteType = bi_sprite;
					}
					
					CBaseEntity@ portalEnt = createPortal(plr, spriteType, spawn_sound);
					portalEnt.pev.origin = portalEnt.pev.origin + offset;
					Portal@ portal = Portal(portalEnt, plr, angles, portalType);
					portals.insertLast(portal);
					
					state.touchingPortal = portals.length()-1;
					if (!super_spawn)
						break;
				}
				if (!super_spawn)
					break;
			}
		}
	}
	
	g_Scheduler.SetTimeout("openPortalMenu", 0, @plr);
}

int getRandomPortal(int portalType, int currentPortalId)
{
	array<int> portalIds;
	for (uint i = 0; i < portals.length(); i++) {
		if (portals[i].enabled and int(i) != currentPortalId and 
			portals[i].type == portalType and portals[i].ent and portals[i].target == -1)
			portalIds.insertLast(i);
	}
	
	if (portalIds.length() > 0 )
	{
		return portalIds[ Math.RandomLong(0, portalIds.length()-1) ];
	}
	return -1;
}

// adjust vector length
Vector resizeVector( Vector v, float length )
{
	float d = length / sqrt( (v.x*v.x) + (v.y*v.y) + (v.z*v.z) );
	v.x *= d;
	v.y *= d;
	v.z *= d;
	return v;
}

void removeEnt(EHandle entHandle)
{
	if (entHandle) {
		CBaseEntity@ ent = entHandle;
		g_EntityFuncs.Remove(ent);
	}
}

void allowMoreBeamsOnPortal(Portal@ portal)
{
	if (portal !is null)
		portal.canAddBeams = true;
}

int fxid = 0;

void checkMonsterTouching(EHandle entHandle)
{
	
	if (entHandle) 
	{
		CBaseEntity@ ent = entHandle;
		if (ent.pev.oldbuttons-1 >= 0) {
			// Check if player is still touching this portal
			Portal@ p = portals[ent.pev.oldbuttons-1];
			if (p.ent) {
				CBaseEntity@ portalEnt = p.ent;
				Vector portalOri = portalEnt.pev.origin;
				portalOri.z -= 36; // account for monster origin on floor;
				float distance = Vector(portalOri - ent.pev.origin).Length();
				if (distance > portalTouchRadius)
					ent.pev.oldbuttons = -1; // monster moved far enough away
				else // monster still touching the portal
					g_Scheduler.SetTimeout("checkMonsterTouching", 0.5f, entHandle); // check again later
			} else {
				ent.pev.oldbuttons = -1; // can't touch a portal that no longer exists!
			}
		}
	}
}

void teleportEnt(CBaseEntity@ ent, int portalIdx, Vector offset)
{
	Portal@ portal = portals[portalIdx];
	if (ent.IsPlayer() != 0)
	{
		CBasePlayer@ plr = cast< CBasePlayer@ >(@ent);
		PlayerState@ state = getPlayerState(plr);

		if (state.touchingPortal == portalIdx) // don't teleport until player stops touching the portal
			return;
		if (state.editing == portalIdx)
			return; // don't teleport if player is editing it
	}
	else
	{
		if (ent.pev.oldbuttons-1 == portalIdx) // oldbuttons = last portal touched
			return;			
	}
	
	int exitType = portal.type == PORTAL_BIDIR ? PORTAL_BIDIR : PORTAL_EXIT;
	int exitId = portal.target;
	if (exitId == -1 or exitId >= int(portals.length())) { // target is invalid
		portal.target = -1;
		exitId = getRandomPortal(exitType, portalIdx);
	}
	if (exitId != -1) {
		Portal@ exit = portals[exitId];
		if (!exit.enabled or !exit.ent)
			return;
		CBaseEntity@ exitEnt = exit.ent;
		
		float zOffset = 0;
		string cname = ent.pev.classname;
		if (cname == 'player' or cname.Find("monster_") == 0) {
			if (cname == 'monster_snark' or cname == 'monster_satchel')
				zOffset = portalOffset;
			else
				zOffset = -portalOffset;
		}
			
		ent.pev.origin = exitEnt.pev.origin + offset + Vector(0, 0, zOffset);
		
		// Force player to duck just in case the exit is too low
		// This has no side effects if the portal is high enough and ducking isn't required
		if (ent.IsPlayer() != 0)
		{
			ent.pev.flDuckTime = 26;
			ent.pev.flags |= FL_DUCKING;
		}
		
		if (exit.rotateMode != ROTATE_NO)
		{
			Vector oldAngles = ent.pev.angles; // used by monsters only (players use v_angle)
			if (exit.rotateMode == ROTATE_YES)
				ent.pev.angles = exit.angles;
			else if (exit.rotateMode == ROTATE_YAW_ONLY)
			{
				if (ent.IsPlayer() != 0)
					ent.pev.angles = ent.pev.v_angle;
				ent.pev.angles.y = exit.angles.y;
			}
			ent.pev.fixangle = FAM_FORCEVIEWANGLES;
			
			// Rotate the velocity vector to correct yaw
			Vector angleDiff = Vector(0,0,0);
			if (ent.IsPlayer() != 0)
				angleDiff.y = exit.angles.y - ent.pev.v_angle.y;
			else 
				angleDiff.y = exit.angles.y - oldAngles.y;
			ent.pev.velocity = Math.RotateVector(ent.pev.velocity, angleDiff, Vector(0,0,0));
		}
		int beamCount = 2;
		int pitch = 80;
		if (exit.exitSpeed != EXIT_MATCH_INPUT)
		{
			float newSpeed = 0;
			switch(exit.exitSpeed)
			{
				case(EXIT_ZERO): 	   newSpeed = 0; break;
				case(EXIT_SLOW): 	   pitch = 90; newSpeed = 512; break;
				case(EXIT_MEDIUM): 	   pitch = 110; beamCount = 3; newSpeed = 768; break;
				case(EXIT_FAST): 	   pitch = 130; beamCount = 4; newSpeed = 1024; break;
				case(EXIT_SUPERSONIC): pitch = 180; beamCount = 5; newSpeed = 4096; break;
			}
			ent.pev.velocity = Math.RotateVector(Vector(1, 0, 0), exit.angles, Vector(0,0,0));
			ent.pev.velocity = resizeVector(ent.pev.velocity, newSpeed);
		}
		
		// create portal effects
		if (exit.canAddBeams)
		{
			for (int k = 0; k < beamCount; k++)
			{
				string spawnerName = "portal_special_fx" + fxid++;
				dictionary keyvalues;
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
				keyvalues["origin"] = "" + exitEnt.pev.origin.x + " " + exitEnt.pev.origin.y + " " + exitEnt.pev.origin.z;
				CBaseEntity@ specialfx = g_EntityFuncs.CreateEntity( "env_beam", keyvalues, true );
				g_EntityFuncs.FireTargets(spawnerName, null, null, USE_ON);
				EHandle fxhandle = specialfx;
				g_Scheduler.SetTimeout("removeEnt", 0.5f, fxhandle);
			}
			exit.canAddBeams = false;
			g_Scheduler.SetTimeout("allowMoreBeamsOnPortal", 0.7f, @exit);
		}
		
		g_SoundSystem.PlaySound(exitEnt.edict(), CHAN_STATIC, spawn_sound, 0.5f, 1.0f, 0, pitch);
		
		if (exit.type == PORTAL_BIDIR)
		{
			if (ent.IsPlayer() != 0)
			{
				CBasePlayer@ plr = cast< CBasePlayer@ >(@ent);
				PlayerState@ state = getPlayerState(plr);
				state.touchingPortal = exitId;
			}
			else
			{
				ent.pev.oldbuttons = exitId+1;
				EHandle entHandle = ent;
				g_Scheduler.SetTimeout("checkMonsterTouching", 0.5f, entHandle);
			}
		}
	}
}

bool oddUpdate = false;


// TODO: This is super ineffecient
// Rewrite this whenever we get something like an EntityCreated hook
// Looping through all map entities many times over every frame is just stupid
void portalThink()
{		
	if (!portalsEnabled or portals.length() == 0)
		return;
		
	// split up the work into 2 frames for better performance. Fast moving entities might fly past some portals.
	uint begin = 0;
	uint end = portals.length() / 2;
	if (oddUpdate) {
		begin = end;
		end = portals.length();
	}
	oddUpdate = !oddUpdate;
		
	for (uint i = begin; i < end; i++) {
		if (portals[i].type == PORTAL_EXIT)
			continue;
		if (!portals[i].enabled or !portals[i].ent)
			continue;
		CBaseEntity@ portalEnt = portals[i].ent;
			
		
		// teleport players, point entites, and some small monsters 
		CBaseEntity@ ent = null;
		do {
			@ent = g_EntityFuncs.FindEntityInSphere(ent, portalEnt.pev.origin, portalTouchRadius, "*", "classname"); 
			if (ent !is null)
			{
				string cname = string(ent.pev.classname);
				if (cname == "grenade" or cname == "playerhornet" or cname == "rpg_rocket" or 
					cname == "bolt" or cname == "grappletongue" or cname == "displacer_portal" or
					cname == "sporegrenade" or cname == "weaponbox" or cname == "player" or
					cname == "shock_beam" or cname == "gonomespit" or cname == "voltigoreshock" or
					cname == "controller_energy_ball" or cname == "controller_head_ball" or
					cname == "hornet" or cname == "squidspit" or cname == "bmortar" or cname == "garg_stomp" or
					cname == "pitdronespike" or cname == "hvr_rocket" or cname == "kingpin_plasma_ball")
				{
					teleportEnt(ent, int(i), Vector(0,0,0));
				}
				//else if (string(ent.pev.classname).Find("env_") != 0)
				//	println("FOUND: " + ent.pev.classname);
			}
		} while (ent !is null);
		
		// Teleport monsters that have origins on the floor (so, basically all of them)
		Vector monsterTeleOrigin = portalEnt.pev.origin;
		monsterTeleOrigin.z -= 36; // adjust portal height for floor origins
		
		@ent = null;
		do {
			@ent = g_EntityFuncs.FindEntityInSphere(ent, monsterTeleOrigin, portalTouchRadius, "monster_*", "classname"); 
			if (ent !is null)
			{
				teleportEnt(ent, int(i), Vector(0,0,-36));
			}
		} while (ent !is null);
	}
	
	// special func_pushable handling
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
				Vector portalOri = portalEnt.pev.origin;
				
				// calculate origin of pushable ("origin" doesn't always give the correct position)
				Vector pushMiddle = pushable.pev.mins + (pushable.pev.maxs - pushable.pev.mins) / 2.0f;
				Vector pushOri = pushable.pev.origin + pushMiddle;
				
				float distance = Vector(portalOri - pushOri).Length();
				if (distance < portalTouchRadius)
				{
					Vector offset = pushable.pev.origin - pushOri;
					teleportEnt(pushable, int(i), offset);
				}
			}
		}
	} while (pushable !is null);
	
	
	if (oddUpdate) return;
	
	// update player states
	array<string>@ stateKeys = player_states.getKeys();
	for (uint i = 0; i < stateKeys.length(); i++)
	{
		PlayerState@ state = cast<PlayerState@>( player_states[stateKeys[i]] );
		CBaseEntity@ plr = state.plr;
		
		if (state.editing != -1)
		{
			if (state.editing >= int(portals.length())) {
				state.editing = -1;
				state.linkLast = -1;
			}
			//if (portals[state.editing].enabled)
			//	portals[state.editing].disable();
			if (plr.pev.deadflag != 0 and portalsEnabled) {
				portals[state.editing].enable();
				state.editing = -1;
				state.removeConfirm = false;
				state.deletePortal = -1;
				state.linkLast = -1;
			}
		}
		
		if (state.touchingPortal == -1)
			continue;
		if (state.touchingPortal >= int(portals.length()))
		{
			state.touchingPortal = -1;
			continue;
		}
		
		// Check if player is still touching this portal
		Portal@ p = portals[state.touchingPortal];
		if (p.ent) {
			CBaseEntity@ portalEnt = p.ent;
			float distance = Vector(portalEnt.pev.origin - plr.pev.origin).Length();
			if (distance > portalTouchRadius)
				state.touchingPortal = -1; // player moved far enough away
		} else {
			state.touchingPortal = -1; // can't touch a portal that no longer exists!
		}
	}
}

string getPortalOwnerName(CBasePlayer@ caller, Portal@ portal)
{
	string portalOwnerId = portal.owner;
	string callerId = g_EngineFuncs.GetPlayerAuthId( caller.edict() );
	if (callerId == 'STEAM_ID_LAN') {
		callerId = caller.pev.netname;
	}
	if (callerId == portalOwnerId)
		return "your";
	
	array<string>@ stateKeys = player_states.getKeys();
	for (uint i = 0; i < stateKeys.length(); i++)
	{
		PlayerState@ state = cast<PlayerState@>( player_states[stateKeys[i]] );
		CBaseEntity@ plr = state.plr;
		if (plr is null)
			continue;
		
		string steamId = g_EngineFuncs.GetPlayerAuthId( plr.edict() );
		if (steamId == 'STEAM_ID_LAN') {
			steamId = plr.pev.netname;
		}
		
		if (steamId == portalOwnerId)
			return string(plr.pev.netname) + "'s";
	}
	return "unowned";
}

void openPortalMenu(CBasePlayer@ plr)
{
	PlayerState@ state = getPlayerState(plr);
	state.initMenu(plr, portalMenuCallback);
				
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
		if (state.deletePortal >= int(portals.length()))
		{
			state.deletePortal = -1;
		}
		else
		{
			Portal@ p = portals[state.deletePortal];
			title = "Remove " + getPortalOwnerName(plr, p) + " portal?";
		}
		
	}
	else title = title + ":";
	state.menu.SetTitle(title + "\n");
	
	if (state.deletePortal != -1)
	{
		state.menu.AddItem("Yes\n", "delete-yes");
		state.menu.AddItem("No\n", "delete-no");
	}
	else if (state.removeConfirm)
	{
		state.menu.AddItem("Yes\n", "kill-all");
		state.menu.AddItem("No\n", "kill-all-cancel");
		state.menu.AddItem("Only my portals\n", "kill-all-owner");
	}
	else if (state.editing != -1 and state.editing < int(portals.length())) // editing a portal
	{
		Portal@ portal = portals[state.editing];

		if (state.linkLast != -1)
		{
			state.menu.AddItem("Yes\n", "edit-link-confirm");
			state.menu.AddItem("No, the one before it\n", "edit-link-last");
			state.menu.AddItem("No, the one closest to me\n", "edit-link-closest");
			state.menu.AddItem("Unlink current target\n", "edit-link-random");
			state.menu.AddItem("Cancel\n", "edit-link-cancel");
		}
		else
		{
			string target = portal.target == -1 ? "Random" : "portal #" + portal.target;
			state.menu.AddItem("Target: " + target + "\n", "edit-link-last");
			state.menu.AddItem("Update position\n", "edit-position");
			
			if (portal.type == PORTAL_EXIT or portal.type == PORTAL_BIDIR)
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
				switch(portal.rotateMode)
				{
					case(ROTATE_NO): rotateMode = "No"; break;
					case(ROTATE_YES): rotateMode = "Yes"; break;
					case(ROTATE_YAW_ONLY): rotateMode = "Yaw only"; break;
				}
				
				state.menu.AddItem("Update angles: yaw = " + int(portal.angles.y) + "\tpitch = " + int(portal.angles.x) + "\n", "edit-angles");
				state.menu.AddItem("Exit speed: " + exitSpeed + "\n", "edit-speed");
				state.menu.AddItem("Rotate entities: " + rotateMode + "\n", "edit-rotate");
			}
			if (portal.type == PORTAL_ENTER or portal.type == PORTAL_BIDIR)
			{
				if (portal.type == PORTAL_ENTER) {
					state.menu.AddItem("\n", "");
					state.menu.AddItem("\n", "");
					state.menu.AddItem("\n", "");
				}
			}
			
			state.menu.AddItem("Done ", "edit-cancel");
		}
	}
	else
	{
		state.menu.AddItem("Create entrance\n", "add-enter");
		state.menu.AddItem("Create exit\n", "add-exit");
		state.menu.AddItem("Create bidirectional\n", "add-bi");
		
		state.menu.AddItem("Edit portal\n", "edit");
		
		state.menu.AddItem("Delete portal\n", "delete");
		
		if (portalsEnabled)
			state.menu.AddItem("Disable portals\n", "toggle-portals");
		else
			state.menu.AddItem("Enable portals\n", "toggle-portals");
		state.menu.AddItem("Remove all portals", "kill-all");
	}

	state.openMenu(plr);
}

HookReturnCode ClientLeave(CBasePlayer@ leaver)
{
	string steamId = g_EngineFuncs.GetPlayerAuthId( leaver.edict() );
	if (steamId == 'STEAM_ID_LAN') {
		steamId = leaver.pev.netname;
	}
		
	array<string>@ stateKeys = player_states.getKeys();
	for (uint i = 0; i < stateKeys.length(); i++)
	{
		PlayerState@ state = cast<PlayerState@>( player_states[stateKeys[i]] );
		CBaseEntity@ plr = state.plr;
		if (stateKeys[i] == steamId)
		{
			if (state.editing != -1 and state.editing < int(portals.length()))
				portals[state.editing].enable();
				
			state.editing = -1;
			state.plr = null;
			state.touchingPortal = -1;
			state.editing = -1;
			state.linkLast = -1;
			break;
		}
	}
	player_states.delete(steamId);
	
	return HOOK_CONTINUE;
}

HookReturnCode ClientSay( SayParameters@ pParams )
{
	CBasePlayer@ plr = pParams.GetPlayer();
	const CCommand@ args = pParams.GetArguments();	
	
	PlayerState@ state = getPlayerState(plr);
	
	if ( args.ArgC() > 0 )
	{
		if ( args[0] == ".portal_menu" )
		{
			pParams.ShouldHide = true;
			
			if (g_PlayerFuncs.AdminLevel(plr) < ADMIN_YES)
			{
				g_PlayerFuncs.SayText(plr, "You don't have access to that command, peasent.\n");
				return HOOK_HANDLED;
			}
	
			state.editing = -1;
			state.linkLast = -1;
			openPortalMenu(plr);

			return HOOK_HANDLED;
		}
	}
	return HOOK_CONTINUE;
}

void PortalMenu( const CCommand@ args )
{
	CBasePlayer@ plr = g_ConCommandSystem.GetCurrentPlayer();
	
	if (g_PlayerFuncs.AdminLevel(plr) < ADMIN_YES)
	{
		g_PlayerFuncs.SayText(plr, "You don't have access to that command, peasent.\n");
		return;
	}
	
	PlayerState@ state = getPlayerState(plr);

	state.editing = -1;
	state.linkLast = -1;
	openPortalMenu(plr);
}