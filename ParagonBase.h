/**************************************************************************
 
 Paragon - BSOS code - by Mike from PinballHelp.com
 
 Switch, solenoid, light definitions
 
 Version 0.1.0 - 04/03/21
 
 
 

 
 
 */

// Solenoids

// original code started at 0, not  sure if we should start at 1?

#define SOL_OUTHOLE           6  //
#define SOL_KNOCKER           5  // 2 in Bally
#define SOL_SAUCER_TREASURE   7  // saucer behind inline drops 5x
#define SOL_SAUCER_PARAGON    9  // paragon saucer
#define SOL_SAUCER_GOLDEN     8  // golden cliffs saucer
#define SOL_LEFT_SLING        13
#define SOL_RIGHT_SLING       14
#define SOL_LEFT_BUMPER       10
#define SOL_RIGHT_BUMPER      11
#define SOL_BEAST_BUMPER       0 // bottom left pop bumper in beast area
#define SOL_CENTER_BUMPER     12
#define SOL_DROP_INLINE       1   // 13 on bally

// 2,3 and 4 don't respond

#define SOL_DROP_RIGHT        4  // NOT USED, 12 in Bally - uses U4/Q17 on Continuous solenoid line (BSOS_SetCoinLockout(true, 0x10); delay(125); BSOS_SetCoinLockout(false, 0x10);

//#define SOL_COIN_LOCKOUT      2 //4?
//#define SOL_FLIPPER_ENABLE    14  //6  // 3 4no

// Cabinet Switches
#define SW_CREDIT_RESET       5  //6
#define SW_TILT               6  //7
#define SW_OUTHOLE            7  //8
#define SW_COIN_1             8  //9
#define SW_COIN_2             9  //10
#define SW_COIN_3             10 //11
#define SW_SLAM               15 // 

// Playfield Switches

#define SW_DROP_INLINE_D      0   // 1000 + 3x bonus multiplier     CurrentDropTargetsValid && 0x40 (64)
#define SW_DROP_INLINE_C      1   // 1000 + 2x bonus multiplier     CurrentDropTargetsValid && 0x20 (32)
#define SW_DROP_INLINE_B      2   // 1000 + bonus advance           CurrentDropTargetsValid && 0x10 (16)
#define SW_DROP_INLINE_A      3   // 1000 + bonus advance           CurrentDropTargetsValid && 0x08 (8)
#define SW_SAUCER_TREASURE    30  // 5000 + 5x bonus multiplier, lites extra ball, then special - treasure chamber saucer (behind inline drops)

#define SW_DROP_TOP           18   // 500 points                                                     CurrentDropTargetsValid && 0x04
#define SW_DROP_MIDDLE        17   // 500 points                                                     CurrentDropTargetsValid && 0x02
#define SW_DROP_BOTTOM        16   // 500 points - all three down awards 10k, 15k, 20k, 25k, special CurrentDropTargetsValid && 0x01

#define SW_RIGHT_OUTLANE      21   // 1000 points
#define SW_RIGHT_INLANE       22   // 1000 points
#define SW_SAUCER_PARAGON     23   // paragon saucer - 1 bonus + light letter, spell paragon+hit=50k + special

#define SW_TOP_ROLLOVER       25   // top center star rollover (500 points + advance bonus)
#define SW_500_REBOUND        26   // 500 point rebound (both left and right)
#define SW_WATERFALL_ROLLOVER 27   // 1000 points + bonus advance, waterfall (right squigly star rollover)  all 3-bank drops down increase value from 1k, to 5k, 10k, special, then 10k - resets on new ball
#define SW_BOTTOM_STANDUP     28   // 10 points + advance bonus, lower center target
#define SW_TOP_STANDUP        29   // 10 points + advance bonus, top center standup target 

#define SW_SAUCER_GOLDEN      31   // golden cliffs saucer (increasing award from 2k+ 2k each time)
#define SW_SPINNER            32   // spinner (100 points, when hit 5x advances bonus)
#define SW_STAR_ROLLOVER      33   // 50 points, top right, upper right star rollover, golden cliffs and drop target rebound switch (advance bonus?)

#define SW_RIGHT_SLING        34   // 500 points - right slingshot
#define SW_LEFT_SLING         35   // 500 points - left slingshot
#define SW_BEAST_BUMPER       36   // 100 points - beast's lair pop bumper lower left
#define SW_CENTER_BUMPER      37   // 100 points
#define SW_RIGHT_BUMPER       38   // 100 points
#define SW_LEFT_BUMPER        39   // 100 points

// Lamp Definitions

#define L_1K_BONUS            0
#define L_2K_BONUS            1
#define L_3K_BONUS            2
#define L_4K_BONUS            3
#define L_5K_BONUS            4
#define L_6K_BONUS            5
#define L_7K_BONUS            6
#define L_8K_BONUS            7
#define L_9K_BONUS            8
#define L_10K_BONUS           9
#define L_20K_BONUS           10  // SCORING NOTE: Bonus 20k if lit is held over on all balls, same for 30k and 40k
#define L_30K_BONUS           11
#define L_40K_BONUS           12

#define L_10K_DROPS           13
#define L_15K_DROPS           14
#define L_20K_DROPS           15
#define L_25K_DROPS           16
#define L_SPECIAL_DROPS       39

#define L_2X_BONUS            20
#define L_3X_BONUS            21
#define L_5X_BONUS            22

#define L_SAUCER_P            24
#define L_SAUCER_AL           25
#define L_SAUCER_R            26
#define L_SAUCER_AR           27
#define L_SAUCER_G            28
#define L_SAUCER_O            29
#define L_SAUCER_N            30

#define L_SAUCER_SPECIAL      18	// upper right paragon saucer special

#define L_CENTER_P            32
#define L_CENTER_AL           33
#define L_CENTER_R            34
#define L_CENTER_AR           35
#define L_CENTER_G            36
#define L_CENTER_O            37
#define L_CENTER_N            38

#define L_2K_GOLDEN           44
#define L_4K_GOLDEN           45
#define L_6K_GOLDEN           46
#define L_8K_GOLDEN           47
#define L_10K_GOLDEN          58
#define L_20K_GOLDEN          59

#define L_5K_WATER            57
#define L_10K_WATER           56    // or 43 - untested
#define L_WATER_SPECIAL       17

#define L_TREASURE_5X         19          
#define L_TREASURE_SPECIAL    31
#define L_TREASURE_EB         23

#define L_SHOOT_AGAIN         42

#define L_SPINNER_1           52 // 2nd from bottom (bottom one is GI/always on)
#define L_SPINNER_2           53
#define L_SPINNER_3           54
#define L_SPINNER_4           55 // top most light in advance bonus spinner

#define L_CREDIT              43 // untested
 
#define L_BB_MATCH            41
#define L_BB_TILT             51
#define L_BB_GAME_OVER        50
#define L_BB_HIGH_SCORE       49
#define L_BB_BALL_IN_PLAY     48
#define L_BB_SHOOT_AGAIN      40

// define renames to match BSOS standards

#define MATCH                   L_BB_MATCH
#define SAME_PLAYER             L_BB_SHOOT_AGAIN
#define EXTRA_BALL              L_SHOOT_AGAIN
#define BALL_IN_PLAY            L_BB_BALL_IN_PLAY
#define HIGH_SCORE              L_BB_HIGH_SCORE
#define GAME_OVER               L_BB_GAME_OVER
#define TILT                    L_BB_TILT
#define PLAYER_1_UP             0
#define PLAYER_2_UP             0
#define PLAYER_3_UP             0
#define PLAYER_4_UP             0


// SWITCHES_WITH_TRIGGERS are for switches that will automatically
// activate a solenoid (like in the case of a chime that rings on a rollover)
// but SWITCHES_WITH_TRIGGERS are fully debounced before being activated
#define NUM_SWITCHES_WITH_TRIGGERS         6    //

// PRIORITY_SWITCHES_WITH_TRIGGERS are switches that trigger immediately
// (like for pop bumpers or slings) - they are not debounced completely
#define NUM_PRIORITY_SWITCHES_WITH_TRIGGERS 6

// Define automatic solenoid triggers (switch, solenoid, number of 1/120ths of a second to fire)
struct PlayfieldAndCabinetSwitch TriggeredSwitches[] = {
  { SW_BEAST_BUMPER, SOL_BEAST_BUMPER, 4 },
  { SW_CENTER_BUMPER, SOL_CENTER_BUMPER, 4 },
  { SW_RIGHT_BUMPER, SOL_RIGHT_BUMPER, 4 },
  { SW_LEFT_BUMPER, SOL_LEFT_BUMPER, 4 },
  { SW_LEFT_SLING, SOL_LEFT_SLING, 4 },
  { SW_RIGHT_SLING, SOL_RIGHT_SLING, 4 },
};

// ---------------------- sound effects

#define SFX_2X                       500
#define SFXC_2X                      3

#define SFX_3X                       510
#define SFXC_3X                      3

#define SFX_5X                       515  // 5x +
#define SFXC_5X                      2

#define SFX_GC2K                       520   // golden cliffs awards - sound address SFX_2K+(value*5)
#define SFXC_GC2K                      3
#define SFX_GC4K                       525
#define SFXC_GC4K                      3
#define SFX_GC6K                     530
#define SFXC_GC6K                    1
#define SFX_GC8K                     535
#define SFXC_GC8K                    1
#define SFX_GC10K                    540
#define SFXC_GC10K                   2
#define SFX_GC12K                    545
#define SFXC_GC12K                   1
#define SFX_GC14K                    550
#define SFXC_GC14K                   2
#define SFX_GC16K                    555
#define SFXC_GC16K                   1
#define SFX_GC18K                    560
#define SFXC_GC18K                   1
#define SFX_GC20K                    565
#define SFXC_GC20K                   1

byte GCcounts[10]={SFXC_GC2K,SFXC_GC4K,SFXC_GC6K,SFXC_GC8K,SFXC_GC10K,SFXC_GC12K,SFXC_GC14K,SFXC_GC16K,SFXC_GC18K,SFXC_GC20K};   // array to hold counts for random sound

// bonus held amounts

#define SFX_20K_BONUS                570
#define SFXC_20K_BONUS               1
#define SFX_30K_BONUS                575
#define SFXC_30K_BONUS               1
#define SFX_40K_BONUS                580
#define SFXC_40K_BONUS               1

#define SFX_BONUS_HELD               585  // end of ball bonus held
#define SFXC_BONUS_HELD              4

#define SFX_BONUS_BIG                590  // bonus > 40k
#define SFXC_BONUS_BIG               3

#define SFX_P                        595  // P-A-R-A-G-O-N  Letter lites +
#define SFX_A                        596
#define SFX_R                        597
#define SFX_A                        598
#define SFX_G                        599
#define SFX_O                        600
#define SFX_N                        601

#define SFX_START_BALL1              20   // game start player ball 1
#define SFXC_START_BALL1             15

#define SFX_PARAGON                  610  // paragon lit
#define SFXC_PARAGON                 2

#define SFX_BALLSAVED                40   // ball save activated
#define SFXC_BALLSAVED               2

#define SFX_POP_BEAST                440  // beast pop +
#define SFXC_POP_BEAST               6

#define SFX_INLINES                  460
#define SFXC_INLINES                 7

#define SFX_SAUCER_TREASURE          470
#define SFXC_SAUCER_TREASURE         1

#define SFX_EXTRABALL                200 // +
#define SFXC_EXTRABALL               3

#define SFX_SAUCER_GOLDEN            390
#define SFXC_SAUCER_GOLDEN           1

#define SFX_COINDROP                 10
#define SFXC_COINDROP                2

#define SFX_HUNTSUCCESS              620
#define SFXC_HUNTSUCCESS             5

#define SFX_SLING_RIGHT              420
#define SFXC_SLING_RIGHT             1
#define SFX_SLING_LEFT               430
#define SFXC_SLING_LEFT              1

#define SFX_SKILLSHOT                630  // generic skill shot
#define SFXC_SKILLSHOT               3
#define SFX_SS_SPECIAL               640  // skill shot special (or 25k in tourney mode)
#define SFXC_SS_SPECIAL              1
#define SFX_SS_GOLDEN                645  // skill shot golden cliffs 20k + 2k
#define SFXC_SS_GOLDEN               1

#define SFX_SHOOT_AGAIN              110  // same player shoot again
#define SFXC_SHOOT_AGAIN             1

#define SFX_SPECIAL                  210 // +
#define SFXC_SPECIAL                 3

#define SFX_TILT                     230  // tilt out 
#define SFXC_TILT                    2

#define SFX_POP_LEFT                 475
#define SFXC_POP_LEFT                4

#define SFX_POP_RIGHT                455
#define SFXC_POP_RIGHT               4

#define SFX_POP_CENTER               450
#define SFXC_POP_CENTER              4

#define SFX_STANDUP_TOP              380
#define SFXC_STANDUP_TOP             1

#define SFX_STANDUP_BOTTOM           360
#define SFXC_STANDUP_BOTTOM          2

#define SFX_TOP_REBOUND              350  // switch upper left rebound
#define SFXC_TOP_REBOUND             1

#define SFX_WELCOME                  1    // when game is powered up
#define SFXC_WELCOME                 4

#define SFX_TIMEOUT                  650  // 10s left in beast mode
#define SFXC_TIMEOUT                 5

#define SFX_HUNTFAIL                 660  // when hunt times out
#define SFXC_HUNTFAIL                4

#define SFX_BEASTMOVE                670  // when beast shot moves
#define SFXC_BEASTMOVE               1

#define SFX_RIGHT_OUTLANE            310
#define SFXC_RIGHT_OUTLANE           1

/*
#define SFX_HUNTSTART
#define SFXC_HUNTSTART

#define SFX_HUNTSTUN
#define SFXC_HUNTSTUN


#define SFX_
#define SFXC_
#define SFX_
#define SFXC_

*/

// base defines

#define SFX_NONE                     0
#define SFXC_GAME_INTRO              3  // number of different sound effects in this category
#define SFX_GAME_INTRO               1  // intro when game boots
#define SFX_GAME_INTRO2              2
#define SFX_GAME_INTRO3              3

#define SFXC_ADD_CREDIT              3
#define SFX_ADD_CREDIT               10
#define SFX_ADD_CREDIT2              11
#define SFX_ADD_CREDIT3              12

#define SFX_START_GAME1              20
#define SFX_START_GAME2              21
#define SFX_START_GAME3              22
#define SFX_START_GAME4              23
#define SFX_START_GAME5              24
#define SFX_START_GAME6              26

#define SFX_ADD_PLAYER2              30
#define SFX_ADD_PLAYER3              31
#define SFX_ADD_PLAYER4              32

#define SFX_BEAST1                   40
#define SFX_BEAST2                   41
#define SFX_BEAST3                   42

#define SFX_SKILL1                   50 // skill shot awarded
#define SFX_SKILL2                   51

/*

#define SFX_
#define SFX_
#define SFX_
#define SFX_
#define SFX_
#define SFX_
#define SFX_
#define SFX_
#define SFX_

*/

// end: ParagonBase.h
