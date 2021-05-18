/*

  Paragon 2021 - by Mike from PinballHelp.com

  code/bsos/work/ParagonBase.ino
  
  working code file
  
  0.0.1 - 4-7-21 - Slightly modified version of bsos/pinballbase.ino that compiles with Paragon definitions
  
  version
  0.002 - stable version with 3 attract modes


Things to do:

 * create paragon_award(PSaucerValue);  // 1-8 p-a-r-a-g-o-n special and corresponding show_paragon_lights()
 * manually fire 3-bank during diagnostic coil test (which # coil?) 
   

*/
#include "BallySternOS.h"
#include "ParagonBase.h"
#include "SelfTestAndAudit.h"
#include "BSOS_Config.h"
#include <EEPROM.h>          // needed for EEPROM.read() etc.

#define MAJOR_VERSION  2021  // update TRIDENT2020_MAJOR_VERSION references to just this
#define MINOR_VERSION  1

#define DEBUG_MESSAGES  1    // enable serial debug logging

#define ENABLE_MATCH         // enable match mode (uses 2% program space)
#define ENABLE_ATTRACT       // enable additional attract mode code (for debugging)

// Wav Trigger defines have been moved to BSOS_Config.h
#if defined(USE_WAV_TRIGGER) || defined(USE_WAV_TRIGGER_1p3)
#include "SendOnlyWavTrigger.h"
SendOnlyWavTrigger wTrig;             // Our WAV Trigger object
#endif


// MachineState
//  0 - Attract Mode
//  negative - self-test modes
//  positive - game play
int MachineState = 0;
boolean MachineStateChanged = true;
#define MACHINE_STATE_ATTRACT         0
#define MACHINE_STATE_INIT_GAMEPLAY   1
#define MACHINE_STATE_INIT_NEW_BALL   2
#define MACHINE_STATE_UNVALIDATED     3
#define MACHINE_STATE_NORMAL_GAMEPLAY 4
#define MACHINE_STATE_COUNTDOWN_BONUS 90
#define MACHINE_STATE_MATCH_MODE      95
#define MACHINE_STATE_BALL_OVER       100
#define MACHINE_STATE_GAME_OVER       110

// from Trident
#define MACHINE_STATE_ADJUST_FREEPLAY           -17
#define MACHINE_STATE_ADJUST_BALL_SAVE          -18
#define MACHINE_STATE_ADJUST_MUSIC_LEVEL        -19
#define MACHINE_STATE_ADJUST_TOURNAMENT_SCORING -20
#define MACHINE_STATE_ADJUST_TILT_WARNING       -21
#define MACHINE_STATE_ADJUST_AWARD_OVERRIDE     -22
#define MACHINE_STATE_ADJUST_BALLS_OVERRIDE     -23
#define MACHINE_STATE_ADJUST_SCROLLING_SCORES   -24
#define MACHINE_STATE_ADJUST_EXTRA_BALL_AWARD   -25
#define MACHINE_STATE_ADJUST_SPECIAL_AWARD      -26
#define MACHINE_STATE_ADJUST_DIM_LEVEL          -27
#define MACHINE_STATE_ADJUST_DONE               -28

// Eventually game mode definitions will go here
#define GAME_MODE_SKILL_SHOT                    0
#define GAME_MODE_UNSTRUCTURED_PLAY             1
#define GAME_MODE_MINI_GAME_QUALIFIED           2

#define TIME_TO_WAIT_FOR_BALL         100

// game specific timing
#define SAUCER_PARAGON_DURATION         2000

// enhanced settings copied from Trident2020 ---------------------

#define EEPROM_BALL_SAVE_BYTE           100
#define EEPROM_FREE_PLAY_BYTE           101
#define EEPROM_MUSIC_LEVEL_BYTE         102
#define EEPROM_SKILL_SHOT_BYTE          103
#define EEPROM_TILT_WARNING_BYTE        104
#define EEPROM_AWARD_OVERRIDE_BYTE      105
#define EEPROM_BALLS_OVERRIDE_BYTE      106
#define EEPROM_TOURNAMENT_SCORING_BYTE  107
#define EEPROM_SCROLLING_SCORES_BYTE    110
#define EEPROM_DIM_LEVEL_BYTE           113
#define EEPROM_EXTRA_BALL_SCORE_BYTE    140
#define EEPROM_SPECIAL_SCORE_BYTE       144

// Sound effect defines are in ParagonBase.h

// playfield items

#define TARGET1_MASK     0x01   // keeps track of drop targets that are down, binary masks for DropsHit
#define TARGET2_MASK     0x02
#define TARGET3_MASK     0x04

#define INLINE1_MASK     0x08   // inlines 1-4
#define INLINE2_MASK     0x16   // inlines 1-4
#define INLINE3_MASK     0x32   // inlines 1-4
#define INLINE4_MASK     0x64   // inlines 1-4

#define SAUCER_DEBOUNCE_TIME  500   // #ms to debounce the saucer hits
#define SAUCER_HOLD_TIME 1000   // amount to freeze letter when hit
#define PARAGON_TIMING   250    // ms to change paragon letters in saucer

byte DropsHit = 0;              // holds mask value indicating which of drops have been hit

// mode times & game limits

#define SKILL_SHOT_DURATION             15

#define MAX_DISPLAY_BONUS               100 // for trident: 55
#define TILT_WARNING_DEBOUNCE_TIME      1000


/*********************************************************************

    Machine state and options

*********************************************************************/

// Game/machine global variables
unsigned long HighScore = 0;

int Credits = 0; // was a byte in Trident2020
int MaximumCredits = 50;
boolean FreePlayMode = false;

byte CurrentPlayer = 0;
byte CurrentBallInPlay = 1;
byte CurrentNumPlayers = 0;
unsigned long CurrentScores[4];
boolean SamePlayerShootsAgain = false;

// from Trident - ino
unsigned long CurrentPlayerCurrentScore = 0;
byte Bonus;
byte BonusX;
byte StandupsHit[4];  // # of standups hit per player
byte CurrentStandupsHit = 0;

unsigned long AwardScores[3];
byte MusicLevel = 3;
unsigned long ExtraBallValue = 0;
unsigned long SpecialValue = 0;
byte DimLevel = 2;
byte ScoreAwardReplay = 0;
boolean HighScoreReplay = true;
boolean MatchFeature = true;           // not in config, defined here
byte SpecialLightAward = 0;
boolean TournamentScoring = false;
boolean ResetScoresToClearVersion = false;
boolean ScrollingScores = true;
unsigned long LastSpinnerHitTime = 0;
unsigned long GameModeStartTime = 0;
unsigned long GameModeEndTime = 0;
unsigned long LastTiltWarningTime = 0;

byte GameMode=GAME_MODE_SKILL_SHOT;
byte MaxTiltWarnings = 2;
byte NumTiltWarnings = 0;

unsigned long CurrentTime = 0;
unsigned long BallTimeInTrough = 0;
unsigned long BallFirstSwitchHitTime = 0;

boolean BallSaveUsed = false;
byte BallSaveNumSeconds = 0;
byte BallsPerGame = 3;

boolean ExtraBallCollected = false;

boolean EnableSkillFx = true;          // enable skill light effects in award section
byte SkillShotsCollected[4];           // track # skill shots collected per player
byte SkillShotValue=0;                 // 2=20k 5=special
unsigned long SkillSweepTime=0;        // time of last skill shot light sweep
#define SKILL_SHOT_SWEEP_TIME 600      // ms to sweep skill shot values
#define SKILL_SHOT_DECREASE   100      // # ms to decrease sweep period for each completed ss
#define SKILL_SHOT_MODE_TIME  30000    // skill shot available for 30 seconds
unsigned int SkillSweepPeriod=SKILL_SHOT_SWEEP_TIME; // variable that changes

// game specific

//unsigned long DropTargetClearTime = 0; // not used

unsigned long SaucerHitTime = 0;       // used for debouncing saucer hit
byte ParagonValue = 0;                 // 1-8 p-a-r-a-g-o-n special
byte ParagonLit[4];                    // which paragon letters are lit, bits 1-7 or 8
unsigned long LastParagonLetterTime=0; //
boolean MoveParagon=true;              // false = don't sweep paragon
byte GoldenSaucerValue = 1;            // 1-10 (value * 2000= points)
byte GoldenSaucerMem[4];               // 0-10, 2k-20k carries from ball to ball
byte WaterfallValue=0;                 // 0=1k 1=5k, 2=10k 3=special
boolean WaterfallSpecialAwarded=false; // per ball
byte RightDropsValue=1;                // 10k,15,20,25,special
byte SpinnerValue=1;                   // 1-6, when 6 add bonus, 100 per spin
byte TreasureValue=1;                  // 5000 + 5x bonus multiplier, extra ball, then special

byte BonusMem[4];                      // carried over bonus scoring

// Written in by Mike - yy
byte CurrentDropTargetsValid = 0;         // bitmask showing which drop targets up right:b,m,t, inline 1-4 1-64 bits
byte DropSequence=0;                      // 1-3 # right drops hit in sequence

// special modes
boolean HuntMode=false;                // true if hunt underway (Challenge)
byte HuntsCompleted[4]={0, 0, 0, 0};   // # hunts completed per player
unsigned long HuntStartTime=0;
//unsigned long HuntFreezeTime=0;        
unsigned long HuntShotTime=0;          // freeze will add
boolean HuntFrozen=false;              // whether been frozen in this position or not, only allowed once per location
byte HuntLocation=0;                   // location on PF for hunt target, 0=inlines, 1=upper drop, 2=spinner, 3=lower drops, 4=golden cliffs, 5=standup, 6=paragon
byte HuntSwitches[6]={                 // switch # for hit location
  SW_DROP_TOP,                         // 1
  SW_SPINNER,
  SW_DROP_BOTTOM,
  SW_SAUCER_GOLDEN,
  SW_BOTTOM_STANDUP,
  SW_SAUCER_PARAGON
};
byte HuntLastShot=0;                  // previous location for hunt
#define HUNT_MODE_LENGTH  45000       // 45 seconds
#define HUNT_BASE_SHOT_LENGTH  4000   // 4 sec max time default per shot
unsigned int HuntShotLength=0;        // length of time the shot actually stays dep on hunts
#define HUNT_BASE_REWARD  2500        // base reward level (x10)
#define HUNT_SLING_VALUE  5           // amount to add to hunt reward based on slings
#define HUNT_BEAST_VALUE  50          // "  beast bumper hits
#define HUNT_POPS_VALUE   10          // "  pops hit
unsigned int HuntReward=0;            // assigned when hunt starts max=65535 (x10) HUNT_BASE_REWARD*(HuntsCompleted+1) - increased based on pops and bumpers / reset each ball

// NOTE: we should display HuntReward value in status


// values that carry over from ball-to-ball
unsigned int DropsRightDownScore[4];      // reward for all right drops down
byte BonusHeld[4];                        // bits: 1=20k, 2=30k, 4=40k

////////////////////////////////////////////////////////////////////////////
//
//  Machine/Hardware-specific code
//
////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------
void RunDemo() {
/* 
    BSOS_PushToTimedSolenoidStack(SOL_CENTER_BUMPER, 4, CurrentTime+5000);
    BSOS_PushToTimedSolenoidStack(SOL_LEFT_BUMPER, 4, CurrentTime+5200);
    BSOS_PushToTimedSolenoidStack(SOL_RIGHT_BUMPER, 4, CurrentTime+5400);
/**/  

 
}

// ----------------------------------------------------------------
void RunSkillShotMode() {
  if (GameMode==GAME_MODE_SKILL_SHOT) {

    if ((BallFirstSwitchHitTime>0) || (CurrentTime-GameModeStartTime>SKILL_SHOT_MODE_TIME)) { // time out skill mode
      GameMode=GAME_MODE_UNSTRUCTURED_PLAY;
      ResetLights();
    }

    if (CurrentTime-SkillSweepTime>SkillSweepPeriod) {
      SkillShotValue++;
      if (SkillShotValue>6) SkillShotValue=0;
      SkillSweepTime=CurrentTime;
      if (SkillShotValue==2) { BSOS_SetLampState(L_20K_GOLDEN, 1,0,20); } else { BSOS_SetLampState(L_20K_GOLDEN, 0); }
      if (SkillShotValue==5) { BSOS_SetLampState(L_SAUCER_SPECIAL, 1,0,20); } else { BSOS_SetLampState(L_SAUCER_SPECIAL, 0); }
    }      
  }

}
// ----------------------------------------------------------------
void reset_3bank() {
  // Paragon-specific - resets right 3-bank drop targets on continuous solenoid line

  if (BSOS_ReadSingleSwitchState(SW_DROP_TOP) || BSOS_ReadSingleSwitchState(SW_DROP_MIDDLE) || BSOS_ReadSingleSwitchState(SW_DROP_BOTTOM)) {
    BSOS_SetCoinLockout(true, 0x10);
    delay(125);
    BSOS_SetCoinLockout(false, 0x10);
  }
  CurrentDropTargetsValid = CurrentDropTargetsValid | 7; // turn on bits 1-3
  DropSequence=0;  
}
// ----------------------------------------------------------------
/* trick to print binary of byte

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

  printf("Leading text "BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(byte));
*/
// ----------------------------------------------------------------
void reset_inline() {
  
  if (BSOS_ReadSingleSwitchState(SW_DROP_INLINE_A) || BSOS_ReadSingleSwitchState(SW_DROP_INLINE_B) || BSOS_ReadSingleSwitchState(SW_DROP_INLINE_C) || BSOS_ReadSingleSwitchState(SW_DROP_INLINE_D)) {
    BSOS_PushToTimedSolenoidStack(SOL_DROP_INLINE, 12, CurrentTime+300);
  }
  CurrentDropTargetsValid = CurrentDropTargetsValid | 120; // turn on bits 4-7 
}
// ----------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////
//
//  Machine Generic code
//
////////////////////////////////////////////////////////////////////////////
byte ReadSetting(byte setting, byte defaultValue) {
  byte value = EEPROM.read(setting);
  if (value == 0xFF) {
    EEPROM.write(setting, defaultValue);
    return defaultValue;
  }
  return value;
}
// ----------------------------------------------------------------
void ReadStoredParameters() {
  HighScore = BSOS_ReadULFromEEProm(BSOS_HIGHSCORE_EEPROM_START_BYTE, 10000);
  Credits = BSOS_ReadByteFromEEProm(BSOS_CREDITS_EEPROM_BYTE);
  if (Credits > MaximumCredits) Credits = MaximumCredits;

  ReadSetting(EEPROM_FREE_PLAY_BYTE, 0);
  FreePlayMode = (EEPROM.read(EEPROM_FREE_PLAY_BYTE)) ? true : false;

  BallSaveNumSeconds = ReadSetting(EEPROM_BALL_SAVE_BYTE, 15);
  if (BallSaveNumSeconds > 20) BallSaveNumSeconds = 20;

  MusicLevel = ReadSetting(EEPROM_MUSIC_LEVEL_BYTE, 3);
  if (MusicLevel > 3) MusicLevel = 3;

  TournamentScoring = (ReadSetting(EEPROM_TOURNAMENT_SCORING_BYTE, 0)) ? true : false;

  MaxTiltWarnings = ReadSetting(EEPROM_TILT_WARNING_BYTE, 2);
  if (MaxTiltWarnings > 2) MaxTiltWarnings = 2;

  byte awardOverride = ReadSetting(EEPROM_AWARD_OVERRIDE_BYTE, 99);
  if (awardOverride != 99) {
    ScoreAwardReplay = awardOverride;
  }

  byte ballsOverride = ReadSetting(EEPROM_BALLS_OVERRIDE_BYTE, 99);
  if (ballsOverride == 3 || ballsOverride == 5) {
    BallsPerGame = ballsOverride;
  } else {
    if (ballsOverride != 99) EEPROM.write(EEPROM_BALLS_OVERRIDE_BYTE, 99);
  }

  ScrollingScores = (ReadSetting(EEPROM_SCROLLING_SCORES_BYTE, 1)) ? true : false;

  ExtraBallValue = BSOS_ReadULFromEEProm(EEPROM_EXTRA_BALL_SCORE_BYTE);
  if (ExtraBallValue % 1000 || ExtraBallValue > 100000) ExtraBallValue = 20000;

  SpecialValue = BSOS_ReadULFromEEProm(EEPROM_SPECIAL_SCORE_BYTE);
  if (SpecialValue % 1000 || SpecialValue > 100000) SpecialValue = 40000;

  DimLevel = ReadSetting(EEPROM_DIM_LEVEL_BYTE, 2);
  if (DimLevel < 2 || DimLevel > 3) DimLevel = 2;
  BSOS_SetDimDivisor(1, DimLevel);

  AwardScores[0] = BSOS_ReadULFromEEProm(BSOS_AWARD_SCORE_1_EEPROM_START_BYTE);
  AwardScores[1] = BSOS_ReadULFromEEProm(BSOS_AWARD_SCORE_2_EEPROM_START_BYTE);
  AwardScores[2] = BSOS_ReadULFromEEProm(BSOS_AWARD_SCORE_3_EEPROM_START_BYTE);

}
// ----------------------------------------------------------------
void AddToBonus(byte bonusAddition) {
  Bonus += bonusAddition;
  if (Bonus > MAX_DISPLAY_BONUS) Bonus = MAX_DISPLAY_BONUS;
}


//===============================================================================
////////////////////////////////////////////////////////////////////////////
//
//  Lamp Management functions
//
////////////////////////////////////////////////////////////////////////////
/*
  No player lamps on Paragon
void SetPlayerLamps(byte numPlayers, byte playerOffset = 0, int flashPeriod = 0) {
  for (int count = 0; count < 4; count++) {
    BSOS_SetLampState(PLAYER_1 + playerOffset + count, (numPlayers == (count + 1)) ? 1 : 0, 0, flashPeriod);
  }
} // END: SetPlayerLamps()
*/
// Game specific

// ----------------------------------------------------------------

void ShowGoldenSaucerLamps() {
  if ((GoldenSaucerValue==1) || (GoldenSaucerValue==6)) 
    { BSOS_SetLampState(L_2K_GOLDEN, 1); } else { BSOS_SetLampState(L_2K_GOLDEN, 0); }
  if ((GoldenSaucerValue==2) || (GoldenSaucerValue==7)) 
    { BSOS_SetLampState(L_4K_GOLDEN, 1); } else { BSOS_SetLampState(L_4K_GOLDEN, 0); }
  if ((GoldenSaucerValue==3) || (GoldenSaucerValue==8)) 
    { BSOS_SetLampState(L_6K_GOLDEN, 1); } else { BSOS_SetLampState(L_6K_GOLDEN, 0); }
  if ((GoldenSaucerValue==4) || (GoldenSaucerValue==9)) 
    { BSOS_SetLampState(L_8K_GOLDEN, 1); } else { BSOS_SetLampState(L_8K_GOLDEN, 0); }
  if ((HuntMode) && (HuntLocation==4)) { // skip if hunt target
  
  } else {
    if ((GoldenSaucerValue>4) && (GoldenSaucerValue<10))
      { BSOS_SetLampState(L_10K_GOLDEN, 1); } else { BSOS_SetLampState(L_10K_GOLDEN, 0); }
    if (GoldenSaucerValue==10)
      { BSOS_SetLampState(L_20K_GOLDEN, 1); } else 
      { if (GameMode!=GAME_MODE_SKILL_SHOT) BSOS_SetLampState(L_20K_GOLDEN, 0); }
  }
}

// ----------------------------------------------------------------

void ShowShootAgainLamp() {
  // turn same player lamps on/off based on ball save time
  // Note: ball save starts once a target has been hit
  if (!BallSaveUsed && BallSaveNumSeconds>0 && (CurrentTime-BallFirstSwitchHitTime)<((unsigned long)(BallSaveNumSeconds-1)*1000)) {
    unsigned long msRemaining = ((unsigned long)(BallSaveNumSeconds-1)*1000)-(CurrentTime-BallFirstSwitchHitTime);
    BSOS_SetLampState(L_SHOOT_AGAIN, 1, 0, (msRemaining<1000)?100:500);
    BSOS_SetLampState(L_BB_SHOOT_AGAIN, 1, 0, (msRemaining<1000)?100:500);    
  } else { // turn off
    BSOS_SetLampState(L_SHOOT_AGAIN, SamePlayerShootsAgain);
    BSOS_SetLampState(L_BB_SHOOT_AGAIN, SamePlayerShootsAgain);    // turn off?
  }
}

// ----------------------------------------------------------------
void ShowAwardLamps() {
  // show some playfield feature lamps 
  
  // Right Drops: 10,15,20,25,30=sp
  if ((HuntMode) && (HuntLocation==1)) { // don't overrite hunt lamps
    
  } else {
    if (DropsRightDownScore[CurrentPlayer]==10000) { BSOS_SetLampState(L_10K_DROPS, 1); } else { BSOS_SetLampState(L_10K_DROPS, 0); }
    if (DropsRightDownScore[CurrentPlayer]==15000) { BSOS_SetLampState(L_15K_DROPS, 1); } else { BSOS_SetLampState(L_15K_DROPS, 0); }
  }
  if ((HuntMode) && (HuntLocation==3)) { // don't overrite hunt lamps
    
  } else {  
    if (DropsRightDownScore[CurrentPlayer]==20000) { BSOS_SetLampState(L_20K_DROPS, 1); } else { BSOS_SetLampState(L_20K_DROPS, 0); }
    if (DropsRightDownScore[CurrentPlayer]==25000) { BSOS_SetLampState(L_25K_DROPS, 1); } else { BSOS_SetLampState(L_25K_DROPS, 0); }
  }
  if (DropsRightDownScore[CurrentPlayer]==30000) { BSOS_SetLampState(L_SPECIAL_DROPS, 1); } else { BSOS_SetLampState(L_SPECIAL_DROPS, 0); }

  // Waterfall  0=1k 1=5k, 2=10k 3=special
  if (WaterfallValue==1) { BSOS_SetLampState(L_5K_WATER, 1,0,300); } else { BSOS_SetLampState(L_5K_WATER, 0); }
  if (WaterfallValue==2) { BSOS_SetLampState(L_10K_WATER, 1,0,250); } else { BSOS_SetLampState(L_10K_WATER, 0); }
  if (WaterfallValue==3) { BSOS_SetLampState(L_WATER_SPECIAL, 1,0,200); } else { BSOS_SetLampState(L_WATER_SPECIAL, 0); }
  
  // bonusX
  if (BonusX==2) { BSOS_SetLampState(L_2X_BONUS, 1); } else { BSOS_SetLampState(L_2X_BONUS, 0); }
  if (BonusX==3) { BSOS_SetLampState(L_3X_BONUS, 1); BSOS_SetLampState(L_TREASURE_5X, 1); } else { BSOS_SetLampState(L_3X_BONUS, 0); BSOS_SetLampState(L_TREASURE_5X, 0); }
  if (BonusX==5) { BSOS_SetLampState(L_5X_BONUS, 1); } else { BSOS_SetLampState(L_5X_BONUS, 0); }

  // treasure
  if ((HuntMode) && (HuntLocation==0)) { // don't overrite hunt lamps
    
  } else {    
    if (TreasureValue==2) { BSOS_SetLampState(L_TREASURE_EB, 1); } else { BSOS_SetLampState(L_TREASURE_EB, 0); }
    if (TreasureValue==3) { BSOS_SetLampState(L_TREASURE_SPECIAL, 1); } else { BSOS_SetLampState(L_TREASURE_SPECIAL, 0); }
  }
  // Spinner 
  if ((HuntMode) && (HuntLocation==2)) { // don't overrite hunt lamps
    
  } else {    
    for (byte x=2; x<6; x++) {
      if (SpinnerValue>=x) { BSOS_SetLampState(L_SPINNER_1+(x-2), 1); } else { BSOS_SetLampState(L_SPINNER_1+(x-2), 0); }
    }
  }
  
} // end: ShowAwardLamps()

// ----------------------------------------------------------------
void ShowParagonLamps() {
  // show/rotate upper right p-a-r-a-g-o-n lamps
  byte x=ParagonLit[CurrentPlayer];
  byte y;
  int z;
  
  // Upper paragon letters
  if ((HuntMode) && (HuntLocation==6)) { // don't overrite hunt lamps
    
  } else {    
    if (x&128) { // paragon lit
      for (y=0; y<7; y++) { BSOS_SetLampState(L_SAUCER_P+y, 1,0,400); }
      BSOS_SetLampState(L_SAUCER_SPECIAL, 1,0,400);
      
    } else { // light p-a-r-a-g-o-n letters
      for (y=0; y<7; y++) { 
        if (ParagonValue==y) { BSOS_SetLampState(L_SAUCER_P+y, 1); } else { BSOS_SetLampState(L_SAUCER_P+y, 0); }      
      }
      if (GameMode!=GAME_MODE_SKILL_SHOT)
        BSOS_SetLampState(L_SAUCER_SPECIAL, 0);
    }
  }

  
  // light center paragon letters 
  // this works, bit 1 turns on lamp0, bit 2 lamp1, etc
  if ((HuntMode) && (HuntLocation==5)) { // don't overrite hunt lamps
    
  } else {    
    if (MachineState==MACHINE_STATE_NORMAL_GAMEPLAY) {
      for (y=0; y<7; y++) {
        BSOS_SetLampState(L_CENTER_P+y, x & (1<<y));
      }  
    }
  }
  
  // letter timing
  if ((MoveParagon) && ((CurrentTime-LastParagonLetterTime)>PARAGON_TIMING)) {
    LastParagonLetterTime=CurrentTime;
    ParagonValue++;
    if (ParagonValue>6) ParagonValue=0;
  }
}
// ----------------------------------------------------------------
void SwitchOffBonus(byte x) {
  // switch off 10k,20,30,40 bonus lights
  for (byte y=0;y<x; y++) {
    BSOS_SetLampState(L_10K_BONUS+y, 0);
  }
}

void ShowBonusOnTree(byte bonus, byte dim=0) {
  if (bonus>MAX_DISPLAY_BONUS) bonus = MAX_DISPLAY_BONUS;
  
  byte cap = 10;  // number of lights in count-down tree

  // Need to clear any upper bonus lights before counting down
  if (bonus<40) BSOS_SetLampState(L_40K_BONUS, 0);
  if (bonus<30) BSOS_SetLampState(L_30K_BONUS, 0);
  if (bonus<20) BSOS_SetLampState(L_20K_BONUS, 0);
  
  if (bonus>=cap) {  // extended bonus lights
    while(bonus>=cap) {
      if (bonus>=40) { 
        bonus-=40; 
        BSOS_SetLampState(L_40K_BONUS, 1); 
        if (bonus<cap)  SwitchOffBonus(3);
      }
      if (bonus>=30) { 
        bonus-=30; 
        BSOS_SetLampState(L_30K_BONUS, 1); 
        if (bonus<cap)  SwitchOffBonus(2);        
      }
      if (bonus>=20) { 
        bonus-=20; 
        BSOS_SetLampState(L_20K_BONUS, 1);
        if (bonus<cap)  SwitchOffBonus(1);        
      }
      if (bonus>cap) { bonus-=cap; BSOS_SetLampState(L_10K_BONUS, 1, dim, 250); } 
      else if (bonus==cap) { bonus-=cap; BSOS_SetLampState(L_10K_BONUS, 1);
      }
      
    } // while
  } else {
    SwitchOffBonus(4); // turn off 10k-40k
  }

  for (byte x=1; x<cap; x++) {
    if (bonus==x) { BSOS_SetLampState(L_1K_BONUS + (x-1), 1); }
    else { BSOS_SetLampState(L_1K_BONUS + (x-1), 0); }
  }

} // END: ShowBonusOnTree()

byte LastBonusShown = 0;
// routine below not used but will be if we want to occasionally re-purpose the bonus lamps for something else
void ShowBonusLamps() {
  
  // special display during skill shot
  if (GameMode==GAME_MODE_SKILL_SHOT) {
    if (EnableSkillFx) {
      for (byte x=0; x<10; x++) BSOS_SetLampState(L_1K_BONUS+x,1,0,200+random(300));
      EnableSkillFx=false; // run one time only until reset
    }
    return;
  }
  
/*  - no game modes yet  
  if (GameMode==GAME_MODE_MINI_GAME_QUALIFIED) {
    byte lightPhase = ((CurrentTime-GameModeStartTime)/100)%15;
    for (byte count=0; count<10; count++) {
      BSOS_SetLampState(L_1K_BONUS+count, (lightPhase==count)||((lightPhase-1)==count), ((lightPhase-1)==count));
    }
  } else 
    */
    if (Bonus!=LastBonusShown) {
    LastBonusShown = Bonus;
    ShowBonusOnTree(Bonus);
  }
}
// ----------------------------------------------------------------
void ResetLights() {   
// set lights back to normal gameplay
  ShowBonusOnTree(Bonus);  // overrides ShowBonusLamps();

}
// ----------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////
//
//  Display Management functions
//
////////////////////////////////////////////////////////////////////////////
unsigned long LastTimeScoreChanged = 0;
unsigned long LastTimeOverrideAnimated = 0;
unsigned long LastFlashOrDash = 0;
unsigned long ScoreOverrideValue[4]= {0, 0, 0, 0};
byte ScoreOverrideStatus = 0;
byte LastScrollPhase = 0;

byte MagnitudeOfScore(unsigned long score) {
  if (score == 0) return 0;

  byte retval = 0;
  while (score > 0) {
    score = score / 10;
    retval += 1;
  }
  return retval;
}

void OverrideScoreDisplay(byte displayNum, unsigned long value, boolean animate) {
  if (displayNum>3) return;
  ScoreOverrideStatus |= (0x10<<displayNum);
  if (animate) ScoreOverrideStatus |= (0x01<<displayNum);
  else ScoreOverrideStatus &= ~(0x01<<displayNum);
  ScoreOverrideValue[displayNum] = value;
}

byte GetDisplayMask(byte numDigits) {
  byte displayMask = 0;
  for (byte digitCount=0; digitCount<numDigits; digitCount++) {
    displayMask |= (0x20>>digitCount);
  }  
  return displayMask;
}

//-----------------------------------------------------------------

void ShowPlayerScores(byte displayToUpdate, boolean flashCurrent, boolean dashCurrent, unsigned long allScoresShowValue=0) {

  if (displayToUpdate==0xFF) ScoreOverrideStatus = 0;

  byte displayMask = 0x3F;
  unsigned long displayScore = 0;
  unsigned long overrideAnimationSeed = CurrentTime/250;
  byte scrollPhaseChanged = false;

  byte scrollPhase = ((CurrentTime-LastTimeScoreChanged)/250)%16;
  if (scrollPhase!=LastScrollPhase) {
    LastScrollPhase = scrollPhase;
    scrollPhaseChanged = true;
  }

  boolean updateLastTimeAnimated = false;

  for (byte scoreCount=0; scoreCount<4; scoreCount++) {
    // If this display is currently being overriden, then we should update it
    if (allScoresShowValue==0 && (ScoreOverrideStatus & (0x10<<scoreCount))) {
      displayScore = ScoreOverrideValue[scoreCount];
      byte numDigits = MagnitudeOfScore(displayScore);
      if (numDigits==0) numDigits = 1;
      if (numDigits<(BALLY_STERN_OS_NUM_DIGITS-1) && (ScoreOverrideStatus & (0x01<<scoreCount))) {
        if (overrideAnimationSeed!=LastTimeOverrideAnimated) {
          updateLastTimeAnimated = true;
          byte shiftDigits = (overrideAnimationSeed)%(((BALLY_STERN_OS_NUM_DIGITS+1)-numDigits)+((BALLY_STERN_OS_NUM_DIGITS-1)-numDigits));
          if (shiftDigits>=((BALLY_STERN_OS_NUM_DIGITS+1)-numDigits)) shiftDigits = (BALLY_STERN_OS_NUM_DIGITS-numDigits)*2-shiftDigits;
          byte digitCount;
          displayMask = GetDisplayMask(numDigits);
          for (digitCount=0; digitCount<shiftDigits; digitCount++) {
            displayScore *= 10;
            displayMask = displayMask>>1;
          }
          BSOS_SetDisplayBlank(scoreCount, 0x00);
          BSOS_SetDisplay(scoreCount, displayScore, false);
          BSOS_SetDisplayBlank(scoreCount, displayMask);
        }
      } else {
        BSOS_SetDisplay(scoreCount, displayScore, true);
      }
      
    } else {
      // No override, update scores designated by displayToUpdate
      if (allScoresShowValue==0) displayScore = (scoreCount==CurrentPlayer)?CurrentPlayerCurrentScore:CurrentScores[scoreCount];
      else displayScore = allScoresShowValue;

      // If we're updating all displays, or the one currently matching the loop, or if we have to scroll
      if (displayToUpdate==0xFF || displayToUpdate==scoreCount || displayScore>BALLY_STERN_OS_MAX_DISPLAY_SCORE) {

        // Don't show this score if it's not a current player score (even if it's scrollable)
        if (displayToUpdate==0xFF && (scoreCount>=CurrentNumPlayers&&CurrentNumPlayers!=0) && allScoresShowValue==0) {
          BSOS_SetDisplayBlank(scoreCount, 0x00);
          continue;
        }

        if (displayScore>BALLY_STERN_OS_MAX_DISPLAY_SCORE) {
          // Score needs to be scrolled
          if ((CurrentTime-LastTimeScoreChanged)<4000) {
            BSOS_SetDisplay(scoreCount, displayScore%(BALLY_STERN_OS_MAX_DISPLAY_SCORE+1), false);  
            BSOS_SetDisplayBlank(scoreCount, BALLY_STERN_OS_ALL_DIGITS_MASK);
          } else {

            // Scores are scrolled 10 digits and then we wait for 6
            if (scrollPhase<11 && scrollPhaseChanged) {
              byte numDigits = MagnitudeOfScore(displayScore);
              
              // Figure out top part of score
              unsigned long tempScore = displayScore;
              if (scrollPhase<BALLY_STERN_OS_NUM_DIGITS) {
                displayMask = BALLY_STERN_OS_ALL_DIGITS_MASK;
                for (byte scrollCount=0; scrollCount<scrollPhase; scrollCount++) {
                  displayScore = (displayScore % (BALLY_STERN_OS_MAX_DISPLAY_SCORE+1)) * 10;
                  displayMask = displayMask >> 1;
                }
              } else {
                displayScore = 0; 
                displayMask = 0x00;
              }

              // Add in lower part of score
              if ((numDigits+scrollPhase)>10) {
                byte numDigitsNeeded = (numDigits+scrollPhase)-10;
                for (byte scrollCount=0; scrollCount<(numDigits-numDigitsNeeded); scrollCount++) {
                  tempScore /= 10;
                }
                displayMask |= GetDisplayMask(MagnitudeOfScore(tempScore));
                displayScore += tempScore;
              }
              BSOS_SetDisplayBlank(scoreCount, displayMask);
              BSOS_SetDisplay(scoreCount, displayScore);
            }
          }          
        } else {
          if (flashCurrent) {
            unsigned long flashSeed = CurrentTime/250;
            if (flashSeed != LastFlashOrDash) {
              LastFlashOrDash = flashSeed;
              if (((CurrentTime/250)%2)==0) BSOS_SetDisplayBlank(scoreCount, 0x00);
              else BSOS_SetDisplay(scoreCount, displayScore, true, 2);
            }
          } else if (dashCurrent) {
            unsigned long dashSeed = CurrentTime/50;
            if (dashSeed != LastFlashOrDash) {
              LastFlashOrDash = dashSeed;
              byte dashPhase = (CurrentTime/60)%36;
              byte numDigits = MagnitudeOfScore(displayScore);
              if (dashPhase<12) { 
                displayMask = GetDisplayMask((numDigits==0)?2:numDigits);
                if (dashPhase<7) {
                  for (byte maskCount=0; maskCount<dashPhase; maskCount++) {
                    displayMask &= ~(0x01<<maskCount);
                  }
                } else {
                  for (byte maskCount=12; maskCount>dashPhase; maskCount--) {
                    displayMask &= ~(0x20>>(maskCount-dashPhase-1));
                  }
                }
                BSOS_SetDisplay(scoreCount, displayScore);
                BSOS_SetDisplayBlank(scoreCount, displayMask);
              } else {
                BSOS_SetDisplay(scoreCount, displayScore, true, 2);
              }
            }
          } else {
            BSOS_SetDisplay(scoreCount, displayScore, true, 2);          
          }
        }
      } // End if this display should be updated
    } // End on non-overridden
  } // End loop on scores

  if (updateLastTimeAnimated) {
    LastTimeOverrideAnimated = overrideAnimationSeed;
  }

} // end: ShowPlayerScores()

////////////////////////////////////////////////////////////////////////////
//
//  Machine State Helper functions
//
////////////////////////////////////////////////////////////////////////////



/*  might possibly want to add later
void AddCoinToAudit(byte switchHit) {

  unsigned short coinAuditStartByte = 0;

  switch (switchHit) {
    case SW_COIN_3: coinAuditStartByte = BSOS_CHUTE_3_COINS_START_BYTE; break;
    case SW_COIN_2: coinAuditStartByte = BSOS_CHUTE_2_COINS_START_BYTE; break;
    case SW_COIN_1: coinAuditStartByte = BSOS_CHUTE_1_COINS_START_BYTE; break;
  }

  if (coinAuditStartByte) {
    BSOS_WriteULToEEProm(coinAuditStartByte, BSOS_ReadULFromEEProm(coinAuditStartByte) + 1);
  }

}
*/


// Trident version
void AddCredit(boolean playSound = false, byte numToAdd = 1) {
  // Adds regular credit to machine + audit, no knocker
  if (Credits < MaximumCredits) {
    Credits += numToAdd;
    if (Credits > MaximumCredits) Credits = MaximumCredits;
    BSOS_WriteByteToEEProm(BSOS_CREDITS_EEPROM_BYTE, Credits);
    if (playSound) PlaySoundEffect(SFX_ADD_CREDIT);
    BSOS_SetDisplayCredits(Credits);
    BSOS_SetCoinLockout(false);
  } else {
    BSOS_SetDisplayCredits(Credits);
    BSOS_SetCoinLockout(true);
  }
} // end: AddCredit()
//-----------------------------------------------------------------
void AwardExtraBall() {

  // add additional checks and sound  
  SamePlayerShootsAgain=true;

}
//-----------------------------------------------------------------
void AwardSpecial() {

  if (TournamentScoring) {
    CurrentPlayerCurrentScore += SpecialValue;
  } else {
    AddSpecialCredit();
  }

  BSOS_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime, true);

}
//-----------------------------------------------------------------

void AddSpecialCredit() {
  // Adds special credit + audit, knocker
  // NOTE: update this to add x special parm?
  AddCredit(false, 1);

  BSOS_WriteULToEEProm(BSOS_TOTAL_REPLAYS_EEPROM_START_BYTE, BSOS_ReadULFromEEProm(BSOS_TOTAL_REPLAYS_EEPROM_START_BYTE) + 1);  
} // end: AddSpecialCredit()
//-----------------------------------------------------------------
boolean AddPlayer(boolean resetNumPlayers=false) {

  if (Credits<1 && !FreePlayMode) return false;
  if (resetNumPlayers) CurrentNumPlayers = 0;  
  if (CurrentNumPlayers>=4) return false;

  CurrentNumPlayers += 1;
  BSOS_SetDisplay(CurrentNumPlayers-1, 0);
  BSOS_SetDisplayBlank(CurrentNumPlayers-1, 0x30);

  if (!FreePlayMode) {
    Credits -= 1;
    BSOS_WriteByteToEEProm(BSOS_CREDITS_EEPROM_BYTE, Credits);
//    BSOS_SetCoinLockout(false); // needed?    
  }

  BSOS_SetDisplayCredits(Credits);

//    PlaySoundEffect(SOUND_EFFECT_ADD_PLAYER_1 + (CurrentNumPlayers - 1));
  
  return true;
} // end: AddPlayer()
//-----------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////
//
//  Diagnostics
//
////////////////////////////////////////////////////////////////////////////


/* original base version

int RunSelfTest(int curState, boolean curStateChanged) {
  int returnState = curState;
  CurrentNumPlayers = 0;

  // Any state that's greater than CHUTE_3 is handled by the Base Self-test code
  // Any that's less, is machine specific, so we handle it here.
  if (curState>=MACHINE_STATE_TEST_CHUTE_3_COINS) {
    returnState = RunBaseSelfTest(returnState, curStateChanged, CurrentTime, SW_CREDIT_RESET);  
  } else {
    returnState = MACHINE_STATE_ATTRACT;
  }

  return returnState;
}
*/
//-----------------------------------------------------------------
/*

Test/Audit/Parameters

00:01 - Lamps
XX:XX - Displays
00:03 - Solenoids
00:04 - Switches
00:05 - Sound
CHECK ALL VALUES YOUR FIRST RUN - THERE ARE NO DEFAULTS!
01 - Award Score Level 1
02 - Award Score Level 2
03 - Award Score Level 3
04 - High Score to Date
05 - Current Credits
06 - Total plays (Audit)
07 - Total replays (Audit)
08 - Total times high score beaten (Audit)
09 - Chute #2 coins (Audit)
10 - Chute #1 coins (Audit)
11 - Chute #3 coins (Audit)
Activating the Slam Switch at any time will reboot into Attract Mode.
12 - Free play off/on (0, 1)
13 - Ball Save Num Seconds (0, 6, 11, 16, 21)
14 - Music Level (0, 1, 2, 3, [4, 5]) [if WAV Trigger is enabled in the build]
15 - Tournament Scoring (0-no, 1-yes)
16 - Tilt Warning (0, 1, 2)
17 - Award Score Override (0 - 7)
18 - Balls per game Override (3, 5)
19 - Scrolling Scores (0-no, 1-yes)
20 - Extra Ball Award (0 - 100,000) [only used for Tournament Scoring]
21 - Special Award (0 - 100,000) [only used for Tournament Scoring]
22 - Dim Level (2=50%, 3=33%)  (50% works ok for Comet 2SMD LEDs)

*/


#define ADJ_TYPE_LIST                 1
#define ADJ_TYPE_MIN_MAX              2
#define ADJ_TYPE_MIN_MAX_DEFAULT      3
#define ADJ_TYPE_SCORE                4
#define ADJ_TYPE_SCORE_WITH_DEFAULT   5
#define ADJ_TYPE_SCORE_NO_DEFAULT     6
byte AdjustmentType = 0;
byte NumAdjustmentValues = 0;
byte AdjustmentValues[8];
unsigned long AdjustmentScore;
byte *CurrentAdjustmentByte = NULL;
unsigned long *CurrentAdjustmentUL = NULL;
byte CurrentAdjustmentStorageByte = 0;
byte TempValue = 0;


int RunSelfTest(int curState, boolean curStateChanged) {
  int returnState = curState;
  CurrentNumPlayers = 0;

#if defined(USE_WAV_TRIGGER) || defined(USE_WAV_TRIGGER_1p3)
  if (curStateChanged) {
    // Send a stop-all command and reset the sample-rate offset, in case we have
    //  reset while the WAV Trigger was already playing.
    wTrig.stopAllTracks();
    wTrig.samplerateOffset(0);
  }
#endif

  // Any state that's greater than CHUTE_3 is handled by the Base Self-test code
  // Any that's less, is machine specific, so we handle it here.
  if (curState >= MACHINE_STATE_TEST_CHUTE_3_COINS) {
    returnState = RunBaseSelfTest(returnState, curStateChanged, CurrentTime, SW_CREDIT_RESET, SW_SLAM);
  } else {
    byte curSwitch = BSOS_PullFirstFromSwitchStack();

    if (curSwitch == SW_SELF_TEST_SWITCH && (CurrentTime - GetLastSelfTestChangedTime()) > 250) {
      SetLastSelfTestChangedTime(CurrentTime);
      returnState -= 1;
    }

    if (curSwitch == SW_SLAM) {
      returnState = MACHINE_STATE_ATTRACT;
    }

    if (curStateChanged) {
      for (int count = 0; count < 4; count++) {
        BSOS_SetDisplay(count, 0);
        BSOS_SetDisplayBlank(count, 0x00);
      }
      BSOS_SetDisplayCredits(MACHINE_STATE_TEST_SOUNDS - curState);
      BSOS_SetDisplayBallInPlay(0, false);
      CurrentAdjustmentByte = NULL;
      CurrentAdjustmentUL = NULL;
      CurrentAdjustmentStorageByte = 0;

      AdjustmentType = ADJ_TYPE_MIN_MAX;
      AdjustmentValues[0] = 0;
      AdjustmentValues[1] = 1;
      TempValue = 0;

      switch (curState) {
        case MACHINE_STATE_ADJUST_FREEPLAY:
          CurrentAdjustmentByte = (byte *)&FreePlayMode;
          CurrentAdjustmentStorageByte = EEPROM_FREE_PLAY_BYTE;
          break;
        case MACHINE_STATE_ADJUST_BALL_SAVE:                        // 18
          AdjustmentType = ADJ_TYPE_LIST;
          NumAdjustmentValues = 5;
          AdjustmentValues[1] = 5;
          AdjustmentValues[2] = 10;
          AdjustmentValues[3] = 15;
          AdjustmentValues[4] = 20;
          CurrentAdjustmentByte = &BallSaveNumSeconds;
          CurrentAdjustmentStorageByte = EEPROM_BALL_SAVE_BYTE;
          break;
        case MACHINE_STATE_ADJUST_MUSIC_LEVEL:
          AdjustmentType = ADJ_TYPE_MIN_MAX_DEFAULT;
          AdjustmentValues[1] = 3;
          CurrentAdjustmentByte = &MusicLevel;
          CurrentAdjustmentStorageByte = EEPROM_MUSIC_LEVEL_BYTE;
          break;
        case MACHINE_STATE_ADJUST_TOURNAMENT_SCORING:
          CurrentAdjustmentByte = (byte *)&TournamentScoring;
          CurrentAdjustmentStorageByte = EEPROM_TOURNAMENT_SCORING_BYTE;
          break;
        case MACHINE_STATE_ADJUST_TILT_WARNING:
          AdjustmentValues[1] = 2;
          CurrentAdjustmentByte = &MaxTiltWarnings;
          CurrentAdjustmentStorageByte = EEPROM_TILT_WARNING_BYTE;
          break;
        case MACHINE_STATE_ADJUST_AWARD_OVERRIDE:
          AdjustmentType = ADJ_TYPE_MIN_MAX_DEFAULT;
          AdjustmentValues[1] = 7;
          CurrentAdjustmentByte = &ScoreAwardReplay;
          CurrentAdjustmentStorageByte = EEPROM_AWARD_OVERRIDE_BYTE;
          break;
        case MACHINE_STATE_ADJUST_BALLS_OVERRIDE:
          AdjustmentType = ADJ_TYPE_LIST;
          NumAdjustmentValues = 3;
          AdjustmentValues[0] = 3;
          AdjustmentValues[1] = 5;
          AdjustmentValues[2] = 99;
          CurrentAdjustmentByte = &BallsPerGame;
          CurrentAdjustmentStorageByte = EEPROM_BALLS_OVERRIDE_BYTE;
          break;
        case MACHINE_STATE_ADJUST_SCROLLING_SCORES:
          CurrentAdjustmentByte = (byte *)&ScrollingScores;
          CurrentAdjustmentStorageByte = EEPROM_SCROLLING_SCORES_BYTE;
          break;

        case MACHINE_STATE_ADJUST_EXTRA_BALL_AWARD:
          AdjustmentType = ADJ_TYPE_SCORE_WITH_DEFAULT;
          CurrentAdjustmentUL = &ExtraBallValue;
          CurrentAdjustmentStorageByte = EEPROM_EXTRA_BALL_SCORE_BYTE;
          break;

        case MACHINE_STATE_ADJUST_SPECIAL_AWARD:
          AdjustmentType = ADJ_TYPE_SCORE_WITH_DEFAULT;
          CurrentAdjustmentUL = &SpecialValue;
          CurrentAdjustmentStorageByte = EEPROM_SPECIAL_SCORE_BYTE;
          break;

        case MACHINE_STATE_ADJUST_DIM_LEVEL:
          AdjustmentType = ADJ_TYPE_LIST;
          NumAdjustmentValues = 2;
          AdjustmentValues[0] = 2;
          AdjustmentValues[1] = 3;
          CurrentAdjustmentByte = &DimLevel;
          CurrentAdjustmentStorageByte = EEPROM_DIM_LEVEL_BYTE;
          for (int count = 0; count < 10; count++) BSOS_SetLampState(L_1K_BONUS + count, 1, 1);
          break;

        case MACHINE_STATE_ADJUST_DONE:
          returnState = MACHINE_STATE_ATTRACT;
          break;
      }

    }

    // Change value, if the switch is hit
    if (curSwitch == SW_CREDIT_RESET) {

      if (CurrentAdjustmentByte && (AdjustmentType == ADJ_TYPE_MIN_MAX || AdjustmentType == ADJ_TYPE_MIN_MAX_DEFAULT)) {
        byte curVal = *CurrentAdjustmentByte;
        curVal += 1;
        if (curVal > AdjustmentValues[1]) {
          if (AdjustmentType == ADJ_TYPE_MIN_MAX) curVal = AdjustmentValues[0];
          else {
            if (curVal > 99) curVal = AdjustmentValues[0];
            else curVal = 99;
          }
        }
        *CurrentAdjustmentByte = curVal;
        if (CurrentAdjustmentStorageByte) EEPROM.write(CurrentAdjustmentStorageByte, curVal);
      } else if (CurrentAdjustmentByte && AdjustmentType == ADJ_TYPE_LIST) {
        byte valCount = 0;
        byte curVal = *CurrentAdjustmentByte;
        byte newIndex = 0;
        for (valCount = 0; valCount < (NumAdjustmentValues - 1); valCount++) {
          if (curVal == AdjustmentValues[valCount]) newIndex = valCount + 1;
        }
        *CurrentAdjustmentByte = AdjustmentValues[newIndex];
        if (CurrentAdjustmentStorageByte) EEPROM.write(CurrentAdjustmentStorageByte, AdjustmentValues[newIndex]);
      } else if (CurrentAdjustmentUL && (AdjustmentType == ADJ_TYPE_SCORE_WITH_DEFAULT || AdjustmentType == ADJ_TYPE_SCORE_NO_DEFAULT)) {
        unsigned long curVal = *CurrentAdjustmentUL;
        curVal += 5000;
        if (curVal > 100000) curVal = 0;
        if (AdjustmentType == ADJ_TYPE_SCORE_NO_DEFAULT && curVal == 0) curVal = 5000;
        *CurrentAdjustmentUL = curVal;
        if (CurrentAdjustmentStorageByte) BSOS_WriteULToEEProm(CurrentAdjustmentStorageByte, curVal);
      }

      if (curState == MACHINE_STATE_ADJUST_DIM_LEVEL) {
        BSOS_SetDimDivisor(1, DimLevel);
      }
    }

    // Show current value
    if (CurrentAdjustmentByte != NULL) {
      BSOS_SetDisplay(0, (unsigned long)(*CurrentAdjustmentByte), true);
    } else if (CurrentAdjustmentUL != NULL) {
      BSOS_SetDisplay(0, (*CurrentAdjustmentUL), true);
    }

  }

  if (curState == MACHINE_STATE_ADJUST_DIM_LEVEL) {
    for (int count = 0; count < 10; count++) BSOS_SetLampState(L_1K_BONUS + count, 1, (CurrentTime / 1000) % 2);
  }

  if (returnState == MACHINE_STATE_ATTRACT) {
    // If any variables have been set to non-override (99), return
    // them to dip switch settings
    // Balls Per Game, Player Loses On Ties, Novelty Scoring, Award Score
//    DecodeDIPSwitchParameters();
    ReadStoredParameters();
  }

  return returnState;
} // end: RunSelfTest()
//-----------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////
//
//  Audio Output functions
//
////////////////////////////////////////////////////////////////////////////
#if defined(USE_WAV_TRIGGER) || defined(USE_WAV_TRIGGER_1p3)
byte CurrentBackgroundSong = SOUND_EFFECT_NONE;
#endif


void PlayBackgroundSong(byte songNum) {

#if defined(USE_WAV_TRIGGER) || defined(USE_WAV_TRIGGER_1p3)
  if (MusicLevel > 1) {
    if (CurrentBackgroundSong != songNum) {
      if (CurrentBackgroundSong != SOUND_EFFECT_NONE) wTrig.trackStop(CurrentBackgroundSong);
      if (songNum != SOUND_EFFECT_NONE) {
#ifdef USE_WAV_TRIGGER_1p3
        wTrig.trackPlayPoly(songNum, true);
#else
        wTrig.trackPlayPoly(songNum);
#endif
        wTrig.trackLoop(songNum, true);
        wTrig.trackGain(songNum, -4);
      }
      CurrentBackgroundSong = songNum;
    }
  }
#else
  byte test = songNum;
  songNum = test;
#endif

}

//-----------------------------------------------------------------

unsigned long NextSoundEffectTime = 0;

void PlaySoundEffect(byte soundEffectNum) {

  if (MusicLevel == 0) return;

#if defined(USE_WAV_TRIGGER) || defined(USE_WAV_TRIGGER_1p3)

#ifndef USE_WAV_TRIGGER_1p3
  if (  soundEffectNum == SFX_SPINNER ) wTrig.trackStop(soundEffectNum);
#endif
  wTrig.trackPlayPoly(soundEffectNum);
#endif

}
//-----------------------------------------------------------------








////////////////////////////////////////////////////////////////////////////
//
//  Attract Mode
//
////////////////////////////////////////////////////////////////////////////

unsigned long BlinkTimer=0; // keep track of how long blinking
unsigned int BlinkPhase=500;  // blink rate

void ShowParagonSweep() {
  // do not run at th e same time as ShowParagonBlink
  if ((CurrentTime-BlinkTimer)>200) {  
    BlinkTimer=CurrentTime;
    byte lampPhase = (byte) ((CurrentTime/200)%7);  // break 200ms into 7 different phases 0-6, going lower than 200 doesn't work
    for (byte x=0;x<7;x++) {
      BSOS_SetLampState(L_CENTER_P+x, lampPhase==x);
    }
  }
}
//-----------------------------------------------------------------
void ShowParagonBlink() {
  // do not run at the same time as ShowParagonSweep
  if ((CurrentTime-BlinkTimer)>3000) {
    BlinkTimer=CurrentTime;
    BlinkPhase=100*random(5)+100; // random blink from 100ms to 500ms
  }
  for (byte x=0;x<7;x++) {
    BSOS_SetLampState(L_CENTER_P+x, 1,0,BlinkPhase); // 3rd param is dim 0=none, 1=50% 2=33%
  }
}
//-----------------------------------------------------------------
void AlternatePlayfieldLights() {
  static byte flip=1;
  if ((CurrentTime-BlinkTimer)>500) {
    BlinkTimer=CurrentTime;
    flip=1-flip; // alternate between 0 and 1
    for (byte x=0;x<40;x++) {  // doesn't cover all lights because don't want to engage bb lights
      BSOS_SetLampState(x,(x+flip)%2,0); // 3rd param is dim 0=none, 1=50% 2=33%
    }     
  }
}
//-----------------------------------------------------------------

#if defined(ENABLE_ATTRACT)
unsigned long AttractLastLadderTime = 0;
byte AttractLastLadderBonus = 0;
unsigned long AttractLastStarTime = 0;
byte InAttractMode = false;
#endif

byte AttractLastHeadMode = 255;
byte AttractLastPlayfieldMode = 255;


int RunAttractMode(int curState, boolean curStateChanged) {

  int returnState = curState;

  // If this is the first time in the attract mode loop
  if (curStateChanged) {
    BSOS_DisableSolenoidStack();
    BSOS_TurnOffAllLamps();
    BSOS_SetDisableFlippers(true);
    // turn off any music?
    if (DEBUG_MESSAGES) {
      Serial.write("Entering Attract Mode\n\r");
    }
// recently uncommented
    AttractLastHeadMode = 0;
    AttractLastPlayfieldMode = 0;
    
    for (int count=0; count<4; count++) {  // blank out displays?
      BSOS_SetDisplayBlank(count, 0x00);     
    }
    BSOS_SetDisplayCredits(Credits);
    BSOS_SetDisplayBallInPlay(0);
    AttractLastHeadMode = 255;
    AttractLastPlayfieldMode = 255;
  } // one time entering attract mode
  
  // -------------------------------------------------------------------- head modes
  
  // Alternate displays between high score and blank
  if ((CurrentTime/6000)%2==0) {  // 6 seconds change time?

    if (AttractLastHeadMode!=1) {   // show high scores
      // NOTE: Trident2020 has new code that does scrolling scores here, also increases display time to allow time for scrolling high scores - not copied yet
      BSOS_SetLampState(HIGH_SCORE, 1, 0, 250);   // turn on high score light
      BSOS_SetLampState(GAME_OVER, 0);            // turn off game over light?
//      SetPlayerLamps(0);
  
      for (int count=0; count<4; count++) {
        BSOS_SetDisplay(count, HighScore, true, 2);
      }
      BSOS_SetDisplayCredits(Credits, true);
      BSOS_SetDisplayBallInPlay(0, true);
    }
    AttractLastHeadMode = 1;
    
  } else {  // blank high scores and show last game(s) played
    if (AttractLastHeadMode!=2) {
      BSOS_SetLampState(HIGH_SCORE, 0);
      BSOS_SetLampState(GAME_OVER, 1);
      BSOS_SetDisplayCredits(Credits, true);
      BSOS_SetDisplayBallInPlay(0, true);
      for (int count=0; count<4; count++) {
        if (CurrentNumPlayers>0) {
          if (count<CurrentNumPlayers) {
            BSOS_SetDisplay(count, CurrentScores[count], true, 2); 
          } else {
            BSOS_SetDisplayBlank(count, 0x00);
            BSOS_SetDisplay(count, 0);          
          }          
        } else {
          BSOS_SetDisplayBlank(count, 0x30);
          BSOS_SetDisplay(count, 0);          
        }
      }
    }
//    SetPlayerLamps(((CurrentTime/250)%4) + 1);
    AttractLastHeadMode = 2;
    
  }
  
  // ------------------------------------------------- playfield modes
  
  if ((CurrentTime/10000)%3==0) {   // ----------------------- mode 1
    if (AttractLastPlayfieldMode!=1) { 
      BSOS_TurnOffAllLamps();
    }

#if defined(ENABLE_ATTRACT)    
    ShowGoldenSaucerLamps();
    ShowParagonLamps();
    ShowAwardLamps();
    ShowParagonBlink();
#endif
    
    AttractLastPlayfieldMode=1;
    
  } else if ((CurrentTime/10000)%3==1) {  // ----------------- mode 2
    if (AttractLastPlayfieldMode!=2) {  // first time setup
      BSOS_TurnOffAllLamps();
    }

#if defined(ENABLE_ATTRACT)    
    AlternatePlayfieldLights();
#endif
    AttractLastPlayfieldMode=2;
  
  } else {                                  // ------------------ last mode
    if (AttractLastPlayfieldMode!=3) {  // first time setup
      BSOS_TurnOffAllLamps();
      
#if defined(ENABLE_ATTRACT)      
      AttractLastLadderBonus = 1;
      AttractLastLadderTime = CurrentTime;
#endif      
      
    }

#if defined(ENABLE_ATTRACT)    
    if ((CurrentTime-AttractLastLadderTime)>200) {
      AttractLastLadderBonus += 1;
      AttractLastLadderTime = CurrentTime;
      ShowBonusOnTree(AttractLastLadderBonus%MAX_DISPLAY_BONUS);
    }
      
    ShowParagonSweep();    
#endif
    
    AttractLastPlayfieldMode = 3;
  }
  
  
  
  // -------------------------------------------------- switch check
  
  // check for certain switches during attract mode: coins, start game, setup
  // any other switch will be displayed in console if debug enabled
  byte switchHit;
  while ( (switchHit=BSOS_PullFirstFromSwitchStack())!=SWITCH_STACK_EMPTY ) {
    if (switchHit==SW_CREDIT_RESET) {
      if (AddPlayer(true)) returnState = MACHINE_STATE_INIT_GAMEPLAY;
    } else if (switchHit==SW_COIN_1 || switchHit==SW_COIN_2 || switchHit==SW_COIN_3) {
//      AddCoinToAudit(switchHit);      
//      AddCredit(true, 1);  // is this a newer version that incorporates bsos_setdisplaycredits?
// above instead of below
      AddCredit();
      BSOS_SetDisplayCredits(Credits, true);
      
    } else if (switchHit==SW_SELF_TEST_SWITCH && (CurrentTime-GetLastSelfTestChangedTime())>250) {  // 1/2 switch debounce?
      returnState = MACHINE_STATE_TEST_LIGHTS;  // enter diagnostic mode
      SetLastSelfTestChangedTime(CurrentTime);
    } else {
#if (DEBUG_MESSAGES)
      char buf[128];
      sprintf(buf, "Switch 0x%02X (%d)\n", switchHit,switchHit);
      buf[127]=0;
      Serial.write(buf);
#endif      
    }
  }

  return returnState;
} // end: RunAttractMode()

//-----------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////
//
//  Game Play functions
//
////////////////////////////////////////////////////////////////////////////

byte CountBits(byte byteToBeCounted) {  // copied from Trident2020 not sure if needed yet
  byte numBits = 0;
  for (byte count=0; count<8; count++) {
    numBits += (byteToBeCounted&0x01);
    byteToBeCounted = byteToBeCounted>>1;
  }
  return numBits;
}
//-----------------------------------------------------------------

/* scratch pad



  DropTargetClearTime = CurrentTime;  // this might be used to do timed modes where we keep track of how much time has elapsed since drops were reset


*/
//-----------------------------------------------------------------

//-----------------------------------------------------------------
//-----------------------------------------------------------------
void GetHoldBonus(byte bset) {
  // sets global Bonus var based on BonusMem
  // called at beginning of ball
  if (bset & 1) Bonus+=20;
  if (bset & 2) Bonus+=30;
  if (bset & 4) Bonus+=40;
}
//-----------------------------------------------------------------
void SetHoldBonus(byte hbonus) {
  // sets BonusMem based on Bonus value
  // called at end of ball, max carry over hbonus =90k
  byte bset;

  if (hbonus>=40) { bset=4; hbonus-=40; } else { bset=0; }
  if (hbonus>=30) { bset=bset|2; hbonus-=30; }
  if (hbonus>=20) { bset=bset|1; }
  BonusMem[CurrentPlayer]=bset;
}
//-----------------------------------------------------------------

void HandleRightDropTargetHit(byte switchHit, unsigned long scoreMultiplier) {
  
// Needs to be optimized 
// more efficient?  byte switchMask = 1<<(SW_DROP_TARGET_1-switchHit);
//   PlaySoundEffect(SOUND_EFFECT_DT_SKILL_SHOT);

  // checking in reverse order in case 2+ hit at same time, so can't get sequential credit
  if (BSOS_ReadSingleSwitchState(SW_DROP_TOP) && (CurrentDropTargetsValid & 4)) {
    CurrentPlayerCurrentScore += 500;
    if ((CurrentDropTargetsValid & 7)==4) { DropSequence++; } // sequence working if only top up
    CurrentDropTargetsValid = CurrentDropTargetsValid & 123; // 127-bit value: turn off bit position value 4
  }  
  if (BSOS_ReadSingleSwitchState(SW_DROP_MIDDLE) && (CurrentDropTargetsValid & 2)) {
    CurrentPlayerCurrentScore += 500;
    CurrentDropTargetsValid = CurrentDropTargetsValid & 125; // 127-bit value: turn off bit 2
    if ((CurrentDropTargetsValid & 5)==4) { DropSequence=2; } // sequence working if only top still up
    else { DropSequence=0; }
  }
  if (BSOS_ReadSingleSwitchState(SW_DROP_BOTTOM) && (CurrentDropTargetsValid & 1)) {
    CurrentPlayerCurrentScore += 500;
    CurrentDropTargetsValid = CurrentDropTargetsValid & 126; // turn off bit 1
    if ((CurrentDropTargetsValid & 6)==6) { DropSequence=1; } // sequence working
    else { DropSequence=0; }
  }

  // --------------------- check to see if all drops down
  if (BSOS_ReadSingleSwitchState(SW_DROP_BOTTOM) && BSOS_ReadSingleSwitchState(SW_DROP_MIDDLE) && BSOS_ReadSingleSwitchState(SW_DROP_TOP)) {
    
    if (DropSequence==3) { // were they done in squence?
//   PlaySoundEffect();
    } else { 
//   PlaySoundEffect();      
    }
    if (DropsRightDownScore[CurrentPlayer]==30000) { // award special
      // NOTE: Do we want to limit special awards per game?
      AwardSpecial(); 
      if (DropSequence==3) AwardSpecial();  // Award a second special for in squence
    } else {
      CurrentPlayerCurrentScore+=(DropSequence==3?2:1)*DropsRightDownScore[CurrentPlayer];   
    }

//  DropTargetClearTime = CurrentTime;  // if we want to keep track of time
    DropsRightDownScore[CurrentPlayer]+=5000; // need to activate special & check boundaries
    if (DropsRightDownScore[CurrentPlayer]==35000) DropsRightDownScore[CurrentPlayer]=10000; // reset score tree after special
    // reset drops
    reset_3bank();    
 
    // handle waterfall ladder
    if (WaterfallSpecialAwarded) { WaterfallValue=2; }
    else {     
      WaterfallValue++;
      if (WaterfallValue>3) WaterfallValue=3;
    }
    if (!HuntMode) { HuntMode=true; }
  } // end: all drop targets down
  
} // end: HandleRightDropTargetHit()

//-----------------------------------------------------------------
void HandleGoldenSaucerHit() {

  if (GameMode==GAME_MODE_SKILL_SHOT) {
    if (SkillShotValue==2) {  // skill shot
      SkillShotsCollected[CurrentPlayer]++;   // need to save to player info
      SkillSweepPeriod-(SkillShotsCollected[CurrentPlayer]*SKILL_SHOT_DECREASE);
      CurrentPlayerCurrentScore+=GoldenSaucerValue*20000;
      PlaySoundEffect(SFX_SKILL1); // play skill shot sound
      GameMode=GAME_MODE_UNSTRUCTURED_PLAY;      
    }
  }

  CurrentPlayerCurrentScore+=GoldenSaucerValue*2000;
  GoldenSaucerValue++;
  if (GoldenSaucerValue>10) { GoldenSaucerValue=10; } 
 
  BSOS_PushToTimedSolenoidStack(SOL_SAUCER_GOLDEN, 5, CurrentTime + SAUCER_PARAGON_DURATION);  
}
//-----------------------------------------------------------------
void HandleParagonHit() {

  if (GameMode==GAME_MODE_SKILL_SHOT) {
    if (SkillShotValue==5) {  // skill shot
      SkillShotsCollected[CurrentPlayer]++;   // need to save to player info
      SkillSweepPeriod-(SkillShotsCollected[CurrentPlayer]*SKILL_SHOT_DECREASE);      
      AwardSpecial();
      //CurrentPlayerCurrentScore+=GoldenSaucerValue*20000;
      PlaySoundEffect(SFX_SKILL2); // play skill shot sound
      GameMode=GAME_MODE_UNSTRUCTURED_PLAY;      
    }
  }  
  
  
  if ((ParagonLit[CurrentPlayer]&128)==128) { // award special  or &127
  
    AwardSpecial();
    ParagonLit[CurrentPlayer]=0;  // reset this
    
  } else {
//    ParagonLit[CurrentPlayer]=(ParagonLit[CurrentPlayer] & ~(1 << (ParagonValue - 1))); // turn off 
    ParagonLit[CurrentPlayer]=(ParagonLit[CurrentPlayer] |(1 << (ParagonValue))); // turn on
    
    AddToBonus(1);
    // play special sound
  }
  if ((ParagonLit[CurrentPlayer]&127)==127) { // Paragon lit
    ParagonLit[CurrentPlayer]=255; // special list
    // play special sound
  }
  
  BSOS_PushToTimedSolenoidStack(SOL_SAUCER_PARAGON, 5, CurrentTime + SAUCER_PARAGON_DURATION);   
}
//-----------------------------------------------------------------
void HandleTreasureSaucerHit() {
  switch (TreasureValue) {
      case 1:
        CurrentPlayerCurrentScore+=5000;
        BonusX=5;
        break;
      case 2:
        AwardExtraBall();
        break;
      case 3:
        AwardSpecial();
        reset_inline();
  }
  TreasureValue++;
  if (TreasureValue>3) TreasureValue=1;
 
  BSOS_PushToTimedSolenoidStack(SOL_SAUCER_TREASURE, 5, CurrentTime + SAUCER_PARAGON_DURATION);
}
//-----------------------------------------------------------------

void setup() {
  if (DEBUG_MESSAGES) {
    Serial.begin(115200);
  }

  // Start out with everything tri-state, in case the original
  // CPU is running
  // Set data pins to input
  // Make pins 2-7 input
  DDRD = DDRD & 0x03;
  // Make pins 8-13 input
  DDRB = DDRB & 0xC0;
  // Set up the address lines A0-A5 as input (for now)
  DDRC = DDRC & 0xC0;

  unsigned long startTime = millis();
  boolean sawHigh = false;
  boolean sawLow = false;
  // for three seconds, look for activity on the VMA line (A5)
  // If we see anything, then the MPU is active so we shouldn't run
  while ((millis()-startTime)<1200) {
    if (digitalRead(A5)) sawHigh = true;
    else sawLow = true;
  }
  // If we saw both a high and low signal, then someone is toggling the 
  // VMA line, so we should hang here forever (until reset)
  if (sawHigh && sawLow) {
    while (1);
  }
    
  // Tell the OS about game-specific lights and switches
  BSOS_SetupGameSwitches(NUM_SWITCHES_WITH_TRIGGERS, NUM_PRIORITY_SWITCHES_WITH_TRIGGERS, TriggeredSwitches);

  if (DEBUG_MESSAGES) {
    Serial.write("Attempting to initialize the MPU\n");
  }
 
  // Set up the chips and interrupts
  BSOS_InitializeMPU();
  BSOS_DisableSolenoidStack();
  BSOS_SetDisableFlippers(true);
  
/*
  // method 1 - use dip switches
  byte dipBank = BSOS_GetDipSwitches(0);
  // Use dip switches to set up game variables
  if (DEBUG_MESSAGES) {
    char buf[32];
    sprintf(buf, "DipBank 0 = 0x%02X\n\r", dipBank);
    Serial.write(buf);
  }
  HighScore = BSOS_ReadULFromEEProm(BSOS_HIGHSCORE_EEPROM_START_BYTE, 10000);
  Credits = BSOS_ReadByteFromEEProm(BSOS_CREDITS_EEPROM_BYTE);
  if (Credits>MaximumCredits) Credits = MaximumCredits;
*/

  // method 2 - read from arduino eeprom
  // Read parameters from EEProm
  ReadStoredParameters();
  CurrentScores[0] = MAJOR_VERSION;
  CurrentPlayerCurrentScore = MAJOR_VERSION;
  CurrentScores[1] = MINOR_VERSION;
  CurrentScores[2] = BALLY_STERN_OS_MAJOR_VERSION;
  CurrentScores[3] = BALLY_STERN_OS_MINOR_VERSION;
  ResetScoresToClearVersion = true;

  
  
#if defined(USE_WAV_TRIGGER) || defined(USE_WAV_TRIGGER_1p3)
  // WAV Trigger startup at 57600
  wTrig.start();
  wTrig.masterGain(0);
  wTrig.setAmpPwr(false);
  delay(10);

  // Send a stop-all command and reset the sample-rate offset, in case we have
  //  reset while the WAV Trigger was already playing.
  wTrig.stopAllTracks();
  wTrig.samplerateOffset(0);
#endif  
  
  
  
  
  BallsPerGame = 3;

  CurrentTime = millis();
  PlaySoundEffect(SFX_GAME_INTRO);  
  
  if (DEBUG_MESSAGES) {
    Serial.write("Done with setup\n");
  }

} // END: setup()   Next runs loop()

//-----------------------------------------------------------------
void HuntSuccess() {
  CurrentPlayerCurrentScore+=HuntReward*10;
  CurrentPlayerCurrentScore++; // add 1 for hunts
  // show some cool light effect and sound
  HuntMode=false;
  HuntStartTime=0;
  HuntsCompleted[CurrentPlayer]++;
  HuntReward+=1000; // let's add another 10k to hunt reward for every one completed per ball
  BSOS_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime, true); // why not
  // psfx
}
//-----------------------------------------------------------------
void HuntFailed() {
  HuntMode=false;
  HuntStartTime=0;
  // psfx
}
//-----------------------------------------------------------------
void HuntMissed() {
  // psfx you missed
}
//-----------------------------------------------------------------
void RunHunt() {
  if (HuntStartTime==0) {  // initialization
    HuntStartTime=CurrentTime;
    HuntLocation=random(7);    // set the location
    HuntShotTime=CurrentTime;  // current shot starts now
    HuntShotLength=HUNT_BASE_SHOT_LENGTH-(500*HuntsCompleted[CurrentPlayer]);
    if (HuntShotLength<500) HuntShotLength=500;
    HuntFrozen=false;
    // psfx
    
    return;
  }
  if (CurrentTime-HuntStartTime>HUNT_MODE_LENGTH) HuntFailed();
  else {
    if ((CurrentTime-HuntShotTime)>HuntShotLength) { // move shot
      HuntLastShot=HuntLocation; // for "you missed" sfx
      HuntLocation++;  
      if (HuntLocation>6) HuntLocation=0;
      HuntFrozen=false;  // can freeze this shot
      HuntShotTime=CurrentTime;
    }
  }
} // end: RunHunt()

//-----------------------------------------------------------------
void ShowHuntLamps() {
// animate the Challenge beast shot lamps
//  byte anim=10+(((((CurrentTime-HuntShotTime)/500)%8)+1)*10);
  byte anim=30;
  switch (HuntLocation) {
    case 0: // inlines.
//      BSOS_SetLampState(L_TREASURE_5X,1,0,anim);
      BSOS_SetLampState(L_TREASURE_SPECIAL,1,0,anim);
      BSOS_SetLampState(L_TREASURE_EB,1,0,anim);
      break;
    case 1: // upper drop.
      BSOS_SetLampState(L_10K_DROPS,1,0,anim);
      BSOS_SetLampState(L_15K_DROPS,1,0,anim);
      break;      
    case 2: // spinner.
      BSOS_SetLampState(L_SPINNER_1,1,0,anim);
      BSOS_SetLampState(L_SPINNER_2,1,0,anim);
      BSOS_SetLampState(L_SPINNER_3,1,0,anim);            
      BSOS_SetLampState(L_SPINNER_4,1,0,anim);
      break;
    case 3: // lower drop.
      BSOS_SetLampState(L_20K_DROPS,1,0,anim);
      BSOS_SetLampState(L_25K_DROPS,1,0,anim); 
      break;
    case 4: // golden cliffs.
      BSOS_SetLampState(L_10K_GOLDEN,1,0,anim);
      BSOS_SetLampState(L_20K_GOLDEN,1,0,anim);
      break;
    case 5: // standup.
      BSOS_SetLampState(L_CENTER_G,1,0,anim);    
      break;
    case 6: // paragon saucer.
      BSOS_SetLampState(L_SAUCER_R,1,0,anim);
      BSOS_SetLampState(L_SAUCER_AR,1,0,anim);
      BSOS_SetLampState(L_SAUCER_G,1,0,anim);    
  }
}
//-----------------------------------------------------------------
boolean HuntHit(byte sw) {
// check for hunt switch hits

// NOTE need to debounce spinner so we only count first spinner hit
  switch (HuntLocation) {
    case(0):
      if ((sw==SW_DROP_INLINE_A) || (sw==SW_DROP_INLINE_B) || (sw==SW_DROP_INLINE_C) || (sw==SW_DROP_INLINE_D) || (sw==SW_SAUCER_TREASURE))
      return(true);
      break;
    default:
      if (HuntSwitches[HuntLocation-1]==sw) return(true);
  }
  return(false);
}
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//-----------------------------------------------------------------
//-----------------------------------------------------------------
// zz


















unsigned long InitGameStartTime = 0;
// ----------------------------------------------------------------

int InitGamePlay(boolean curStateChanged) {

  if (DEBUG_MESSAGES) {
    Serial.write("InitGamePlay\n\r");
  }  

  int returnState = MACHINE_STATE_INIT_GAMEPLAY;

  
  if (curStateChanged) { // run once/first-time    

  if (DEBUG_MESSAGES) {
    Serial.write("InitGamePlay - state changed, runonce\n\r");
  }    
  
#if defined(USE_WAV_TRIGGER) || defined(USE_WAV_TRIGGER_1p3)
    wTrig.stopAllTracks();
    wTrig.samplerateOffset(0);
#endif
      
    InitGameStartTime = CurrentTime;
    BSOS_SetCoinLockout((Credits>=MaximumCredits)?true:false);
    
    BSOS_SetDisableFlippers(true);
    BSOS_DisableSolenoidStack();
    BSOS_TurnOffAllLamps();
    BSOS_SetDisplayBallInPlay(1);

    // Set up general game variables
    CurrentPlayer = 0;
    CurrentBallInPlay = 1;
    SamePlayerShootsAgain = false;    
    
    for (int count=0; count<4; count++) {
      // init game play variables
      CurrentScores[count] = 0;          // zero scores
      SkillShotsCollected[count]=0;      // zero skill shots collected
      
      // game specific values that carry over from ball-to-ball only init here - xx      
      DropsRightDownScore[count]=10000;  // reset right drops value tree
      BonusHeld[count]=0;                 // any 20k+ bonus 
      GoldenSaucerMem[count]=1;          // zero player golden saucer values
      ParagonLit[count]=0;
      BonusMem[count]=0;                 // 0=none, 1=20k, 2=30k, 3=40k
          
    }
    CurrentPlayerCurrentScore=0;        // start with 0
  

    // if the ball is in the outhole, then we can move on
    if (BSOS_ReadSingleSwitchState(SW_OUTHOLE)) {
      if (DEBUG_MESSAGES) {
        Serial.write("Ball is in trough - starting new ball\n");
      }
      BSOS_EnableSolenoidStack();
      BSOS_SetDisableFlippers(false);
      returnState = MACHINE_STATE_INIT_NEW_BALL;
    } else {

      if (DEBUG_MESSAGES) {
        Serial.write("Ball is not in trough - firing stuff and giving it a chance to come back\n");
      }
      
      // Otherwise, let's see if it's in a spot where it could get trapped,
      // for instance, a saucer (if the game has one)
//      BSOS_PushToSolenoidStack(SOL_SAUCER, 5, true);

      // And then set a base time for when we can continue
      InitGameStartTime = CurrentTime;
    }

    for (int count=0; count<4; count++) {
      BSOS_SetDisplay(count, 0);
      BSOS_SetDisplayBlank(count, 0x00);
    }
    
    ShowPlayerScores(0xFF, false, false); // new



RunDemo();
    
  } // end run-once moving on

  // Wait for TIME_TO_WAIT_FOR_BALL seconds, or until the ball appears
  // The reason to bail out after TIME_TO_WAIT_FOR_BALL is just
  // in case the ball is already in the shooter lane.
  if ((CurrentTime-InitGameStartTime)>TIME_TO_WAIT_FOR_BALL || BSOS_ReadSingleSwitchState(SW_OUTHOLE)) {
    BSOS_EnableSolenoidStack();
    BSOS_SetDisableFlippers(false);
    returnState = MACHINE_STATE_INIT_NEW_BALL;
  }
  
  return returnState;  
}

//==================================================== zz
//
int InitNewBall(bool curStateChanged, byte playerNum, int ballNum) {  

  if (curStateChanged) {
    
if (DEBUG_MESSAGES) { 
  Serial.write("InitNewBall(curStateChanged)\r\n");
}         
    
    BallFirstSwitchHitTime = 0;
    SamePlayerShootsAgain = false;

    BSOS_SetDisableFlippers(false);
    BSOS_EnableSolenoidStack(); 
    BSOS_SetDisplayCredits(Credits, true);
    
    if (BSOS_ReadSingleSwitchState(SW_OUTHOLE)) { // kick ball into shooter lane
      BSOS_PushToTimedSolenoidStack(SOL_OUTHOLE, 4, CurrentTime + 100);
    }

    for (int count=0; count<CurrentNumPlayers; count++) {   // show scores on display
      BSOS_SetDisplay(count, CurrentScores[count], true, 2);
    }

    BSOS_SetDisplayBallInPlay(ballNum);
    BSOS_SetLampState(BALL_IN_PLAY, 1);
    BSOS_SetLampState(TILT, 0);

    if (BallSaveNumSeconds>0) { // if ball save active, turn on backbox light
      BSOS_SetLampState(L_BB_SHOOT_AGAIN, 1, 0, 500);
    }

    reset_3bank();  // reset right 3-bank
    reset_inline(); // reset inline drops

    Bonus = 1;
    ShowBonusOnTree(1);
    BonusX = 1;
    BallSaveUsed = false;
    BallTimeInTrough = 0;
    NumTiltWarnings = 0;
    LastTiltWarningTime = 0;
    MoveParagon=true;

    // Initialize game-specific start-of-ball lights & variables    
//    DropTargetClearTime = 0;
    ExtraBallCollected = false;
    
    HuntReward=HUNT_BASE_REWARD*(HuntsCompleted[playerNum]+1);
    
    // reset ball specific ladders

    WaterfallValue=0;
    RightDropsValue=1;
    WaterfallSpecialAwarded=false;  // reset each ball?
    SpinnerValue=1;
    TreasureValue=1;

// new ball setups
/*
    CurrentPlayerCurrentScore = CurrentScores[CurrentPlayer]; // Reset score at top 
    CurrentStandupsHit = StandupsHit[CurrentPlayer]; // not used
    GoldenSaucerValue=GoldenSaucerMem[CurrentPlayer]; // Carries from ball to ball
    GetHoldBonus(BonusMem[CurrentPlayer]);
*/

    CurrentPlayerCurrentScore=CurrentScores[playerNum]; // Reset score at top 
//  CurrentStandupsHit=StandupsHit[playerNum]; // not used
    GoldenSaucerValue=GoldenSaucerMem[playerNum]; // Carries from ball to ball
    GetHoldBonus(BonusMem[playerNum]);    


if (DEBUG_MESSAGES) { 
      char buf[128];
      sprintf(buf, "InitNewBall: Ball %d Over P%d CurrentScore=%d\n\r",CurrentBallInPlay,CurrentPlayer,CurrentPlayerCurrentScore);
      buf[127]=0;
      Serial.write(buf);
      
      for (byte x=0;x<4;x++) {
        sprintf(buf, " CurrentScores[%d]=%d\n\r",x,CurrentScores[x]);
      Serial.write(buf);        
      }     
}  

    
    
    ParagonValue=0; // p-a-r-a-g-o-n or 8

    // set up skill shot
    GameModeStartTime=CurrentTime;
    GameMode=GAME_MODE_SKILL_SHOT;
    EnableSkillFx=true;
    
  } // end new ball init


  // check to see if ball in saucers
  if (BSOS_ReadSingleSwitchState(SW_SAUCER_TREASURE)) BSOS_PushToSolenoidStack(SOL_SAUCER_TREASURE, 5);  
  if (BSOS_ReadSingleSwitchState(SW_SAUCER_PARAGON)) BSOS_PushToSolenoidStack(SOL_SAUCER_PARAGON, 5);
  if (BSOS_ReadSingleSwitchState(SW_SAUCER_GOLDEN)) BSOS_PushToSolenoidStack(SOL_SAUCER_GOLDEN, 5);

  

  
  // We should only consider the ball initialized when 
  // the ball is no longer triggering the SW_OUTHOLE
  if (BSOS_ReadSingleSwitchState(SW_OUTHOLE)) {
    return MACHINE_STATE_INIT_NEW_BALL;  // loop again
  } else {

    return MACHINE_STATE_NORMAL_GAMEPLAY;
  }
  
}


//====================================================

boolean PlayerUpLightBlinking = false;

int NormalGamePlay() {
  int returnState = MACHINE_STATE_NORMAL_GAMEPLAY;


  // If the playfield hasn't been validated yet, flash score and player up num
  if (BallFirstSwitchHitTime==0) {
    BSOS_SetDisplayFlash(CurrentPlayer, CurrentScores[CurrentPlayer], CurrentTime, 500, 2);
    if (!PlayerUpLightBlinking) {
//      SetPlayerLamps((CurrentPlayer+1), 500);
      PlayerUpLightBlinking = true;
    }
  } else {
    if (PlayerUpLightBlinking) {
//      SetPlayerLamps((CurrentPlayer+1));
      PlayerUpLightBlinking = false;
    }
  }

  if (GameMode==GAME_MODE_SKILL_SHOT) RunSkillShotMode();
  
  if (HuntMode) { 
    RunHunt();
    ShowHuntLamps();
  }
  
  // lamp functions
  ShowShootAgainLamp();
  ShowGoldenSaucerLamps();
  //ShowBonusOnTree(Bonus);  // replace with ShowBonusLamps() when using modes
  ShowBonusLamps();
  ShowAwardLamps();  // waterfall and drops
  ShowParagonLamps();

  
// new
  ShowPlayerScores(CurrentPlayer, (BallFirstSwitchHitTime==0)?true:false, (BallFirstSwitchHitTime>0 && ((CurrentTime-LastTimeScoreChanged)>2000))?true:false);  
  
  // Check to see if ball is in the outhole
  if (BSOS_ReadSingleSwitchState(SW_OUTHOLE)) {
    if (BallTimeInTrough==0) {
      BallTimeInTrough = CurrentTime;
    } else {
      // Make sure the ball stays on the sensor for at least 
      // 0.5 seconds to be sure that it's not bouncing
      if ((CurrentTime-BallTimeInTrough)>500) {

        if (BallFirstSwitchHitTime==0) BallFirstSwitchHitTime = CurrentTime;
        
        // if we haven't used the ball save, and we're under the time limit, then save the ball
        if (  !BallSaveUsed && 
              ((CurrentTime-BallFirstSwitchHitTime)/1000)<((unsigned long)BallSaveNumSeconds) ) {
        
          BSOS_PushToTimedSolenoidStack(SOL_OUTHOLE, 4, CurrentTime + 100);
          if (BallFirstSwitchHitTime>0) {
            BallSaveUsed = true;
            BSOS_SetLampState(L_SHOOT_AGAIN, 0);
            BSOS_SetLampState(L_BB_SHOOT_AGAIN, 0);
            GameMode=GAME_MODE_UNSTRUCTURED_PLAY;  // turn off any skill shot mode
            ResetLights();  // set playfield lights to normal
            // play ball save sfx            
          }
          BallTimeInTrough = CurrentTime;

          returnState = MACHINE_STATE_NORMAL_GAMEPLAY;          
        } else {
          // one-time, end of ball routines
          SetHoldBonus(Bonus);  // works well - one time end of ball call
          
          returnState = MACHINE_STATE_COUNTDOWN_BONUS;
        }
      }
    }
  } else {
    BallTimeInTrough = 0;
  }

  return returnState;
}

//====================================================

unsigned long CountdownStartTime = 0;
unsigned long LastCountdownReportTime = 0;
unsigned long BonusCountDownEndTime = 0;

int CountdownBonus(boolean curStateChanged) {

  // If this is the first time through the countdown loop
  if (curStateChanged) {
    BSOS_SetLampState(BALL_IN_PLAY, 1, 0, 250);

    CountdownStartTime = CurrentTime;
    ShowBonusOnTree(Bonus);

    LastCountdownReportTime = CountdownStartTime;
    BonusCountDownEndTime = 0xFFFFFFFF;
  }

  if ((CurrentTime - LastCountdownReportTime) > 200) { // # ms to count down bonus?

    if (Bonus > 0) {

      // Only give sound & score if this isn't a tilt
      if (NumTiltWarnings <= MaxTiltWarnings) {
//        PlaySoundEffect(SOUND_EFFECT_BONUS_COUNT + (BonusX-1));
        CurrentPlayerCurrentScore += 1000*((unsigned long)BonusX);
      }

      Bonus -= 1;
      ShowBonusOnTree(Bonus);
    } else if (BonusCountDownEndTime == 0xFFFFFFFF) {
//      PlaySoundEffect(SOUND_EFFECT_BALL_OVER);
      BSOS_SetLampState(L_1K_BONUS, 0);
      BonusCountDownEndTime = CurrentTime + 1000;
    }
    LastCountdownReportTime = CurrentTime;
  }

  if (CurrentTime > BonusCountDownEndTime) {

    // Reset any lights & variables of goals that weren't completed

    BonusCountDownEndTime = 0xFFFFFFFF;
    return MACHINE_STATE_BALL_OVER;
  }

  return MACHINE_STATE_COUNTDOWN_BONUS;
}

//====================================================
void CheckHighScores() {
  unsigned long highestScore = 0;
  int highScorePlayerNum = 0;
  for (int count = 0; count < CurrentNumPlayers; count++) {
    if (CurrentScores[count] > highestScore) highestScore = CurrentScores[count];
    highScorePlayerNum = count;
  }

  if (highestScore > HighScore) {
    HighScore = highestScore;
    if (HighScoreReplay) {
      AddCredit(false, 3);
      BSOS_WriteULToEEProm(BSOS_TOTAL_REPLAYS_EEPROM_START_BYTE, BSOS_ReadULFromEEProm(BSOS_TOTAL_REPLAYS_EEPROM_START_BYTE) + 3);
    }
    BSOS_WriteULToEEProm(BSOS_HIGHSCORE_EEPROM_START_BYTE, highestScore);
    BSOS_WriteULToEEProm(BSOS_TOTAL_HISCORE_BEATEN_START_BYTE, BSOS_ReadULFromEEProm(BSOS_TOTAL_HISCORE_BEATEN_START_BYTE) + 1);

    for (int count = 0; count < 4; count++) {
      if (count == highScorePlayerNum) {
        BSOS_SetDisplay(count, CurrentScores[count], true, 2);
      } else {
        BSOS_SetDisplayBlank(count, 0x00);
      }
    }

    BSOS_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime, true);
    BSOS_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 300, true);
    BSOS_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime + 600, true);
  }
}


//====================================================
#if defined(ENABLE_MATCH)

unsigned long MatchSequenceStartTime = 0;
unsigned long MatchDelay = 150;
byte MatchDigit = 0;
byte NumMatchSpins = 0;
byte ScoreMatches = 0;

int ShowMatchSequence(boolean curStateChanged) {
  if (!MatchFeature) return MACHINE_STATE_ATTRACT;

  if (curStateChanged) {
    MatchSequenceStartTime = CurrentTime;
    MatchDelay = 1500;
//    MatchDigit = random(0, 10);
    MatchDigit = CurrentTime%10;
    NumMatchSpins = 0;
    BSOS_SetLampState(MATCH, 1, 0);
    BSOS_SetDisableFlippers();
    ScoreMatches = 0;
    BSOS_SetLampState(BALL_IN_PLAY, 0);
  }

  if (NumMatchSpins < 40) {
    if (CurrentTime > (MatchSequenceStartTime + MatchDelay)) {
      MatchDigit += 1;
      if (MatchDigit > 9) MatchDigit = 0;
      //PlaySoundEffect(10+(MatchDigit%2));
//      PlaySoundEffect(SOUND_EFFECT_MATCH_SPIN);
      BSOS_SetDisplayBallInPlay((int)MatchDigit * 10);
      MatchDelay += 50 + 4 * NumMatchSpins;
      NumMatchSpins += 1;
      BSOS_SetLampState(MATCH, NumMatchSpins % 2, 0);

      if (NumMatchSpins == 40) {
        BSOS_SetLampState(MATCH, 0);
        MatchDelay = CurrentTime - MatchSequenceStartTime;
      }
    }
  }

  if (NumMatchSpins >= 40 && NumMatchSpins <= 43) {
    if (CurrentTime > (MatchSequenceStartTime + MatchDelay)) {
      if ( (CurrentNumPlayers > (NumMatchSpins - 40)) && ((CurrentScores[NumMatchSpins - 40] / 10) % 10) == MatchDigit) {
        ScoreMatches |= (1 << (NumMatchSpins - 40));
        AddSpecialCredit();
       
        BSOS_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime, true); 
       
        MatchDelay += 1000;
        NumMatchSpins += 1;
        BSOS_SetLampState(MATCH, 1);
      } else {
        NumMatchSpins += 1;
      }
      if (NumMatchSpins == 44) {
        MatchDelay += 5000;
      }
    }
  }

  if (NumMatchSpins > 43) {
    if (CurrentTime > (MatchSequenceStartTime + MatchDelay)) {
      return MACHINE_STATE_ATTRACT;
    }
  }

  for (int count = 0; count < 4; count++) {
    if ((ScoreMatches >> count) & 0x01) {
      // If this score matches, we're going to flash the last two digits
      if ( (CurrentTime / 200) % 2 ) {
        BSOS_SetDisplayBlank(count, BSOS_GetDisplayBlank(count) & 0x0F);
      } else {
        BSOS_SetDisplayBlank(count, BSOS_GetDisplayBlank(count) | 0x30);
      }
    }
  }

  return MACHINE_STATE_MATCH_MODE;
}
#endif // ENABLE_MATCH
//====================================================

int RunGamePlayMode(int curState, boolean curStateChanged) {
  int returnState = curState;
//  unsigned long scoreAtTop = CurrentScores[CurrentPlayer];
  // unsigned long scoreAtTop = CurrentPlayerCurrentScore;
  
  byte bonusAtTop = Bonus;

  unsigned long scoreMultiplier = 1;  // currently not implemented
  
  
  // Very first time into gameplay loop
  if (curState==MACHINE_STATE_INIT_GAMEPLAY) {
    returnState = InitGamePlay(curStateChanged);    
  } else if (curState==MACHINE_STATE_INIT_NEW_BALL) {
    returnState = InitNewBall(curStateChanged, CurrentPlayer, CurrentBallInPlay);
  } else if (curState==MACHINE_STATE_NORMAL_GAMEPLAY) {
    returnState = NormalGamePlay(); // also includes skill shot/modes
  } else if (curState==MACHINE_STATE_COUNTDOWN_BONUS) {
    //   Do not put one-time end of ball stuff here - this loops
    returnState = CountdownBonus(curStateChanged);
    ShowPlayerScores(CurrentPlayer, (BallFirstSwitchHitTime==0)?true:false, (BallFirstSwitchHitTime>0 && ((CurrentTime-LastTimeScoreChanged)>2000))?true:false);

//    returnState = MACHINE_STATE_BALL_OVER; // disable this when countdown enabled
    
  } else if (curState==MACHINE_STATE_BALL_OVER) {    

    // end of ball memory settings ---- SAVE PLAYER STATE AT BALL END
    CurrentScores[CurrentPlayer] = CurrentPlayerCurrentScore;
    
    // If you tilt out, you might lose the progress below
    GoldenSaucerMem[CurrentPlayer] = GoldenSaucerValue;
    // SetHoldBonus done before countdown bonus
  
    if (SamePlayerShootsAgain) {
      returnState = MACHINE_STATE_INIT_NEW_BALL;
    } else {  // ---- new player
      CurrentPlayer+=1;
      if (CurrentPlayer>=CurrentNumPlayers) {
        CurrentPlayer = 0;
        CurrentBallInPlay+=1;
      }
      // ----- NEW PLAYER MEMORY LOADS ----------------------------------------------
      CurrentPlayerCurrentScore=CurrentScores[CurrentPlayer];
      GoldenSaucerValue=GoldenSaucerMem[CurrentPlayer];
      
      if (CurrentBallInPlay>BallsPerGame) {
        CheckHighScores();
//        PlaySoundEffect(SOUND_EFFECT_GAME_OVER);
//        SetPlayerLamps(0);
        for (int count=0; count<CurrentNumPlayers; count++) {
          BSOS_SetDisplay(count, CurrentScores[count], true, 2);
        }

        returnState = MACHINE_STATE_MATCH_MODE;
      }
      else returnState = MACHINE_STATE_INIT_NEW_BALL;
    } // new ball/player?
    
  } else if (curState==MACHINE_STATE_MATCH_MODE) {
    #if defined(ENABLE_MATCH)
    returnState = ShowMatchSequence(curStateChanged);
    #else
    returnState = MACHINE_STATE_GAME_OVER; // disable match
    #endif
  } else {
    returnState = MACHINE_STATE_ATTRACT;
  }

  // ============ SWITCH HITS ========================================================
  byte switchHit;
  while ( (switchHit=BSOS_PullFirstFromSwitchStack())!=SWITCH_STACK_EMPTY ) {

    if (HuntMode) { if (HuntHit(switchHit)) HuntSuccess(); }
//    if ((HuntMode) && HuntHit(switchHit)) HuntSuccess();  // does short circuit work?
    
    switch (switchHit) {
      case SW_SELF_TEST_SWITCH:
        returnState = MACHINE_STATE_TEST_LIGHTS;
        SetLastSelfTestChangedTime(CurrentTime);
        break; 
      case SW_COIN_1:
      case SW_COIN_2:
      case SW_COIN_3:
        AddCredit();
        BSOS_SetDisplayCredits(Credits, true);
        break;
      case SW_CREDIT_RESET:
        if (CurrentBallInPlay<2) {
          // If we haven't finished the first ball, we can add players
          AddPlayer();
        } else {
          // If the first ball is over, pressing start again resets the game
          returnState = MACHINE_STATE_INIT_GAMEPLAY;
        }
        if (DEBUG_MESSAGES) {
          Serial.write("Start game button pressed\n\r");
        }
        break;   

      // game-specific switch action ===============================================
      
        // 3-bank drop targets
        case SW_DROP_TOP:
        case SW_DROP_MIDDLE:
        case SW_DROP_BOTTOM:
/*        
if (DEBUG_MESSAGES) { 
      char buf[128];
      sprintf(buf, "Right Drop (%d) [%d]\n\r", switchHit,CurrentDropTargetsValid);
      Serial.write(buf);
} */    
          HandleRightDropTargetHit(switchHit,scoreMultiplier);
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;
          break;      
          
        // Standup targets
        case SW_BOTTOM_STANDUP:
        case SW_TOP_STANDUP:

          if ((HuntMode) && (!HuntFrozen)) { // handle stunning during the hunt by hitting standups
            HuntShotTime=CurrentTime;        
            HuntFrozen=true;
            // sfx stunned beast
          }
 
          CurrentPlayerCurrentScore+=10;
          AddToBonus(1);
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;
          break;
          
        // inline drops
        // NOTE - not keeping track of target masks because really doesn't matter they have to be hit in sequential order anyway
        case SW_DROP_INLINE_A:
          CurrentPlayerCurrentScore+=1000;
          AddToBonus(1);
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;
          break;
        case SW_DROP_INLINE_B:
          CurrentPlayerCurrentScore+=1000;
          AddToBonus(1);
          // no BallFirstSwitchHitTime because shouldn't be hittable 
          break;        
        case SW_DROP_INLINE_C: // 2x
          if (BonusX>4) { // already maxxed multiplier
            CurrentPlayerCurrentScore+=1000;
            AddToBonus(1);            
          } else { BonusX=2; }
          // no BallFirstSwitchHitTime because shouldn't be hittable 
          break;                
        case SW_DROP_INLINE_D: // 3x
          if (BonusX>4) { // already maxxed multiplier
            CurrentPlayerCurrentScore+=1000;
            AddToBonus(1);            
          } else { BonusX=3; }
          // no BallFirstSwitchHitTime because shouldn't be hittable 
          break;   

        // lanes
        case SW_RIGHT_OUTLANE:
          CurrentPlayerCurrentScore+=1000;
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;        
          break;
        case SW_RIGHT_INLANE:
          CurrentPlayerCurrentScore+=1000;
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;        
          break;          
          
        // Paragon saucer
        case SW_SAUCER_PARAGON:
          // We only count a saucer hit if it hasn't happened in the last 500ms software debounce)
          MoveParagon=false;
          // probably play sound here before debounce?
          if (SaucerHitTime==0 || (CurrentTime-SaucerHitTime)>SAUCER_HOLD_TIME) { // saucer_debounce_time 500
            SaucerHitTime = CurrentTime;
            HandleParagonHit();
          }
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime; 
          break;

        // Paragon saucer
        case SW_SAUCER_GOLDEN:
          if (SaucerHitTime==0 || (CurrentTime-SaucerHitTime)>SAUCER_DEBOUNCE_TIME) {
            SaucerHitTime = CurrentTime;
            HandleGoldenSaucerHit();
          }
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime; 
          break;

        case SW_TOP_ROLLOVER:
          CurrentPlayerCurrentScore+=500;
          AddToBonus(1);
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;        
          break;          
          
        case SW_500_REBOUND:
          CurrentPlayerCurrentScore+=500;
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;        
          break;
          
        case SW_WATERFALL_ROLLOVER: // 1k, 5k, 10k, special, back to 10k stay per ball
          // waterfall value advanced by right 3-bank drops
          AddToBonus(1);
          switch (WaterfallValue) {
            case 0:
              CurrentPlayerCurrentScore+=1000;
              break;
            case 1:
            case 2:
              CurrentPlayerCurrentScore+=WaterfallValue*5000;
              break;
            case 3:
              AwardSpecial();
              WaterfallSpecialAwarded=true; // Note this isn't saved per player
              WaterfallValue=2;
              break;
          }        
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;        
          break;
          
        case SW_SPINNER:  // 100+ every 5 hits add bonus
// do we want to debounce spinner during hunt mode?  yy        
          CurrentPlayerCurrentScore+=100;
          SpinnerValue++;
          if (SpinnerValue>5) { AddToBonus(1); SpinnerValue=1; }
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;        
          break;       

        case SW_STAR_ROLLOVER:  // also rebound behind right drops
          CurrentPlayerCurrentScore+=50;
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;        
          break;
          
        case SW_RIGHT_SLING:
        case SW_LEFT_SLING:
          CurrentPlayerCurrentScore+=500;
          HuntReward+=HUNT_SLING_VALUE;
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;        
          break;          
          
        case SW_BEAST_BUMPER:
          CurrentPlayerCurrentScore+=100;
          HuntReward+=HUNT_BEAST_VALUE;          
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;        
          break;        
        case SW_CENTER_BUMPER:
        case SW_RIGHT_BUMPER:
        case SW_LEFT_BUMPER:
          HuntReward+=HUNT_POPS_VALUE;        
          CurrentPlayerCurrentScore+=100;
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;        
          break;  

        // treasure chamber saucer
        case SW_SAUCER_TREASURE:
          if (SaucerHitTime==0 || (CurrentTime-SaucerHitTime)>SAUCER_DEBOUNCE_TIME) {
            SaucerHitTime = CurrentTime;
            HandleTreasureSaucerHit();
          }
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime; 
          break;          
           
          
          
    }
  } // while switch hit
  
  // Let's check for things to un-freeze - should this be moved to showParagon?
  if ((CurrentTime-SaucerHitTime)>SAUCER_HOLD_TIME) MoveParagon=true;  

  // This constantly updates the player scores array - otherwise, if someone tilts the score value for next ball will be value at start of ball  
  if (CurrentPlayerCurrentScore != CurrentScores[CurrentPlayer]) {
    CurrentScores[CurrentPlayer]=CurrentPlayerCurrentScore;
    ShowPlayerScores(0xFF, false, false);   
  }
  
//  if (scoreAtTop!=CurrentScores[CurrentPlayer]) {
//    Serial.write("Score changed\n");
//  }

  return returnState;
}

//====================================================

void loop() {
  // This line has to be in the main loop
  BSOS_DataRead(0);

  CurrentTime = millis();
  int newMachineState = MachineState;

  // Machine state is self-test/attract/game play
  if (MachineState<0) {
    newMachineState = RunSelfTest(MachineState, MachineStateChanged);
  } else if (MachineState==MACHINE_STATE_ATTRACT) {
    newMachineState = RunAttractMode(MachineState, MachineStateChanged);
  } else {
    newMachineState = RunGamePlayMode(MachineState, MachineStateChanged);
  }

  if (newMachineState!=MachineState) {
    MachineState = newMachineState;
    MachineStateChanged = true;
  } else {
    MachineStateChanged = false;
  }

  BSOS_ApplyFlashToLamps(CurrentTime);
  BSOS_UpdateTimedSolenoidStack(CurrentTime);
}

//====================================================
