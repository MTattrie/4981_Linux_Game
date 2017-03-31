/*
    Created by Maitiu Morton 2/8/2017
        Edited by DericM 3/8/2017
*/
#include "Rifle.h"

Rifle::Rifle(int32_t id) : InstantWeapon(RifleVars::TYPE, TEXTURES::RIFLE, RifleVars::FIRESOUND, RifleVars::HITSOUND, RifleVars::RELOADSOUND,
        RifleVars::EMPTYSOUND, RifleVars::RANGE, RifleVars::DAMAGE, RifleVars::AOE, RifleVars::PENETRATION,
        RifleVars::CLIP, RifleVars::CLIPMAX, RifleVars::AMMO, RifleVars::RELOADDELAY, RifleVars::FIREDELAY, id) {

}
