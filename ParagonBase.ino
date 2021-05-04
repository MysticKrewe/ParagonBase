/*

  Paragon 2021 - by Mike from PinballHelp.com

  code/bsos/work/ParagonBase.ino
  
  working code file
  
  0.0.1 - 4-7-21 - Slightly modified version of bsos/pinballbase.ino that compiles with Paragon definitions


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
boolean MatchFeature = true;
byte SpecialLightAward = 0;
boolean TournamentScoring = false;
boolean ResetScoresToClearVersion = false;
boolean ScrollingScores = true;
unsigned long LastSpinnerHitTime = 0;
unsigned long GameModeStartTime = 0;
unsigned long GameModeEndTime = 0;
unsigned long LastTiltWarningTime = 0;

byte GameMode = GAME_MODE_SKILL_SHOT;
byte MaxTiltWarnings = 2;
byte NumTiltWarnings = 0;

unsigned long CurrentTime = 0;
unsigned long BallTimeInTrough = 0;
unsigned long BallFirstSwitchHitTime = 0;

boolean BallSaveUsed = false;
byte BallSaveNumSeconds = 0;
byte BallsPerGame = 3;

boolean ExtraBallCollected = false;

// game specific

unsigned long DropTargetClearTime = 0;

unsigned long SaucerHitTime = 0;       // used for debouncing saucer hit
byte PSaucerValue = 0;                 // 1-8 p-a-r-a-g-o-n special
byte GoldenSaucerValue = 1;            // 1-10 (value * 2000= points)
byte WaterfallValue=0;                 // 0=1k 1=5k, 2=10k 3=special
boolean WaterfallSpecialAwarded=false; // per ball
byte RightDropsValue=1;                // 10k,15,20,25,special
byte SpinnerValue=1;                   // 1-6, when 6 add bonus, 100 per spin

// Written in by Mike - yy
byte CurrentDropTargetsValid = 0;         // bitmask showing which drop targets up right:b,m,t, inline 1-4 1-64 bits
byte DropSequence=0;                      // 1-3 # right drops hit in sequence

// values that carry over from ball-to-ball
unsigned int DropsRightDownScore[4];      // reward for all right drops down
byte BonusHeld[4];                        // bits: 1=20k, 2=30k, 4=40k

////////////////////////////////////////////////////////////////////////////
//
//  Machine/Hardware-specific code
//
////////////////////////////////////////////////////////////////////////////

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


// copied from Trident
// ----------------------------------------------------------------

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
  if ((GoldenSaucerValue>4) || (GoldenSaucerValue<10))
    { BSOS_SetLampState(L_10K_GOLDEN, 1); } else { BSOS_SetLampState(L_10K_GOLDEN, 0); }
  if (GoldenSaucerValue==10)
    { BSOS_SetLampState(L_20K_GOLDEN, 1); } else { BSOS_SetLampState(L_20K_GOLDEN, 0); }

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
void ShowBonusOnTree(byte bonus, byte dim=0) {
  if (bonus>MAX_DISPLAY_BONUS) bonus = MAX_DISPLAY_BONUS;
  
  byte cap = 10;

  for (byte turnOff=(bonus+1); turnOff<11; turnOff++) {
    BSOS_SetLampState(L_1K_BONUS + (turnOff-1), 0);
  }
  if (bonus==0) return;

  if (bonus>=cap) {
    while(bonus>=cap) {
      BSOS_SetLampState(L_1K_BONUS + (cap-1), 1, dim, 250); // I belive the 250 should sync with the bonus countdown time
      bonus -= cap;
      cap -= 1;
      if (cap==0) {
        bonus = 0;
        break;
      }
    }
    for (byte turnOff=(bonus+1); turnOff<(cap+1); turnOff++) {
      BSOS_SetLampState(L_1K_BONUS + (turnOff-1), 0);
    }
  }

  byte bottom; 
  for (bottom=1; bottom<bonus; bottom++){
    BSOS_SetLampState(L_1K_BONUS + (bottom-1), 0);
  }

  if (bottom<=cap) {
    BSOS_SetLampState(L_1K_BONUS + (bottom-1), 1, 0);
  }    
} // END: ShowBonusOnTree()

byte LastBonusShown = 0;
void ShowBonusLamps() {
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


// copied from Trident

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

void AddSpecialCredit() {
  // Adds special credit + audit, knocker
  // NOTE: update this to add x special parm?
  AddCredit(false, 1);
  BSOS_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime, true);
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
        case MACHINE_STATE_ADJUST_BALL_SAVE:
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


byte AttractLastHeadMode = 255;
byte AttractLastPlayfieldMode = 255;

int RunAttractMode(int curState, boolean curStateChanged) {

  int returnState = curState;

  // If this is the first time in the attract mode loop
  if (curStateChanged) {
    BSOS_DisableSolenoidStack();
    BSOS_TurnOffAllLamps();
    BSOS_SetDisableFlippers(true);
    if (DEBUG_MESSAGES) {
      Serial.write("Entering Attract Mode\n\r");
    }
    

//    AttractLastHeadMode = 0;
//    AttractLastPlayfieldMode = 0;
    
    for (int count=0; count<4; count++) {  // blank out displays?
      BSOS_SetDisplayBlank(count, 0x00);     
    }
    BSOS_SetDisplayCredits(Credits);
    BSOS_SetDisplayBallInPlay(0);
    AttractLastHeadMode = 255;
    AttractLastPlayfieldMode = 255;
  }

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

  if ((CurrentTime/10000)%3==0) {   // every 30s turn off all playfield lamps?
    if (AttractLastPlayfieldMode!=1) {
      BSOS_TurnOffAllLamps();
    }
    
    AttractLastPlayfieldMode = 1;
  } else { 
    if (AttractLastPlayfieldMode!=2) {
      BSOS_TurnOffAllLamps();
    }

    AttractLastPlayfieldMode = 2;
  }
  
  
  
  // ---------------------------------------
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
#ifdef DEBUG_MESSAGES
      char buf[128];
      sprintf(buf, "Switch 0x%02X (%d)\n", switchHit,switchHit);
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

  // check to see if all drops down
  if (BSOS_ReadSingleSwitchState(SW_DROP_BOTTOM) && BSOS_ReadSingleSwitchState(SW_DROP_MIDDLE) && BSOS_ReadSingleSwitchState(SW_DROP_TOP)) {
    
    if (DropSequence==3) { // were they done in squence?
//   PlaySoundEffect();
    } else { 
//   PlaySoundEffect();      
    }
    if (DropsRightDownScore[CurrentPlayer]==30000) { // award special
// award_special((DropSequence==3?2:1)); // yy    
    } else {
      CurrentPlayerCurrentScore+=(DropSequence==3?2:1)*DropsRightDownScore[CurrentPlayer];   
    }

//  DropTargetClearTime = CurrentTime;  // if we want to keep track of time
    DropsRightDownScore[CurrentPlayer]+=5000; // need to activate special & check boundaries
    if (DropsRightDownScore[CurrentPlayer]==35000) DropsRightDownScore[CurrentPlayer]=10000; // reset score tree after special
    // reset drops
    reset_3bank();    
    //CurrentDropTargetsValid = CurrentDropTargetsValid | 7; // turn on bits 1-3
    //DropSequence=0;  done in reset_3bank();

    // handle waterfall ladder
    if (WaterfallSpecialAwarded) { WaterfallValue=2; }
    else {     
      WaterfallValue++;
      if (WaterfallValue>3) WaterfallValue=3;
    }
  } // end: all drop targets down
  
} // end: HandleRightDropTargetHit()

//-----------------------------------------------------------------
void HandleGoldenSaucerHit() {

  CurrentPlayerCurrentScore+=GoldenSaucerValue*2000;
  GoldenSaucerValue++;
  if (GoldenSaucerValue>10) { GoldenSaucerValue=10; } 
 
  BSOS_PushToTimedSolenoidStack(SOL_SAUCER_GOLDEN, 5, CurrentTime + SAUCER_PARAGON_DURATION);  
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
    sprintf(buf, "DipBank 0 = 0x%02X\n", dipBank);
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
      CurrentScores[count] = 0;
      
      // game specific values that carry over from ball-to-ball only init here - xx
      
      DropsRightDownScore[count]=10000;  // reset right drops value tree
      BonusHeld[count]=0;                 // any 20k+ bonus 
          
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

    // Initialize game-specific start-of-ball lights & variables    
    DropTargetClearTime = 0;
    ExtraBallCollected = false;
    
    // reset ball specific ladders
    GoldenSaucerValue=1;
    WaterfallValue=0;
    RightDropsValue=1;
    WaterfallSpecialAwarded=false;  // reset each ball?
    SpinnerValue=1;
    
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

  // lamp functions
  ShowShootAgainLamp();
  ShowGoldenSaucerLamps();
  
  
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
          }
          BallTimeInTrough = CurrentTime;

          returnState = MACHINE_STATE_NORMAL_GAMEPLAY;          
        } else {
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
//====================================================
//====================================================
//====================================================

int RunGamePlayMode(int curState, boolean curStateChanged) {
  int returnState = curState;
  unsigned long scoreAtTop = CurrentScores[CurrentPlayer];
  // unsigned long scoreAtTop = CurrentPlayerCurrentScore;
  
  byte bonusAtTop = Bonus;

  unsigned long scoreMultiplier = 1;  // currently not implemented
  
  
  // Very first time into gameplay loop
  if (curState==MACHINE_STATE_INIT_GAMEPLAY) {
    returnState = InitGamePlay(curStateChanged);    
  } else if (curState==MACHINE_STATE_INIT_NEW_BALL) {
    returnState = InitNewBall(curStateChanged, CurrentPlayer, CurrentBallInPlay);
  } else if (curState==MACHINE_STATE_NORMAL_GAMEPLAY) {
    returnState = NormalGamePlay();
  } else if (curState==MACHINE_STATE_COUNTDOWN_BONUS) {

//    returnState = CountdownBonus(curStateChanged);
    ShowPlayerScores(CurrentPlayer, (BallFirstSwitchHitTime==0)?true:false, (BallFirstSwitchHitTime>0 && ((CurrentTime-LastTimeScoreChanged)>2000))?true:false);

    returnState = MACHINE_STATE_BALL_OVER; // disable this when countdown enabled
    
  } else if (curState==MACHINE_STATE_BALL_OVER) {    


if (DEBUG_MESSAGES) { 
      char buf[32];
      sprintf(buf, " Ball %d Over P%d\n\r",CurrentBallInPlay,CurrentPlayer);
      Serial.write(buf);
}  


  
    // copied from Trident2020
    CurrentScores[ CurrentPlayer] = CurrentPlayerCurrentScore;
    //
  
    if (SamePlayerShootsAgain) {
      returnState = MACHINE_STATE_INIT_NEW_BALL;
    } else {
      CurrentPlayer+=1;
      if (CurrentPlayer>=CurrentNumPlayers) {
        CurrentPlayer = 0;
        CurrentBallInPlay+=1;
      }

// new ball setups
      // Reset score at top since player changed
      CurrentPlayerCurrentScore = CurrentScores[CurrentPlayer];
      CurrentStandupsHit = StandupsHit[CurrentPlayer];
      scoreAtTop = CurrentPlayerCurrentScore;

      
      if (CurrentBallInPlay>BallsPerGame) {
//        CheckHighScores();
//        PlaySoundEffect(SOUND_EFFECT_GAME_OVER);
//        SetPlayerLamps(0);
        for (int count=0; count<CurrentNumPlayers; count++) {
          BSOS_SetDisplay(count, CurrentScores[count], true, 2);
        }

        returnState = MACHINE_STATE_MATCH_MODE;
      }
      else returnState = MACHINE_STATE_INIT_NEW_BALL;
    }    
  } else if (curState==MACHINE_STATE_MATCH_MODE) {
    returnState = MACHINE_STATE_GAME_OVER;
  } else {
    returnState = MACHINE_STATE_ATTRACT;
  }

  byte switchHit;
  while ( (switchHit=BSOS_PullFirstFromSwitchStack())!=SWITCH_STACK_EMPTY ) {

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
      char buf[32];
      sprintf(buf, "Right Drop (%d) [%d]\n\r", switchHit,CurrentDropTargetsValid);
      Serial.write(buf);
} */    
          HandleRightDropTargetHit(switchHit,scoreMultiplier);
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;
          break;      
          
        // Standup targets
        case SW_BOTTOM_STANDUP:
        case SW_TOP_STANDUP:
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
          if (SaucerHitTime==0 || (CurrentTime-SaucerHitTime)>SAUCER_DEBOUNCE_TIME) {
            SaucerHitTime = CurrentTime;
 //           ShowSaucerHit = PSaucerValue; // 1-7 p-a-r-a-g-o-n
            AddToBonus(1);
            // paragon_award(PSaucerValue);  // 1-8 p-a-r-a-g-o-n special
            BSOS_PushToTimedSolenoidStack(SOL_SAUCER_PARAGON, 5, CurrentTime + SAUCER_PARAGON_DURATION);             
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
              AddSpecialCredit();
              WaterfallSpecialAwarded=true;
              WaterfallValue=2;
              break;
          }        
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;        
          break;
          
        case SW_SPINNER:  // 100+ every 5 hits add bonus
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
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;        
          break;          
          
        case SW_BEAST_BUMPER:
        case SW_CENTER_BUMPER:
        case SW_RIGHT_BUMPER:
        case SW_LEFT_BUMPER:         
          CurrentPlayerCurrentScore+=100;
          if (BallFirstSwitchHitTime == 0) BallFirstSwitchHitTime = CurrentTime;        
          break;          
           
          
          
    }
  }
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
