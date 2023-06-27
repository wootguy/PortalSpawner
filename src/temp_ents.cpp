#include "temp_ents.h"
#include "const.h"

void te_beaments(edict_t* start, edict_t* end,
	const char* sprite, int frameStart,
	int frameRate, int life, int width, int noise,
	Color c, int scroll,
	int msgType, edict_t* dest) {

	MESSAGE_BEGIN(msgType, SVC_TEMPENTITY, NULL, dest);
	WRITE_BYTE(TE_BEAMENTS);
	WRITE_SHORT(ENTINDEX(start));
	WRITE_SHORT(ENTINDEX(end));
	WRITE_SHORT(g_engfuncs.pfnModelIndex(sprite));
	WRITE_BYTE(frameStart);
	WRITE_BYTE(frameRate);
	WRITE_BYTE(life);
	WRITE_BYTE(width);
	WRITE_BYTE(noise);
	WRITE_BYTE(c.r);
	WRITE_BYTE(c.g);
	WRITE_BYTE(c.b);
	WRITE_BYTE(c.a); // actually brightness
	WRITE_BYTE(scroll);
	MESSAGE_END();
}

void te_tracer(Vector start, Vector end,
	int msgType, edict_t* dest)
{
	MESSAGE_BEGIN(msgType, SVC_TEMPENTITY, NULL, dest);
	WRITE_BYTE(TE_TRACER);
	WRITE_COORD(start.x);
	WRITE_COORD(start.y);
	WRITE_COORD(start.z);
	WRITE_COORD(end.x);
	WRITE_COORD(end.y);
	WRITE_COORD(end.z);
	MESSAGE_END();
}

void te_killbeam(edict_t* target,
	int msgType, edict_t* dest)
{
	MESSAGE_BEGIN(msgType, SVC_TEMPENTITY, NULL, dest);
	WRITE_BYTE(TE_KILLBEAM);
	WRITE_SHORT(ENTINDEX(target));
	MESSAGE_END();
}