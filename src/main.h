#pragma once
#include "Portal.h"
#include "private_api.h"

class PlayerState {
public:
	int touchingPortal = -1;   // portal ID player is currently touching (-1 if not touching any)
	int editing = -1; 		  // portalId currently being edited
	bool removeConfirm = false;   // player should confirm if he wants ALL portals removed
	int deletePortal = -1;     // player should confirm if he wants to delete another player's portal
	int linkLast = -1; 		  // player is choosing a previous portal to link to
	int menuPage = 0;         // custom menu paging. I don't like the default paging >:|
};

enum portal_types
{
	PORTAL_ENTER,
	PORTAL_EXIT,
	PORTAL_BIDIR,
};

enum exit_speeds
{
	EXIT_MATCH_INPUT,
	EXIT_ZERO,
	EXIT_SLOW,
	EXIT_MEDIUM,
	EXIT_FAST,
	EXIT_SUPERSONIC,
	EXIT_SPEEDS
};

enum rotate_modes
{
	ROTATE_NO,
	ROTATE_YES,
	ROTATE_YAW_ONLY,
	ROTATE_MODES
};

void openPortalMenu(EHandle h_plr);
bool saveMapPortals();
void loadMapPortals();
