#include "meta_init.h"
#include <string>

// use these functions to communicate with the MetaHelper angelscript plugin

// call a metamod function whenever an angelscript hook is triggered
// valid hook types: TakeDamage
void hook_angelscript(std::string hook, std::string cmdname, void (*callback)());

// read a custom keyvalue from an entity
int readCustomKeyvalueInteger(edict_t* ent, std::string keyName);
float readCustomKeyvalueFloat(edict_t* ent, std::string keyName);
std::string readCustomKeyvalueString(edict_t* ent, std::string keyName);
Vector readCustomKeyvalueVector(edict_t* ent, std::string keyName);

void TakeDamage(edict_t* victim, edict_t* inflictor, edict_t* attacker, float damage, int damageType);