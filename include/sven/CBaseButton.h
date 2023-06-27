#pragma once
#pragma pack(push,1)

// This code was automatically generated by the ApiGenerator plugin.
// Prefer updating the generator code instead of editing this directly.
// "u[]" variables are unknown data.

// Example entity: func_button
class CBaseButton : public CBaseToggle {
public:
    byte u4_0[4];
    bool m_fStayPushed; // button stays pushed in until touched again?
    bool m_fRotating; // a rotating button?  default is a sliding button.
};
#pragma pack(pop)