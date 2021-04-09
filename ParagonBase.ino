/*

  Paragon 2021 - by Mike from PinballHelp.com
  
  working code file
  
  0.0.1 - 4-7-21 - Slightly modified version of bsos/pinballbase.ino that compiles with Paragon definitions
  
  

*/
#include "BallySternOS.h"
#include "ParagonBase.h"
#include "SelfTestAndAudit.h"
#include "BSOS_Config.h"
#include <EEPROM.h>          // needed for EEPROM.read() etc.

#define MAJOR_VERSION  2021  // update TRIDENT2020_MAJOR_VERSION references to just this
#define MINOR_VERSION  1

#define DEBUG_MESSAGES  1

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

byte DropsHit = 0;              // holds mask value indicating which of drops have been hit

// mode times & game limits

#define SKILL_SHOT_DURATION             15

#define MAX_DISPLAY_BONUS               100 // for trident: 55
#define TILT_WARNING_DEBOUNCE_TIME      1000


// ----------------------------------------------------------------

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
byte StandupsHit[4];
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

// ----------------------------------------------------------------

// copied from Trident


byte ReadSetting(byte setting, byte defaultValue) {
  byte value = EEPROM.read(setting);
  if (value == 0xFF) {
    EEPROM.write(setting, defaultValue);
    return defaultValue;
  }
  return value;
}

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

} // END: setup()

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

////////////////////////////////////////////////////////////////////////////
//
//  Machine State Helper functions
//
////////////////////////////////////////////////////////////////////////////

/* base version
void AddCredit() {
  if (Credits<MaximumCredits) {
    Credits++;
    BSOS_WriteByteToEEProm(BSOS_CREDITS_EEPROM_BYTE, Credits);
//    PlaySoundEffect(SOUND_EFFECT_ADD_CREDIT);
  } else {
  }
  BSOS_SetCoinLockout((Credits<MaximumCredits)?false:true);
}
*/
// Trident version
void AddCredit(boolean playSound = false, byte numToAdd = 1) {
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
}

void AddSpecialCredit() {
  AddCredit(false, 1);
  BSOS_PushToTimedSolenoidStack(SOL_KNOCKER, 3, CurrentTime, true);
  BSOS_WriteULToEEProm(BSOS_TOTAL_REPLAYS_EEPROM_START_BYTE, BSOS_ReadULFromEEProm(BSOS_TOTAL_REPLAYS_EEPROM_START_BYTE) + 1);  
}

boolean AddPlayer() {

  if (Credits<1 && !FreePlayMode) return false;
  if (CurrentNumPlayers>=4) return false;

  CurrentNumPlayers += 1;
  BSOS_SetDisplay(CurrentNumPlayers-1, 0);
  BSOS_SetDisplayBlank(CurrentNumPlayers-1, 0x30);

  if (!FreePlayMode) {
    Credits -= 1;
    BSOS_WriteByteToEEProm(BSOS_CREDITS_EEPROM_BYTE, Credits);
  }

  BSOS_SetDisplayCredits(Credits);

  return true;
}


int InitNewBall(bool curStateChanged, byte playerNum, int ballNum) {  

  if (curStateChanged) {
    BallFirstSwitchHitTime = 0;
    SamePlayerShootsAgain = false;

    BSOS_SetDisableFlippers(false);
    BSOS_EnableSolenoidStack(); 
    BSOS_SetDisplayCredits(Credits, true);
//    SetPlayerLamps(playerNum+1, 500);
    
    if (BSOS_ReadSingleSwitchState(SW_OUTHOLE)) {
      BSOS_PushToTimedSolenoidStack(SOL_OUTHOLE, 4, CurrentTime + 100);
    }

    for (int count=0; count<CurrentNumPlayers; count++) {
      BSOS_SetDisplay(count, CurrentScores[count], true, 2);
    }

    BSOS_SetDisplayBallInPlay(ballNum);
    BSOS_SetLampState(BALL_IN_PLAY, 1);
    BSOS_SetLampState(TILT, 0);

    if (BallSaveNumSeconds>0) {
      BSOS_SetLampState(SAME_PLAYER, 1, 0, 500);
//      BSOS_SetLampState(HEAD_SAME_PLAYER, 1, 0, 500);
    }
  }
  
  // We should only consider the ball initialized when 
  // the ball is no longer triggering the SW_OUTHOLE
  if (BSOS_ReadSingleSwitchState(SW_OUTHOLE)) {
    return MACHINE_STATE_INIT_NEW_BALL;
  } else {
    return MACHINE_STATE_NORMAL_GAMEPLAY;
  }
  
}



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
    for (int count=0; count<4; count++) {
      BSOS_SetDisplayBlank(count, 0x00);     
    }
    BSOS_SetDisplayCredits(Credits);
    BSOS_SetDisplayBallInPlay(0);
    AttractLastHeadMode = 255;
    AttractLastPlayfieldMode = 255;
  }

  // Alternate displays between high score and blank
  if ((CurrentTime/6000)%2==0) {

    if (AttractLastHeadMode!=1) {
      BSOS_SetLampState(HIGH_SCORE, 1, 0, 250);
      BSOS_SetLampState(GAME_OVER, 0);
//      SetPlayerLamps(0);
  
      for (int count=0; count<4; count++) {
        BSOS_SetDisplay(count, HighScore, true, 2);
      }
      BSOS_SetDisplayCredits(Credits, true);
      BSOS_SetDisplayBallInPlay(0, true);
    }
    AttractLastHeadMode = 1;
    
  } else {
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

  if ((CurrentTime/10000)%3==0) {  
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

  byte switchHit;
  while ( (switchHit=BSOS_PullFirstFromSwitchStack())!=SWITCH_STACK_EMPTY ) {
    if (switchHit==SW_CREDIT_RESET) {
      if (AddPlayer()) returnState = MACHINE_STATE_INIT_GAMEPLAY;
    } else if (switchHit==SW_COIN_1 || switchHit==SW_COIN_2 || switchHit==SW_COIN_3) {
      AddCredit();
      BSOS_SetDisplayCredits(Credits, true);
    } else if (switchHit==SW_SELF_TEST_SWITCH && (CurrentTime-GetLastSelfTestChangedTime())>500) {
      returnState = MACHINE_STATE_TEST_LIGHTS;
      SetLastSelfTestChangedTime(CurrentTime);
    } else {
#ifdef DEBUG_MESSAGES
      char buf[128];
      sprintf(buf, "Switch 0x%02X\n", switchHit);
      Serial.write(buf);
#endif      
    }
  }

  return returnState;
}



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
            BSOS_SetLampState(SAME_PLAYER, 0);
//            BSOS_SetLampState(HEAD_SAME_PLAYER, 0);
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


unsigned long InitGameStartTime = 0;

int InitGamePlay(boolean curStateChanged) {
  int returnState = MACHINE_STATE_INIT_GAMEPLAY;

  if (curStateChanged) {
    InitGameStartTime = CurrentTime;
    BSOS_SetCoinLockout((Credits>=MaximumCredits)?true:false);
    BSOS_SetDisableFlippers(true);
    BSOS_DisableSolenoidStack();
    BSOS_TurnOffAllLamps();
    BSOS_SetDisplayBallInPlay(1);

    // Set up general game variables
    CurrentPlayer = 0;
    CurrentBallInPlay = 1;
    for (int count=0; count<4; count++) CurrentScores[count] = 0;

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
  }

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

int RunGamePlayMode(int curState, boolean curStateChanged) {
  int returnState = curState;
  unsigned long scoreAtTop = CurrentScores[CurrentPlayer];
  
  // Very first time into gameplay loop
  if (curState==MACHINE_STATE_INIT_GAMEPLAY) {
    returnState = InitGamePlay(curStateChanged);    
  } else if (curState==MACHINE_STATE_INIT_NEW_BALL) {
    returnState = InitNewBall(curStateChanged, CurrentPlayer, CurrentBallInPlay);
  } else if (curState==MACHINE_STATE_NORMAL_GAMEPLAY) {
    returnState = NormalGamePlay();
  } else if (curState==MACHINE_STATE_COUNTDOWN_BONUS) {
//    returnState = CountdownBonus(curStateChanged);
    returnState = MACHINE_STATE_BALL_OVER;
  } else if (curState==MACHINE_STATE_BALL_OVER) {    
    if (SamePlayerShootsAgain) {
      returnState = MACHINE_STATE_INIT_NEW_BALL;
    } else {
      CurrentPlayer+=1;
      if (CurrentPlayer>=CurrentNumPlayers) {
        CurrentPlayer = 0;
        CurrentBallInPlay+=1;
      }
        
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
    }
  }

  if (scoreAtTop!=CurrentScores[CurrentPlayer]) {
    Serial.write("Score changed\n");
  }
  return returnState;
}


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
