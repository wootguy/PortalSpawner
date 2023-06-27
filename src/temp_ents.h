#pragma once
#include "meta_init.h"
#include "misc_utils.h"

class Color {
public:
	uint8_t r, g, b, a;

	Color() { r = g = b = a = 0; }
	Color(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a = 255) { r = _r; g = _g; b = _b; a = _a; }
	Color(Vector v) { r = int(v.x); g = int(v.y); b = int(v.z); a = 255; }
};

const Color RED(255, 0, 0);
const Color GREEN(0, 255, 0);
const Color BLUE(0, 0, 255);
const Color WHITE(255, 255, 255);

void te_beaments(edict_t* start, edict_t* end,
	const char* sprite = "sprites/laserbeam.spr", int frameStart = 0,
	int frameRate = 100, int life = 10, int width = 32, int noise = 1,
	Color c = BLUE, int scroll = 32,
	int msgType = MSG_BROADCAST, edict_t* dest = NULL);

void te_tracer(Vector start, Vector end,
	int msgType = MSG_BROADCAST, edict_t* dest = NULL);

void te_killbeam(edict_t* target,
	int msgType = MSG_BROADCAST, edict_t* dest = NULL);