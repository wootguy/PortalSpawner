#pragma once
#pragma pack(push,1)

// This code was automatically generated by the ApiGenerator plugin.
// Prefer updating the generator code instead of editing this directly.
// "u[]" variables are unknown data.

// Example entity: func_tank
class CBaseTank : public CBaseEntity {
public:
    byte u10_0[184];
    float m_yawCenter; // "Center" yaw
    float m_yawRate; // Max turn rate to track targets
    float m_yawRange; // Range of turning motion (one-sided: 30 is +/- 30 degress from center)<br>Zero is full rotation
    float m_pitchCenter; // "Center" pitch
    byte u10_1[8];
    float m_flNextAttack; // Next attack time
    vec3_t m_vecControllerUsePos; // Start origin of the player that is currently controlling this tank.<br>Used to determine when a player has moved too far to continue controlling this tank.
    float m_yawTolerance; // Tolerance angle
    float m_pitchRate; // Max turn rate on pitch
    float m_pitchRange; // Range of pitch motion as above
    float m_pitchTolerance; // Tolerance angle
    float m_fireLast; // Last time I fired
    float m_fireRate; // How many rounds/second
    float m_lastSightTime; // Last time I saw target
    float m_persist; // Persistence of firing (how long do I shoot when I can't see)
    float m_minRange; // Minimum range to aim/track
    float m_maxRange; // Max range to aim/track
    vec3_t m_barrelPos; // Length of the freakin barrel
    float m_spriteScale; // Scale of any sprites we shoot
    byte u10_2[8];
    int m_bulletType; // Bullet type
    int m_iBulletDamage; // 0 means use Bullet type's default damage
    vec3_t m_sightOrigin; // Last sight of target
    int m_spread; // firing spread
    string_t m_iszMaster; // Master entity (game_team_master or multisource)
};
#pragma pack(pop)