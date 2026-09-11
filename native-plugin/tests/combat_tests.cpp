// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#include "../src/combat_math.hpp"
#include <iostream>
#include <cstdlib>
void check(bool value,const char* message){if(!value){std::cerr<<message<<"\n";std::exit(1);}}
int main(){
 // Body-switch calls made directly by furniture must obey camera ownership,
 // regardless of whether DialogueMenu is open or the POV flag already says 3P.
 for(bool nativeRequest:{false,true}){
  check(!combat::renderedFirstPerson(true,true,nativeRequest),"Owned furniture body switched to first person");
  check(combat::renderedFirstPerson(true,false,nativeRequest)==nativeRequest,"Pip-Boy/intro body switch blocked");
  check(combat::renderedFirstPerson(false,true,nativeRequest)==nativeRequest,"Non-player body switch modified");
 }
 for(int action:{2,3,4,5,6})check(combat::nativeSwing(action),"Native attack recovery interrupted");
 for(int action:{-1,0,1,7,8,9,10})check(!combat::nativeSwing(action),"Non-attack action stalls pursuit");
 check(!combat::mayLeaveScriptedStartup(true,false,true,false),"Bed animation interrupted");
 check(!combat::mayLeaveScriptedStartup(true,false,false,true),"Scripted movement lock interrupted");
 check(!combat::mayLeaveScriptedStartup(false,true,false,false),"Opening dialogue interrupted");
 check(combat::mayLeaveScriptedStartup(true,false,false,false),"Normal controls never release startup");
 // Native SayTo dialogue can leave gameMode true and no DialogueMenu open.
 for(int stage: {0,3,10,15,17,30,36,40,45,50,54}){
  check(combat::openingOwnsPlayer(true,true,stage),"Opening camera released before appearance/stand-up");
  check(!combat::mayLeaveScriptedStartup(true,false,false,false,combat::openingOwnsPlayer(true,true,stage)),"Gap between intro lines enabled interactions");
 }
 check(combat::openingOwnsPlayer(false,true,15),"Loading an intro save bypassed startup guard");
 check(!combat::openingOwnsPlayer(false,false,0),"Ordinary save without opening quest was blocked");
 check(!combat::openingOwnsPlayer(true,true,55),"Appearance-complete walking stage never released");
 for(bool newGame:{false,true}){
  bool opening=combat::openingOwnsPlayer(newGame,true,55);
  check(combat::mayLeaveScriptedStartup(true,false,false,combat::nativeMovementLocked(0x0C),opening),"Native handoff stayed blocked by unrelated fight/Pip-Boy restrictions");
  check(!combat::mayLeaveScriptedStartup(true,false,false,combat::nativeMovementLocked(0x0D),opening),"Stage handoff bypassed scripted movement lock");
  check(!combat::mayLeaveScriptedStartup(false,true,false,false,opening),"Stage handoff interrupted dialogue");
 }
 check(!combat::mayLeaveScriptedStartup(true,false,false,combat::nativeMovementLocked(0x59)),"Native scripted control lock was ignored");
 check(combat::mayLeaveScriptedStartup(true,false,false,combat::nativeMovementLocked(0x0C)),"Pip-Boy/combat restriction incorrectly blocked walking");
 check(combat::nativeCombatLocked(0x0C),"Opening combat restriction was ignored");
 auto north=combat::aim({0,0,0},{0,100,0});check(std::abs(north.yaw)<.001f,"North heading incorrect");
 auto east=combat::aim({0,0,0},{100,0,0});check(std::abs(east.yaw-1.5707963f)<.001f,"East heading incorrect");
 check(combat::aim({0,0,0},{0,100,100}).pitch<0,"Upward aim has wrong pitch sign");
 check(combat::aim({0,0,0},{0,100,-100}).pitch>0,"Downward aim has wrong pitch sign");
 check(combat::canAttack(100,200,true,false,true,true,false),"Clear in-range attack rejected");
 check(combat::canAttack(300,200,true,false,true,true,false),"Distant ranged shot rejected");
 check(combat::canAttack(100,200,false,false,true,true,false),"Obstructed ranged shot rejected");
 check(!combat::canAttack(100,200,true,true,true,true,false),"Moving attack accepted");
 check(!combat::canAttack(100,200,true,false,false,true,false),"Menu attack accepted");
 check(!combat::canAttack(100,200,true,false,true,false,false),"Dead target attack accepted");
 // Obstruction never overrides other ranged safety gates; melee retains reach checks.
 check(!combat::canAttack(100,200,false,false,true,true,true),"Obstructed melee accepted");
 check(!combat::canEngage(100,200,false,true,true,true),"Obstructed melee stopped pursuit");
 check(combat::canAttack(100,200,true,false,true,true,true),"Clear melee rejected");
 check(!combat::canAttack(300,200,true,false,true,true,true),"Out-of-reach melee accepted");
 check(combat::canEngage(12000,200,false,true,true,false),"Ranged target unnecessarily starts pursuit");
 check(combat::canAttack(300,200,false,false,true,true,false),"Distant obstructed ranged attempt rejected");
 check(!combat::canAttack(100,200,false,true,true,true,false),"Blocked shot bypassed movement gate");
 check(!combat::canAttack(100,200,false,false,false,true,false),"Blocked shot bypassed menu gate");
 check(!combat::canAttack(100,200,false,false,true,false,false),"Blocked shot bypassed dead-target gate");
 auto muzzleAim=combat::aim({20,0,70},{100,100,70});
 check(std::abs(muzzleAim.pitch)<.001f,"Level muzzle-to-torso shot aims vertically");
 check(std::abs(muzzleAim.yaw-std::atan2(80.f,100.f))<.001f,"Muzzle lateral offset ignored");
 auto crouched=combat::aim({20,0,70},{100,100,40});check(crouched.pitch>0,"Lower torso target not tracked");
 // Engagement is decided before movement is stopped, independent of a pending route.
 check(combat::canEngage(199,200,true,true,true,false),"In-range chase did not yield to attack");
 check(combat::canEngage(199,200,false,true,true,false),"Obstructed in-range target kept chasing");
 check(combat::refreshPursuit(100,0,0,1000,true,true),"First approach waited for timer");
 check(!combat::refreshPursuit(1249,1000,500,1000,true,true),"Moving target bypassed bounded update rate");
 check(combat::refreshPursuit(1250,1000,100,1000,true,true),"Stale pending route prevented pursuit update");
 check(!combat::refreshPursuit(1250,1000,5,1000,true,false),"Minor target movement restarted pursuit");
 check(combat::refreshPursuit(1250,1000,0,1000,false,false),"Ended approach delayed fresh pursuit");
 check(!combat::refreshPursuit(1250,1000,0,1000,false,true),"Unchanged pending search restarted");
 check(combat::meleeDistance({0,0,0},{60,0,0})==60,"Melee range used weapon height");
 check(combat::meleeDistance({0,0,0},{60,0,200})==200,"Melee attacked across floors");
 check(combat::useAimDownSights(600,false,false),"Distant ranged attack did not aim");
 check(!combat::useAimDownSights(300,false,true),"Close ranged attack retained sights");
 check(combat::useAimDownSights(450,false,true)&&!combat::useAimDownSights(450,false,false),"Aim threshold oscillates");
 check(!combat::useAimDownSights(600,true,true),"Melee activated aim/block control");
 // Load/new-game ownership is available before gameplay or dialogue polling.
 check(combat::startupCameraReady(true,false,true,true,true),"Loaded startup camera not armed");
 check(!combat::startupCameraReady(true,false,true,false,true),"Preload/title/failed-load camera armed");
 check(!combat::startupCameraReady(true,false,true,true,false),"Missing loaded player camera armed");
 check(!combat::startupCameraReady(false,false,true,true,true),"Disabled auto-start acquired camera");
 check(!combat::startupCameraReady(true,true,true,true,true),"Enabled camera repeated startup");
 check(!combat::startupCameraReady(true,false,false,true,true),"Missing hooks acquired camera");
 // Menu, dialogue, furniture and native POV are deliberately not ownership inputs.
 check(combat::keepCamera(true,true,false),"Loaded world lost camera ownership");
 check(!combat::keepCamera(true,false,false),"Missing world retained camera");
 check(!combat::keepCamera(false,true,false),"Disabled mod retained camera");
 check(!combat::keepCamera(true,true,true),"Explicit disable failed to release camera");
 for(uint32_t mode:{1u,2u,3u,4u})check(!combat::keepCamera(true,true,false,mode),"Pip-Boy camera not released during transition");
 check(combat::keepCamera(true,true,false,0),"Closing Pip-Boy did not restore isometric ownership");
 check(combat::restoreThirdPersonBody(true,false,true,false,false),"Dialogue menu failed to restore player body");
 check(combat::restoreThirdPersonBody(true,false,true,true,false),"Seated dialogue failed to retain player body");
 check(!combat::restoreThirdPersonBody(false,false,true,false,false),"Pip-Boy or disabled camera forced third person");
 check(!combat::restoreThirdPersonBody(true,false,false,false,false),"Unrelated menu forced third person");
 check(combat::restoreThirdPersonBody(true,true,false,true,false),"Seated SayTo scene left isometric body in first person");
 check(combat::restoreThirdPersonBody(true,true,false,false,false),"Standing after dialogue left first-person arms active");
 check(combat::suppressNativeLook(true,true,false),"Seated gameplay leaked mouse-look into native POV");
 check(!combat::suppressNativeLook(true,false,true),"Dialogue choices lost their native mouse");
 check(!combat::suppressNativeLook(true,false,false),"Native menu lost its mouse");
 check(!combat::suppressNativeLook(false,true,false),"Intro or disabled mod blocked native looking");
 check(!combat::restoreThirdPersonBody(true,false,true,false,true),"Visible dialogue body triggered repeated POV changes");
 check(combat::meleeHeading({0,0,0},{1,-1,40},1.2f,.016f)==1.2f,"Overlapping melee target spun the player");
 check(std::abs(combat::meleeHeading({0,0,0},{100,0,0},0,.016f))<.101f,"Melee rotation exceeded its rate limit");
 check(combat::meleeHeading({0,0,0},{-1,-100,0},3.13f,.016f)>3.13f,"Melee yaw wrap took the long rotation");
 check(!combat::meleeFacing({0,0,0},{100,0,0},0),"Melee attack accepted while facing sideways");
 check(combat::meleeFacing({0,0,0},{0,100,0},0),"Aligned melee attack blocked");
 check(combat::meleeFacing({0,0,0},{1,1,0},3),"Overlapping target requires unstable heading");
 combat::SightsGate sights;
 check(!sights.ready(1000,true,false),"Submitted aim input treated as native aiming");
 check(!sights.ready(1100,true,true),"Native aim had no settling interval");
 check(sights.ready(1220,true,true),"Settled native aim blocked firing");
 check(!sights.ready(1230,true,false),"Lost native aim allowed firing");
 check(!sights.ready(1400,true,true),"Reacquired aim reused stale settling time");
 check(sights.ready(1401,false,false),"Hip fire required native sights");
 combat::ShotHold shot;shot.begin(1000);
 check(shot.update(1016,false),"Queued shot released aim on following frame");
 check(shot.update(1100,true),"Active firing animation released aim");
 check(!shot.update(1250,false),"Finished native action retained aim");
 shot.begin(2000);check(shot.update(3000,false),"Delayed native shot lost its aim");
 check(!shot.update(3500,false),"Rejected shot never released aim");
 for(engine::Vec endpoint: {engine::Vec{100,200,75},engine::Vec{-800,1500,400},engine::Vec{0,-400,-20}}){
  engine::Vec origin{20,10,70};auto expected=engine::normalized(endpoint-origin);
  check(engine::length(combat::direction(combat::aim(origin,endpoint))-expected)<.0001f,"Aim laser direction differs from actor yaw/pitch convention");
 }
 std::cout<<"World aim and combat gating checks passed.\n";
}
