#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include "gba.h"
#include "font.h"
#include "background.h"
#include "sprites.h"
#include "playerskins.h"
#include "arenas.h"
#include "entity.h"

void enterMenu(int menu);

std::string toString(int number);
void drawText(int x, int y, const std::string& string);
void drawTextCentered(int y, const std::string& string);
void clearText();

const char* weaponName(int weapon);
void drawMenuOption(int y, const std::string& label, bool selected);

void applySkin(int skin);
int skinTileBase(int skin);
void loadArena(int index);
void applyArenaPalette(int index);
bool arenaUnlocked(int index);
void buildBlastSprite();
void buildFlameSprites();
void buildAngrySprites();
int monsterTile(Monster& monster);
void applyShake();
void countKill(Monster& monster);
bool pauseGame();
int weaponTile(int weapon);
void updateExplosion();

void startBenchmark();
void finishBenchmark();
void writeBenchResults();
int benchPercentile(int percent);
int readKeys();
bool skinUnlocked(int skin);
std::string skinRequirement(int skin);
void loadProgress();
void saveProgress();

void shoot();
void explode(int x, int y, int radius);
void updateGrenade(Bullet& grenade);
int spawnBullet(int type);
void spawnMonster();
bool tryMove(int x, int y, int testX, int testY, Entity& entity);
int checkMapCollision(int x, int y, int testX, int testY);
bool checkEntityCollision(Entity e1, Entity e2);

//The 128 hardware objects, carved up. Everything is derived from the pool
//sizes, so raising MAX_BULLETS moves the rest along rather than quietly
//overwriting the text. Menus draw no game objects, so text starts right after
//the player there and the whole run of slots is available for the credits.

Player player(0,0);

std::vector<Monster> enemies;
const int MAX_ENEMIES = 20;

std::vector<Bullet> bullets;
const int MAX_BULLETS = 28;

const int OBJ_WEAPON = 0;
const int OBJ_CRATE = 1;
const int OBJ_PLAYER = 2;
const int OBJ_ENEMY_BASE = 3;
const int OBJ_BULLET_BASE = OBJ_ENEMY_BASE + MAX_ENEMIES;
const int OBJ_BLAST = OBJ_BULLET_BASE + MAX_BULLETS;
const int OBJ_TEXT_GAME = OBJ_BLAST + 1;
const int OBJ_TEXT_MENU = OBJ_ENEMY_BASE;

int textBase = OBJ_TEXT_MENU;

//Rockets and mines destroy everything within this many pixels of where they
//went off, measured from the centre of each monster; a grenade's blast is a
//little tighter. The blast is drawn as a fireball that swells to that size,
//holds, then collapses; the kill follows the picture outwards so nothing dies
//before the disc reaches it.
const int EXPLOSION_RADIUS = 44;
const int GRENADE_RADIUS   = 32;
const int EXPLOSION_GROW   = 8;		//frames spent swelling
const int EXPLOSION_HOLD   = 20;	//about a third of a second at full size
const int EXPLOSION_FADE   = 8;		//frames spent collapsing again
const int EXPLOSION_FRAMES = EXPLOSION_GROW + EXPLOSION_HOLD + EXPLOSION_FADE;
const int EXPLOSION_DEBRIS = 10;

//The fireball is a 64x64 sprite magnified by an affine transform, so a single
//object covers every size it needs to be. It is drawn in three bands - core,
//middle and rim - each its own palette entry, and those three entries are
//recoloured every frame to take it from a white flash through yellow, orange
//and red down to smoke. Only one blast is ever alive, so they can be shared.
const int BLAST_TILE = 304;
const int BLAST_CORE = 40;
const int BLAST_MID  = 41;
const int BLAST_RIM  = 42;

//Explosions shake everything but the score for a moment
const int SHAKE_FRAMES = 14;
int shakeTimer;

//Monsters that walk into the fire come back in at the top, angry: red, twice
//as fast, and angry for good. The red frames are copies of the green ones
//with the three greens swapped for reds, made at start-up into free tiles.
//Large frames are 16x16, so in the 2D sprite layout their bottom halves sit
//one 16-tile row below the top halves, same as in the original sheet.
const int ANGRY_LARGE_TILE = 432;	//copies of 32-39, and 48-55 below them
const int ANGRY_SMALL_TILE = 440;	//copies of 16-21

//The flamethrower isn't in the 2014 spritesheet, so its icon and three flame
//frames are drawn at start-up into the free tiles just past the skins, in
//sprite palette entries nothing else uses (the sheet stops at 20).
const int FLAME_TILE      = 290;	//three frames, big to small
const int FLAME_ICON_TILE = 293;
const int FLAME_FRAMES    = 3;
const int FLAME_YELLOW = 24;
const int FLAME_ORANGE = 25;
const int FLAME_RED    = 26;
const int GUN_DARK     = 27;
const int GUN_LIGHT    = 28;
const int FLAME_SMOKE  = 29;

int explosionTimer;
int explosionRadius;
int explosionX;
int explosionY;

Crate crate(0, 0, 0);

//A palette entry and what to put in it
struct PaletteTweak{
	unsigned char entry;
	unsigned short colour;
};

//An arena. The girder layer doubles as the collision map - checkMapCollision
//reads screenblock 30 directly - so a new arena is data rather than code, as
//long as it keeps to the tile conventions: 16-20 are the deadly fire, 21-25
//are passable, 0 is open air and everything else is solid.
struct Arena{
	const char* name;
	
	const unsigned short* girders;	//also the collision map
	const unsigned short* fence;
	const unsigned short* building;
	const unsigned short* clouds;
	
	unsigned char playerX, playerY;
	unsigned char monsterX, monsterY;
	
	const unsigned char* crateSpots;	//x,y pairs, in pixels
	int crateCount;
	
	const unsigned char* fireTiles;		//x,y pairs, in tiles, animated each frame
	int fireCount;
	
	//Recolouring the shared tileset is what makes the arenas look like
	//different places. Only the entries that differ from backgroundPal are
	//listed; the fire keeps entries 1, 8, 9 and 14, which nothing else uses,
	//so the flames stay orange whatever the walls are made of.
	const PaletteTweak* palette;
	int paletteCount;
	
	int unlockScore;	//needed in the arena before this one
};

//Pale stone under a night sky
const PaletteTweak templePalette[] = {
	{10, RGB(25,24,21)}, {11, RGB(28,27,24)}, {12, RGB( 9, 8, 8)},
	{13, RGB(18,17,15)}, {15, RGB( 7, 6, 6)}, {16, RGB(11,10, 8)},
	{17, RGB(21,19,16)}, {18, RGB(24,23,21)}, {19, RGB(27,26,24)},
	{20, RGB( 9, 8, 7)}, {21, RGB(17,15,12)},
	{ 5, RGB( 6, 6,10)}, { 6, RGB( 5, 5, 8)}, { 7, RGB( 9, 9,13)},
	{ 3, RGB(10, 9,12)}, { 4, RGB( 9, 8,11)},
};

//Cold steel, lit from nowhere in particular
const PaletteTweak siloPalette[] = {
	{10, RGB(16,18,20)}, {11, RGB(21,23,25)}, {12, RGB( 6, 7, 8)},
	{13, RGB(12,14,16)}, {15, RGB( 4, 5, 6)}, {16, RGB( 7, 8,10)},
	{17, RGB( 9,15,20)}, {18, RGB(18,20,22)}, {19, RGB(22,24,26)},
	{20, RGB( 5, 6, 8)}, {21, RGB(11,14,17)},
	{ 5, RGB( 6, 7, 8)}, { 6, RGB( 5, 6, 7)}, { 7, RGB( 8, 9,11)},
	{ 3, RGB( 7, 8,10)}, { 4, RGB( 6, 7, 9)},
};

//Crate spots, as x,y pairs. Every one is standing on something solid and clear
//of the fire; there are plenty so the next crate is rarely somewhere you have
//just been.
const unsigned char yardCrates[] = {
	 80, 32,   112, 32,   150, 32,
	 16, 64,    48, 64,   176, 64,   216, 64,
	 80,104,   115,104,   150,104,
	 16,136,    48,136,   184,136,   216,136,
	 80,144,   150,144,
};
const unsigned char yardFire[] = { 14,19,  15,19 };

const unsigned char siloCrates[] = {
	 16, 24,    40, 24,   192, 24,   216, 24,
	 80, 56,   112, 56,   152, 56,
	 16, 88,    40, 88,   192, 88,   216, 88,
	 72,120,   112,120,   152,120,
	 24,136,   208,136,
	 64,144,   160,144,
};

const unsigned char templeCrates[] = {
	 40, 32,    80, 32,   152, 32,   192, 32,
	 24, 64,   112, 64,   208, 64,
	 40, 96,    80, 96,   152, 96,   192, 96,
	 96,128,   128,128,
	 16,136,   208,136,
	 72,144,   160,144,
};

const Arena arenas[] = {
	{"CONSTRUCTION YARD",
	 girderMap, fenceBGMap, buildingBGMap, cloudBGMap,
	 116, 76,  116, 0,
	 yardCrates, sizeof(yardCrates)/2,
	 yardFire, sizeof(yardFire)/2,
	 0, 0,
	 0},
	
	{"ROCKET SILO",
	 siloGirderMap, blankMap, blankMap, cloudBGMap,
	 116, 56,  116, 0,
	 siloCrates, sizeof(siloCrates)/2,
	 yardFire, sizeof(yardFire)/2,
	 siloPalette, sizeof(siloPalette)/sizeof(siloPalette[0]),
	 10},
	
	{"MOON TEMPLE",
	 templeGirderMap, blankMap, buildingBGMap, cloudBGMap,
	 116, 56,  116, 0,
	 templeCrates, sizeof(templeCrates)/2,
	 yardFire, sizeof(yardFire)/2,
	 templePalette, sizeof(templePalette)/sizeof(templePalette[0]),
	 10},
};
const int NUM_ARENAS = sizeof(arenas) / sizeof(arenas[0]);

//How far from the player a new crate has to appear
const int CRATE_CLEARANCE = 56;

int currentArena;
int arenaBest[NUM_ARENAS];	//best score in each, which is what unlocks the next

int weapon;
const int NUM_WEAPONS = 11;
int score;
int highScore;
int lifetimeCrates;
int lifetimeDeaths;
int lifetimeKills;			//every monster, angry ones included
int lifetimeAngryKills;
int selectedSkin;

//Progress is only written to SRAM when it has actually changed, and never
//during play: emulators react to SRAM writes by flushing the save file, which
//stutters the frame. Nothing is lost by waiting - a run can only end in death.
bool progressDirty;

//Frame timing meter, toggled with SELECT. Timer 0 counts one tick per 64
//cycles, so a whole 280896-cycle frame is 4389 ticks. Measured from the start
//of VBlank to the end of the next frame's work, so it covers everything except
//the idle wait - 100% means the frame budget is exactly used up.
const int FRAME_TICKS = 4389;
const int TM_FREQ_64 = 1;
const int TM_ENABLE = 0x80;

bool showMeter = false;	//SELECT turns it on during play
int frameTicks;
int peakTicks;
int overruns;

//Benchmark: a fixed thirty second run with the player invincible and the
//controls driven from a seeded random stream, so the same work happens every
//time and two builds can be compared directly. Timers 2 and 3 cascade into a
//32-bit clock for the whole run, which is what gives a real frame rate rather
//than the 60 the vsync wait would always report.
//
//It deliberately measures a hard case rather than a typical one: the monster
//pool is kept full and the weapon is rotated, because left to itself a run
//collects no crates at all and never fires anything but the machine gun.
const int BENCH_FRAMES = 1800;

//Longer than the slowest reload, so that even the 60 frame weapons - and so
//the explosion path - actually get to fire while they are selected
const int BENCH_WEAPON_HOLD = 90;
const int BENCH_SPAWN_ODDS = 6;
const unsigned int BENCH_SEED = 20140506;
const int TM_FREQ_1024 = 3;
const int TM_CASCADE = 4;

//Frame times are binned rather than kept, which is enough for a percentile
//and costs a kilobyte: 512 bins of 16 ticks covers nearly two whole frames.
const int BENCH_BINS = 512;
const int BENCH_BIN_SHIFT = 4;

int benchFrames;		//frames left to run, 0 when not benchmarking
int benchTickSum;
int benchPeak;
int benchLow;
int benchOverruns;
int benchDeaths;
unsigned int benchCycles;	//whole run, in units of 1024 cycles
int benchHeldKeys;
int benchHoldTimer;
unsigned short benchHist[BENCH_BINS];

//What the run actually did, so a frame time can be read against the work that
//produced it - and so a change to, say, the bullet count can be seen landing
int benchShots;
int benchExplosions;
int benchPeakBullets;
int benchPeakEnemies;

//Carried across frames by the game loop. Global so that starting a game - and
//so a benchmark - resets them; otherwise a second run begins on a different
//animation phase and consumes the random stream differently to the first.
int cloudScroll;
int loopSlower;
bool shootCoolDown;
int reloadTimer;

//Name of the weapon from the crate just collected, shown for a short while
//at the place the crate was picked up from
std::string pickupText;
int pickupTimer;
int pickupX;
int pickupY;
const int PICKUP_SHOW_FRAMES = 90;

//A playable character. The player sprite is drawn from four palette entries
//that nothing else uses, so a skin is just those four colours - swapping them
//recolours every frame of the animation at once.
struct Skin{
	const char* name;
	uint16_t colours[4];	//palette entries 16, 17, 19 and 20
	int unlockScore;	//crates needed in a single game
	int unlockCrates;	//crates needed across every game, used instead when > 0
};

const Skin skins[] = {
	{"GUY",       {RGB(31,22,14), RGB(31,16, 5), RGB(27,11, 4), RGB(27, 8, 3)},   0,    0},
	{"ASTRONAUT", {RGB(31,31,31), RGB(25,26,29), RGB(17,18,23), RGB(11,11,16)},  10,    0},
	{"NINJA",     {RGB(14,15,21), RGB( 8, 9,14), RGB( 5, 5, 9), RGB( 3, 3, 6)},  20,    0},
	{"CROCODILE", {RGB(21,28,12), RGB(13,23, 6), RGB( 7,16, 3), RGB( 4,10, 2)},  25,    0},
	{"ROBOT",     {RGB(28,29,30), RGB(20,22,24), RGB(13,15,17), RGB( 8, 9,10)},  30,    0},
	{"PIKLUPU",   {RGB(31,23,28), RGB(29,13,22), RGB(21, 7,15), RGB(14, 4,10)},  35,    0},
	{"CHICKEN",   {RGB(31,30,24), RGB(31,26, 8), RGB(26,19, 3), RGB(18,12, 1)},  50,    0},
	{"VLAMBEER",  {RGB(31,29,16), RGB(30,23, 5), RGB(23,15, 2), RGB(16,10, 1)},   0, 1000},
};
const int NUM_SKINS = sizeof(skins) / sizeof(skins[0]);

//Skin 0 wears the original sprite tiles; the rest are loaded above the font,
//14 frames each. Keep the table above in step with tools/genskins.py.
const int SKIN_TILE_BASE = 192;

//Cartridge SRAM (8-bit access only) is used to keep progress between play
//sessions. The save starts with a tag, so that blank or unavailable SRAM
//reads back as "nothing saved yet" rather than as garbage.
#define SRAM_BASE ((volatile uint8_t *) 0x0E000000)

//Emulators and flash carts work out the save type by looking for this
//string in the ROM image, so it has to survive being otherwise unused.
const char saveTypeTag[] __attribute__((used)) = "SRAM_V113";

void init(){
	// Set display options.
	REG_DISPCNT = DCNT_MODE0 | DCNT_OBJ | DCNT_BG0 | DCNT_BG1 | DCNT_BG2 | DCNT_BG3;
	
	// Set background 0 options.
	REG_BG0CNT = BG_CBB(0) | BG_SBB(30) | BG_8BPP | BG_REG_32x32;
	REG_BG0HOFS = 0;
	REG_BG0VOFS = 0;
	// Set background 1 options.
	REG_BG1CNT = BG_CBB(0) | BG_SBB(29) | BG_8BPP | BG_REG_32x32;
	REG_BG1HOFS = 0;
	REG_BG1VOFS = 0;
	// Set background 2 options.
	REG_BG2CNT = BG_CBB(0) | BG_SBB(28) | BG_8BPP | BG_REG_32x32;
	REG_BG2HOFS = 0;
	REG_BG2VOFS = 0;
	// Set background 3 options.
	REG_BG3CNT = BG_CBB(0) | BG_SBB(27) | BG_8BPP | BG_REG_32x32;
	REG_BG3HOFS = 0;
	REG_BG3VOFS = 0;
	
	LoadTileData(0, 0, backgroundTiles, backgroundTilesLen);
	LoadPaletteBGData(0, menuBGPal, menuBGPalLen);
	
	LoadTileData(4, 0, spritesTiles, spritesTilesLen);
	LoadTileData(4, 64, font_bold, 8192);
	LoadTileData(4, SKIN_TILE_BASE, playerSkinTiles, playerSkinTilesLen);
	buildBlastSprite();
	buildFlameSprites();
	buildAngrySprites();
	LoadPaletteObjData(0, spritesPal, spritesPalLen);
	SetPaletteObj(30, RGB(26, 6, 4));	//angry monsters, for greens 6, 8 and 9
	SetPaletteObj(31, RGB(31,14, 8));
	SetPaletteObj(32, RGB(13, 2, 3));
	SetPaletteObj(FLAME_YELLOW, RGB(31,29,10));
	SetPaletteObj(FLAME_ORANGE, RGB(31,16, 3));
	SetPaletteObj(FLAME_RED,    RGB(24, 5, 2));
	SetPaletteObj(GUN_DARK,     RGB( 7, 7, 8));
	SetPaletteObj(GUN_LIGHT,    RGB(17,17,19));
	SetPaletteObj(FLAME_SMOKE,  RGB(12,11,11));
	
	loadProgress();
	loadArena(currentArena);
	applyArenaPalette(currentArena);
	applySkin(selectedSkin);
}

void gameInit(){
	shakeTimer = 0;
	loadArena(currentArena);
	applyArenaPalette(currentArena);
	
	(&player)->~Player();
    new (&player) Player(arenas[currentArena].playerX, arenas[currentArena].playerY);
	
	//Emptied rather than just marked dead: spawnBullet and spawnMonster take a
	//different path once a pool has been filled once, so leaving them at full
	//size would make a second run behave differently to the first
	enemies.clear();
	bullets.clear();
	
	(&crate)->~Crate();
    new (&crate) Crate(0,0,0);
	
	weapon = 1;
	score = 0;
	applySkin(selectedSkin);
	
	cloudScroll = 0;
	loopSlower = 0;
	shootCoolDown = true;
	reloadTimer = 0;
	
	peakTicks = 0;
	overruns = 0;
	explosionTimer = 0;
	pickupText = "";
	pickupTimer = 0;
	pickupX = 0;
	pickupY = 0;

	//Set-up Objects
	clearText();
	textBase = OBJ_TEXT_GAME;
	ClearObjects();
	
	//Weapon
	int shape = 0;
	if(weapon == 8)shape = 1;
	SetObject(0,
	          ATTR0_SHAPE(shape) | ATTR0_8BPP | ATTR0_REG | ATTR0_Y(player.getY()+1),
			  ATTR1_SIZE(0) | ATTR1_X(player.getX()+4),
			  ATTR2_ID8(weaponTile(weapon)));
	
	//Crate
	SetObject(1,
	          ATTR0_SHAPE(0) | ATTR0_8BPP | ATTR0_REG | ATTR0_Y(crate.getY()),
			  ATTR1_SIZE(0) | ATTR1_X(crate.getX()),
			  ATTR2_ID8(14));
	
	//Player
	SetObject(2,
	          ATTR0_SHAPE(0) | ATTR0_8BPP | ATTR0_REG | ATTR0_Y(player.getY()),
			  ATTR1_SIZE(0) | ATTR1_X(player.getX()),
			  ATTR2_ID8(skinTileBase(selectedSkin) + 1));
}

int main()
{
	init();
	
#ifdef BENCH_AUTORUN
	startBenchmark();
#else
	enterMenu(0);
#endif

	int meterCoolDown = 0;
	bool startWasDown = false;
	
	//Free-running timer for the frame meter
	REG_TM0D = 0;
	REG_TM0CNT = TM_ENABLE | TM_FREQ_64;
	
	//GAME LOOP
	while (true)
	{	
		if(meterCoolDown > 0)meterCoolDown--;
		if((REG_KEYINPUT & KEY_SELECT) == 0 && meterCoolDown == 0){
			showMeter = !showMeter;
			meterCoolDown = 30;
		}
		
		//START pauses, on a fresh press only, so holding it doesn't flicker
		//in and out. Never during the benchmark.
		bool startDown = (REG_KEYINPUT & KEY_START) == 0;
		if(startDown && !startWasDown && benchFrames <= 0){
			if(pauseGame()){
				//Quit from the pause menu
				saveProgress();
				enterMenu(0);
				startWasDown = true;
				continue;
			}
			startDown = true;
			
			//The pause shouldn't read as one enormous frame on the meter
			REG_TM0CNT = 0;
			REG_TM0D = 0;
			REG_TM0CNT = TM_ENABLE | TM_FREQ_64;
		}
		startWasDown = startDown;
		
		//INPUT
		int keys = readKeys();
		
		if((keys & KEY_RIGHT) == 0){
			tryMove(2,0,player.getWidth()-2,player.getHeight()/2,player);
			player.setRunning(true);
			player.setDir(true);
		}
		if((keys & KEY_LEFT) == 0){
			tryMove(-2,0,0,player.getHeight()/2,player);
			player.setRunning(true);
			player.setDir(false);
		}
		if((keys & KEY_UP) == 0 || (keys & KEY_B) == 0){
			if(player.getLanded()){
				player.jump();
				player.updateFrame();
			}
			player.setJumpHeight(player.getJumpHeight()+4);
		}
		if((keys & KEY_A) == 0){
			if(shootCoolDown){
				if(weapon == 8){
					if(player.getDir())tryMove(-2,0,0,player.getHeight()/2,player);
					else tryMove(2,0,player.getWidth()-2,player.getHeight()/2,player);
				}else if(weapon == 1){
					if(player.getDir())tryMove(-1,0,0,player.getHeight()/2,player);
					else tryMove(1,0,player.getWidth()-2,player.getHeight()/2,player);
				}
				shoot();
				shootCoolDown = false;
				reloadTimer = 0;
			}
		}
		
		//Player movement
		if(player.getJumping()){
			if(!tryMove(0,-3,player.getWidth()/2,0,player)){
				player.setJumping(false);
			}
		}else{
			if(!tryMove(0,2,player.getWidth()/2,player.getHeight(),player)){
				tryMove(0,1,player.getWidth()/2,player.getHeight(),player);
				player.setLanded(true);
				player.setJumping(false);
			}else{
				player.setLanded(false);
			}
		}

		//Monster Movement. Angry monsters take two steps a frame.
		for(int i = 0; i < enemies.size(); i++){
			if(!enemies.at(i).isDead()){
				int steps = enemies.at(i).getAngry() ? 2 : 1;
				for(int n = 0; n < steps && !enemies.at(i).isDead(); n++){
					if(enemies.at(i).getDir()){
						if(!tryMove(1,0,enemies.at(i).getWidth(),(enemies.at(i).getHeight()/2)+1,enemies.at(i))){
							enemies.at(i).setDir(false);
						}
					}else{
						if(!tryMove(-1,0,0,(enemies.at(i).getHeight()/2)+1,enemies.at(i))){
							enemies.at(i).setDir(true);
						}
					}
				}
				if(!enemies.at(i).isDead()){
					tryMove(0,2,enemies.at(i).getWidth()/2,enemies.at(i).getHeight(),enemies.at(i));
				}
				
				//Only the fire kills anything in here. Rather than dying, the
				//monster comes back in at the top, angry.
				if(enemies.at(i).isDead()){
					enemies.at(i).enrage(arenas[currentArena].monsterX, arenas[currentArena].monsterY);
					ObjBuffer[i+OBJ_ENEMY_BASE].attr2 = ATTR2_ID8(monsterTile(enemies.at(i)));
				}
				ObjBuffer[i+OBJ_ENEMY_BASE].attr0 &= ~(ATTR0_HIDE);
			}else{
				ObjBuffer[i+OBJ_ENEMY_BASE].attr0 |= ATTR0_HIDE;
			}
		}
		
		//Bullet movement
		for(int i = 0; i < bullets.size(); i++){
			if(!bullets.at(i).isDead()){
			
				int yMove = 0;
				if(rand() % 3 == 0)yMove = bullets.at(i).getLift(); 
				
				if(bullets.at(i).getType() == 4){
				
					tryMove(0,1,bullets.at(i).getWidth(),bullets.at(i).getHeight()+4,bullets.at(i));
					
				}else if(bullets.at(i).getType() == 6){
					if(player.getDir()){
						ObjBuffer[i+OBJ_BULLET_BASE].attr1 &= ~(ATTR1_HFLIP);
						bullets.at(i).move(player.getX()+8, player.getY());
					}else{
						ObjBuffer[i+OBJ_BULLET_BASE].attr1 |= ATTR1_HFLIP;
						bullets.at(i).move(player.getX()-8, player.getY());
					}	
					if(bullets.at(i).charge()){
						bullets.at(i).setDead(true);
						int number = 0;
						if(player.getDir())number = (SCREEN_WIDTH - player.getX())/8;
						else number = (0 + player.getX())/8;
	
						for(int i = 0; i < number; i++){
							if(spawnBullet(7) < 0)break;
						}
					}
				}else if(bullets.at(i).getType() == 5){
					updateGrenade(bullets.at(i));
				}else if(bullets.at(i).getType() == 10){
					//Flame - quick out of the nozzle, slowing to a lick as it
					//burns down, drifting a little and curling upwards at the
					//end. Scenery puts it out.
					Bullet& flame = bullets.at(i);
					int age = flame.getCharger();
					int speed = (age < 6) ? 3 : (age < 12) ? 2 : 1;
					int dy = 0;
					if((age % 2) == 0)dy = flame.getLift();
					if(age >= 10 && (age % 3) == 0)dy = -1;
					
					if(!tryMove(flame.getDir() ? speed : -speed, dy,
					            flame.getDir() ? flame.getWidth() : 0, flame.getHeight()/2 + 1, flame)){
						flame.setDead(true);
					}
					
					if(flame.charge())flame.setDead(true);
					
					int shown = (flame.getCharger() * FLAME_FRAMES) / 18;
					if(shown >= FLAME_FRAMES)shown = FLAME_FRAMES - 1;
					ObjBuffer[i+OBJ_BULLET_BASE].attr2 = ATTR2_ID8(FLAME_TILE + shown);
				}else if(bullets.at(i).getType() == 7){
					if(bullets.at(i).charge())bullets.at(i).setDead(true);
				}else if(bullets.at(i).getType() == 9){
					//Blast debris - thrown outwards, gone in a moment, and it
					//passes through scenery rather than piling up against it
					int step = 2;
					if(!bullets.at(i).getDir())step = -2;
					
					bullets.at(i).move(bullets.at(i).getX() + step,
					                   bullets.at(i).getY() + bullets.at(i).getLift());
					
					if(bullets.at(i).charge())bullets.at(i).setDead(true);
					
					//Burning bits, dwindling as they fly
					int shown = (bullets.at(i).getCharger() * FLAME_FRAMES) / 12;
					if(shown >= FLAME_FRAMES)shown = FLAME_FRAMES - 1;
					ObjBuffer[i+OBJ_BULLET_BASE].attr2 = ATTR2_ID8(FLAME_TILE + shown);
				}else if(bullets.at(i).getType() == 8){
					//Katana slash - held just in front of the player for a
					//moment rather than travelling anywhere. The blade is
					//drawn at the far end of the reach, and flips over half
					//way through so the swing reads as a cut.
					if(player.getDir())bullets.at(i).move(player.getX()+4, player.getY()-4);
					else bullets.at(i).move(player.getX()-12, player.getY()-4);
					
					if(bullets.at(i).getCharger() >= 4)ObjBuffer[i+OBJ_BULLET_BASE].attr1 |= ATTR1_VFLIP;
					else ObjBuffer[i+OBJ_BULLET_BASE].attr1 &= ~(ATTR1_VFLIP);
					if(player.getDir())ObjBuffer[i+OBJ_BULLET_BASE].attr1 &= ~(ATTR1_HFLIP);
					else ObjBuffer[i+OBJ_BULLET_BASE].attr1 |= ATTR1_HFLIP;
					
					if(bullets.at(i).charge())bullets.at(i).setDead(true);
				}else{
					if(bullets.at(i).getDir()){
						if(!tryMove(2,yMove,bullets.at(i).getWidth(), bullets.at(i).getHeight(), bullets.at(i))){	
							if(bullets.at(i).getType() == 3 && !bullets.at(i).getBounce()){
								bullets.at(i).setBounce(true);
								bullets.at(i).setDir(false);
							}else{
								bullets.at(i).setDead(true);
								if(bullets.at(i).getType() == 0){
									explode(bullets.at(i).getX(), bullets.at(i).getY(), EXPLOSION_RADIUS);
								}
							}
						}
					}else{ 
						if(!tryMove(-2,yMove,bullets.at(i).getWidth(), bullets.at(i).getHeight(), bullets.at(i))){
							if(bullets.at(i).getType() == 3 && !bullets.at(i).getBounce()){
								bullets.at(i).setBounce(true);
								bullets.at(i).setDir(true);
							}else{
								bullets.at(i).setDead(true);
								if(bullets.at(i).getType() == 0){
									explode(bullets.at(i).getX(), bullets.at(i).getY(), EXPLOSION_RADIUS);
								}
							}
						}
					}
				}
				ObjBuffer[i+OBJ_BULLET_BASE].attr0 &= ~(ATTR0_HIDE);
			}else{
				ObjBuffer[i+OBJ_BULLET_BASE].attr0 |= ATTR0_HIDE;
			}
		}
		
		//Weapons are settled before the player is, so a monster cut down or
		//blown up this frame can't still take the player with it
		for(int i = 0; i < enemies.size(); i++){
			if(!enemies.at(i).isDead()){
				for(int k = 0; k < bullets.size(); k++){
					if(!bullets.at(k).isDead() && bullets.at(k).getType() != 9){
						if(checkEntityCollision(bullets.at(k), enemies.at(i))){
							//A katana swing lands once on each monster it
							//reaches, however long it stays in contact, and
							//spins a large monster about
							if(bullets.at(k).getType() == 10){
								//A flame burns through, scorching each
								//monster it passes over once
								if(bullets.at(k).strike(i)){
									enemies.at(i).hurt(bullets.at(k).getDamage());
								}
								continue;
							}
							if(bullets.at(k).getType() == 8){
								if(!bullets.at(k).strike(i))continue;
								enemies.at(i).hurt(bullets.at(k).getDamage());
								if(!enemies.at(i).getSize()){
									enemies.at(i).setDir(!enemies.at(i).getDir());
								}
								break;
							}
							
							enemies.at(i).hurt(bullets.at(k).getDamage());
							
							if(bullets.at(k).getType() != 3
							&& bullets.at(k).getType() != 6
							&& bullets.at(k).getType() != 7
							&& bullets.at(k).getType() != 8){
								bullets.at(k).setDead(true);
								if(bullets.at(k).getType() == 0
								|| bullets.at(k).getType() == 4){
									explode(bullets.at(k).getX(), bullets.at(k).getY(), EXPLOSION_RADIUS);
								}else if(bullets.at(k).getType() == 5){
									explode(bullets.at(k).getX()+2, bullets.at(k).getY()+2, GRENADE_RADIUS);
								}
							}
							break;
						}
					}
				}
				
				if(enemies.at(i).isDead()){
					countKill(enemies.at(i));
				}else if(checkEntityCollision(player, enemies.at(i))){
					player.setDead(true);
				}
			}
		}
		
		if(checkEntityCollision(player, crate)){
			crate.setDead(true);
			weapon = crate.getWeapon();
			score++;
			lifetimeCrates++;
			if(score > highScore)highScore = score;
			if(score > arenaBest[currentArena])arenaBest[currentArena] = score;
			progressDirty = true;
			pickupText = weaponName(weapon);
			pickupTimer = PICKUP_SHOW_FRAMES;
			//Remember where the crate was, before it is moved somewhere else
			pickupX = crate.getX();
			pickupY = crate.getY();
		}
		
		if(crate.isDead()){
			//Never where it just was, and never on top of or right next to
			//the player, or crates get picked up two and three at a time.
			//A few tries is plenty with this many spots; if they all miss,
			//the last one is used anyway.
			const Arena& arena = arenas[currentArena];
			int spot = 0;
			for(int tries = 0; tries < 8; tries++){
				spot = rand() % arena.crateCount;
				int sx = arena.crateSpots[spot*2];
				int sy = arena.crateSpots[(spot*2)+1];
				int dx = sx - player.getX();
				int dy = sy - player.getY();
				
				if(sx == crate.getX() && sy == crate.getY())continue;
				if((dx*dx) + (dy*dy) < CRATE_CLEARANCE * CRATE_CLEARANCE)continue;
				break;
			}
			crate.move(arena.crateSpots[spot*2], arena.crateSpots[(spot*2)+1]);
			int newWep = 0;
			do newWep = rand() % NUM_WEAPONS;
			while(newWep == weapon);
			crate.setWeapon(newWep);
			crate.setDead(false);
		}
		
		//General Slow things
		if(loopSlower % 7 == 0){
			cloudScroll++;
			REG_BG3HOFS = cloudScroll;
		
			const Arena& arena = arenas[currentArena];
			for(int i = 0; i < arena.fireCount; i++){
				SetTile(30, arena.fireTiles[i*2], arena.fireTiles[(i*2)+1], 16+rand()%5);
			}
			
			player.updateFrame();
			
			for(int i = 0; i < enemies.size(); i++){
				if(!enemies.at(i).isDead()){
					enemies.at(i).updateFrame();
				}
			}
			ObjBuffer[2].attr2 = ATTR2_ID8(skinTileBase(selectedSkin) + player.getFrame());
			for(int i = 0; i < enemies.size(); i++){
				ObjBuffer[i+OBJ_ENEMY_BASE].attr2 = ATTR2_ID8(monsterTile(enemies.at(i)));
			}
			
		}
		
		reloadTimer++;
		if(weapon == 0 || weapon == 5 || weapon == 6 || weapon == 7){
			if(reloadTimer >= 60){
				shootCoolDown = true;
			}
		}else if(weapon == 2 || weapon == 4){
			if(reloadTimer >= 45){
				shootCoolDown = true;
			}
		}else if (weapon == 3 || weapon == 9){
			if(reloadTimer >= 20){
				shootCoolDown = true;
			}
		}else if(weapon ==  1){
			if(reloadTimer >= 6){
				shootCoolDown = true;
			}
		}else if(weapon == 10){
			//Flamethrower - a flame every third frame, and each only lives
			//18, so about six are ever alive at once
			if(reloadTimer >= 3){
				shootCoolDown = true;
			}
		}else{
			//Minigun. At 2 frames it fires 30 a second, which is what the pool
			//size has to be able to carry - a round takes about 56 frames to
			//cross from the middle of the arena to a wall.
			if(reloadTimer >= 2){
				shootCoolDown = true;
			}
		}
		if(reloadTimer > 1000)reloadTimer = 60;
		
		//Monster spawner
		int spawnOdds = 100;
		if(benchFrames > 0)spawnOdds = BENCH_SPAWN_ODDS;
		
		if(rand()%spawnOdds == 0){
			spawnMonster();	
		}
		
		if(benchFrames > 0 && ((BENCH_FRAMES - benchFrames) % BENCH_WEAPON_HOLD) == 0){
			weapon = (weapon + 1) % NUM_WEAPONS;
		}
		
		//Player state updates
		if(player.getJumping())player.setState(2);
		else if(player.getRunning())player.setState(1);
		else player.setState(0);
		
		if(!player.getDir()){
			ObjBuffer[0].attr1 |= ATTR1_HFLIP;
			ObjBuffer[2].attr1 |= ATTR1_HFLIP;
		}else{
			ObjBuffer[0].attr1 &= ~(ATTR1_HFLIP);
			ObjBuffer[2].attr1 &= ~(ATTR1_HFLIP);
		}
		
		if(weapon == 8)ObjBuffer[0].attr0 |= ATTR0_SHAPE(1);
		else ObjBuffer[0].attr0 &= ~(ATTR0_SHAPE(1));
		
		ObjBuffer[0].attr2 = ATTR2_ID8(weaponTile(weapon));
			
		for(int i = 0; i < enemies.size(); i++){
			if(!enemies.at(i).isDead()){
				if(!enemies.at(i).getDir())ObjBuffer[i+OBJ_ENEMY_BASE].attr1 |= ATTR1_HFLIP;
				else ObjBuffer[i+OBJ_ENEMY_BASE].attr1 &= ~(ATTR1_HFLIP);
			}
		}
		
		updateExplosion();
		
		player.update();
		
		if(player.isDead()){
			if(benchFrames > 0){
				//Invincible for the benchmark - drop back in and carry on, so
				//a bad run still measures a full ten seconds of work
				benchDeaths++;
				(&player)->~Player();
				new (&player) Player(arenas[currentArena].playerX, arenas[currentArena].playerY);
			}else{
				lifetimeDeaths++;
				progressDirty = true;
				saveProgress();
				enterMenu(2);
			}
		}
	
		//Render
		if(!player.getDir()){
			if(weapon != 8)SetObjectX(0, player.getX()-4);
			else{
				if(player.getX()-12 <= 0) SetObjectX(0, 0);
				else SetObjectX(0, player.getX()-12);
			}	
		}else{
			SetObjectX(0, player.getX()+4);
		}
		SetObjectY(0, player.getY()+1);
		
		SetObjectX(1, crate.getX());
		SetObjectY(1, crate.getY());
		
		SetObjectX(2, player.getX());
		SetObjectY(2, player.getY());
		
		for(int i = 0; i < enemies.size(); i++){
			if(!enemies.at(i).isDead()){
				SetObjectX(i+OBJ_ENEMY_BASE, enemies.at(i).getX());
				SetObjectY(i+OBJ_ENEMY_BASE, enemies.at(i).getY());
			}
		}
		
		for(int i = 0; i < bullets.size(); i++){
			if(!bullets.at(i).isDead()){
				if(bullets.at(i).getType() == 8){
					//The slash hitbox is 16x12; draw its blade at the outer end
					int reach = player.getDir() ? 8 : 0;
					SetObjectX(i+OBJ_BULLET_BASE, bullets.at(i).getX() + reach);
					SetObjectY(i+OBJ_BULLET_BASE, bullets.at(i).getY() + 2);
				}else{
					SetObjectX(i+OBJ_BULLET_BASE, bullets.at(i).getX());
					SetObjectY(i+OBJ_BULLET_BASE, bullets.at(i).getY());
				}
			}
		}
		
		applyShake();

		drawText(112,10,toString(score));
		
		if(showMeter && benchFrames <= 0){
			drawText(  4, 150, "CPU");
			drawText( 32, 150, toString(frameTicks * 100 / FRAME_TICKS));
			drawText( 72, 150, "PK");
			drawText( 96, 150, toString(peakTicks * 100 / FRAME_TICKS));
			drawText(136, 150, "OV");
			drawText(160, 150, toString(overruns));
		}
		
		if(pickupTimer > 0){
			//Centre the name over the crate, sitting just above it, and keep
			//it on screen when the crate was near an edge
			int textWidth = (int) pickupText.length() * 8;
			int textX = pickupX + (crate.getWidth()/2) - (textWidth/2);
			if(textX < 0)textX = 0;
			if(textX > SCREEN_WIDTH - textWidth)textX = SCREEN_WIDTH - textWidth;
			
			int textY = pickupY - 10;
			if(textY < 0)textY = 0;
			
			drawText(textX, textY, pickupText);
			pickupTimer--;
		}
		
		frameTicks = REG_TM0D;
		if(frameTicks > peakTicks)peakTicks = frameTicks;
		if(frameTicks > FRAME_TICKS)overruns++;
		
		if(benchFrames > 0){
			int live = 0;
			for(int i = 0; i < bullets.size(); i++)if(!bullets.at(i).isDead())live++;
			if(live > benchPeakBullets)benchPeakBullets = live;
			
			live = 0;
			for(int i = 0; i < enemies.size(); i++)if(!enemies.at(i).isDead())live++;
			if(live > benchPeakEnemies)benchPeakEnemies = live;
			
			benchTickSum += frameTicks;
			if(frameTicks > benchPeak)benchPeak = frameTicks;
			if(frameTicks < benchLow)benchLow = frameTicks;
			if(frameTicks > FRAME_TICKS)benchOverruns++;
			
			int bin = frameTicks >> BENCH_BIN_SHIFT;
			if(bin >= BENCH_BINS)bin = BENCH_BINS - 1;
			benchHist[bin]++;
		}
		
		WaitVSync();
		
		//Restart the measurement at the top of VBlank, so that the object and
		//text updates below are counted as part of the frame's work
		REG_TM0CNT = 0;
		REG_TM0D = 0;
		REG_TM0CNT = TM_ENABLE | TM_FREQ_64;
		
		UpdateObjects();
		clearText();
		loopSlower++;
		
		if(benchFrames > 0){
			benchFrames--;
			
#ifdef BENCH_HEARTBEAT
			//Diagnostic only, and never every frame: an SRAM write makes the
			//emulator flush its save file, which is itself a stall
			if((benchFrames % 60) == 0){
				SRAM_BASE[128] = 'H';
				SRAM_BASE[129] = 'B';
				SRAM_BASE[130] = benchFrames & 0xFF;
				SRAM_BASE[131] = (benchFrames >> 8) & 0xFF;
			}
#endif
			
			if(benchFrames == 0)finishBenchmark();
		}
	}

	return 0;

}

void enterMenu(int menu){
	//A death mid-shake would otherwise leave the menus knocked askew
	shakeTimer = 0;
	REG_BG0HOFS = 0; REG_BG0VOFS = 0;
	REG_BG1HOFS = 0; REG_BG1VOFS = 0;
	REG_BG2HOFS = 0; REG_BG2VOFS = 0;
	REG_BG3VOFS = 0;
	LoadPaletteBGData(0, menuBGPal, menuBGPalLen);
	clearText();
	
	bool active = true;
	int option = 0;
	int skinChoice = 0;
	int arenaChoice = 0;
	int keyPressCoolDown = 30;
	int navCoolDown = 0;
	const int MAIN_MENU_OPTIONS = 2;
	const int EXTRAS_OPTIONS = 5;
	if(menu == 0)ClearObjects();
	
	//MENU LOOP
	while(active){
		keyPressCoolDown--;
		if(keyPressCoolDown <= 0)keyPressCoolDown = 0;
		navCoolDown--;
		if(navCoolDown <= 0)navCoolDown = 0;
		clearText();
		
		//Game over is drawn over the frozen arena, so its text has to stay
		//clear of the game objects. Every other page starts from a blank
		//screen, so the text can begin right after the player and the credits
		//get the whole run of slots they need.
		if(menu == 2)textBase = OBJ_TEXT_GAME;
		else textBase = OBJ_TEXT_MENU;
		
		switch(menu){
			case 0://Main menu
			if((REG_KEYINPUT & KEY_UP) == 0 && navCoolDown == 0){
				option--;
				if(option < 0)option = MAIN_MENU_OPTIONS-1;
				navCoolDown = 12;
			}
			if((REG_KEYINPUT & KEY_DOWN) == 0 && navCoolDown == 0){
				option++;
				if(option >= MAIN_MENU_OPTIONS)option = 0;
				navCoolDown = 12;
			}
			if((REG_KEYINPUT & KEY_A) == 0 && keyPressCoolDown == 0){
				if(option == 0){
					active = false;
					gameInit();
				}else{
					menu = 6;
					option = 0;
					keyPressCoolDown = 30;
				}
			}
			drawText(60,18,"Super Crate Box");
			drawMenuOption( 62, "PLAY",   option == 0);
			drawMenuOption( 80, "EXTRAS", option == 1);
			drawTextCentered(126, arenas[currentArena].name);
			drawTextCentered(140,"BEST " + toString(highScore));
			break;
			
			case 6://Extras
			if((REG_KEYINPUT & KEY_UP) == 0 && navCoolDown == 0){
				option--;
				if(option < 0)option = EXTRAS_OPTIONS-1;
				navCoolDown = 12;
			}
			if((REG_KEYINPUT & KEY_DOWN) == 0 && navCoolDown == 0){
				option++;
				if(option >= EXTRAS_OPTIONS)option = 0;
				navCoolDown = 12;
			}
			if((REG_KEYINPUT & KEY_A) == 0 && keyPressCoolDown == 0){
				if(option == 0){
					menu = 3;
					skinChoice = selectedSkin;
				}else if(option == 1){
					menu = 5;
					arenaChoice = currentArena;
				}else if(option == 2){
					menu = 7;
				}else if(option == 3){
					active = false;
					startBenchmark();
				}else{
					menu = 1;
				}
				keyPressCoolDown = 30;
			}
			if((REG_KEYINPUT & KEY_B) == 0 && keyPressCoolDown == 0){
				menu = 0;
				option = 1;
				keyPressCoolDown = 30;
				break;
			}
			drawTextCentered(18, "EXTRAS");
			drawMenuOption( 48, "SKINS",     option == 0);
			drawMenuOption( 62, "ARENA",     option == 1);
			drawMenuOption( 76, "STATS",     option == 2);
			drawMenuOption( 90, "BENCHMARK", option == 3);
			drawMenuOption(104, "ABOUT",     option == 4);
			drawTextCentered(137, "B = BACK");
			break;
			
			case 7://Stats
			if(((REG_KEYINPUT & KEY_A) == 0 || (REG_KEYINPUT & KEY_B) == 0) && keyPressCoolDown == 0){
				menu = 6;
				option = 2;
				keyPressCoolDown = 30;
				break;
			}
			drawTextCentered(18, "STATS");
			drawText(28, 44, "BEST SCORE");
			drawText(164, 44, toString(highScore));
			drawText(28, 58, "CRATES");
			drawText(164, 58, toString(lifetimeCrates));
			drawText(28, 72, "KILLS");
			drawText(164, 72, toString(lifetimeKills));
			drawText(28, 86, "ANGRY KILLS");
			drawText(164, 86, toString(lifetimeAngryKills));
			drawText(28, 100, "DEATHS");
			drawText(164, 100, toString(lifetimeDeaths));
			drawTextCentered(137, "B = BACK");
			break;
			
			case 1://About
			if(((REG_KEYINPUT & KEY_A) == 0 || (REG_KEYINPUT & KEY_B) == 0) && keyPressCoolDown == 0){
				menu = 6;
				option = 4;
				keyPressCoolDown = 30;
				break;
			}
			
			drawText(30, 20, "GBA - Super Crate Box");
			drawText(30,60, "Created by Peter Black");
			drawText(70,80, "for CGA 2014");
			drawTextCentered(104, "Updated by Tyler Barron");
			drawText(8, 135, "Assets & Gameplay C Vlambeer");
			break;
			
			case 2://Game Over
			if((REG_KEYINPUT & KEY_A) == 0 && keyPressCoolDown == 0){
				active = false;
				gameInit();
			}
			if((REG_KEYINPUT & KEY_B) == 0 && keyPressCoolDown == 0){
				menu = 0;
				ClearObjects();
				keyPressCoolDown = 30;
			}
		
			drawText(82,60,"GAME OVER");
			drawText(85,80,"Score:");
			drawText(133,80,toString(score));
			drawText(85,95,"Deaths:");
			drawText(141,95,toString(lifetimeDeaths));
			drawText(77, 125,"A = Retry");
			drawText(65, 135,"B = Main Menu");
			
			
			break;
			
			case 4://Benchmark results
			if((REG_KEYINPUT & KEY_A) == 0 && keyPressCoolDown == 0){
				active = false;
				startBenchmark();
			}
			if((REG_KEYINPUT & KEY_B) == 0 && keyPressCoolDown == 0){
				menu = 6;
				option = 3;
				ClearObjects();
				keyPressCoolDown = 30;
			}
			{
				//Averaged first so the multiply cannot run past 32 bits
				int avg = benchTickSum / BENCH_FRAMES;
				int fps10 = 0;
				if(benchCycles > 0)fps10 = (BENCH_FRAMES * 16384 * 10) / (int) benchCycles;
				
				drawTextCentered(14, "BENCHMARK");
				
				drawText(28, 36, "AVG");
				drawText(160, 36, toString((avg * 100) / FRAME_TICKS) + "%");
				
				drawText(28, 50, "P95");
				drawText(160, 50, toString((benchPercentile(95) * 100) / FRAME_TICKS) + "%");
				
				drawText(28, 64, "PEAK");
				drawText(160, 64, toString((benchPeak * 100) / FRAME_TICKS) + "%");
				
				drawText(28, 78, "FPS");
				drawText(160, 78, toString(fps10 / 10) + "." + toString(fps10 % 10));
				
				drawText(28, 92, "SLOW");
				drawText(160, 92, toString(benchOverruns));
				
				drawText(28, 106, "DEATHS");
				drawText(160, 106, toString(benchDeaths));
				
				drawTextCentered(126, "A = AGAIN");
				drawTextCentered(140, "B = MENU");
			}
			break;
			
			case 5://Arena select
			if((REG_KEYINPUT & KEY_LEFT) == 0 && navCoolDown == 0){
				arenaChoice--;
				if(arenaChoice < 0)arenaChoice = NUM_ARENAS-1;
				navCoolDown = 12;
			}
			if((REG_KEYINPUT & KEY_RIGHT) == 0 && navCoolDown == 0){
				arenaChoice++;
				if(arenaChoice >= NUM_ARENAS)arenaChoice = 0;
				navCoolDown = 12;
			}
			if((REG_KEYINPUT & KEY_A) == 0 && keyPressCoolDown == 0){
				if(arenaUnlocked(arenaChoice)){
					currentArena = arenaChoice;
					progressDirty = true;
					saveProgress();
				}
				keyPressCoolDown = 15;
			}
			if((REG_KEYINPUT & KEY_B) == 0 && keyPressCoolDown == 0){
				menu = 6;
				option = 1;
				keyPressCoolDown = 30;
				break;
			}
			
			drawTextCentered(18, "ARENA");
			drawTextCentered(52, arenas[arenaChoice].name);
			
			if(!arenaUnlocked(arenaChoice)){
				drawTextCentered(76, "LOCKED");
				drawTextCentered(90, "SCORE " + toString(arenas[arenaChoice].unlockScore) + " IN");
				drawTextCentered(102, arenas[arenaChoice-1].name);
			}else{
				drawTextCentered(76, "BEST " + toString(arenaBest[arenaChoice]));
				if(arenaChoice == currentArena)drawTextCentered(94, "SELECTED");
				else drawTextCentered(94, "A = SELECT");
			}
			
			drawTextCentered(125, toString(arenaChoice+1) + " OF " + toString(NUM_ARENAS));
			drawTextCentered(137, "B = BACK");
			break;
			
			case 3://Skin select
			if((REG_KEYINPUT & KEY_LEFT) == 0 && navCoolDown == 0){
				skinChoice--;
				if(skinChoice < 0)skinChoice = NUM_SKINS-1;
				navCoolDown = 12;
			}
			if((REG_KEYINPUT & KEY_RIGHT) == 0 && navCoolDown == 0){
				skinChoice++;
				if(skinChoice >= NUM_SKINS)skinChoice = 0;
				navCoolDown = 12;
			}
			if((REG_KEYINPUT & KEY_A) == 0 && keyPressCoolDown == 0){
				if(skinUnlocked(skinChoice)){
					selectedSkin = skinChoice;
					progressDirty = true;
					saveProgress();
				}
				keyPressCoolDown = 15;
			}
			if((REG_KEYINPUT & KEY_B) == 0 && keyPressCoolDown == 0){
				//Put the equipped skin back before leaving the preview
				SetObject(0, ATTR0_HIDE, 0, 0);
				applySkin(selectedSkin);
				menu = 6;
				option = 0;
				keyPressCoolDown = 30;
				break;
			}
			
			//Show the character being browsed, in its own colours
			applySkin(skinChoice);
			SetObject(0,
			          ATTR0_SHAPE(0) | ATTR0_8BPP | ATTR0_REG | ATTR0_Y(56),
			          ATTR1_SIZE(0) | ATTR1_X(116),
			          ATTR2_ID8(skinTileBase(skinChoice)));
			
			drawTextCentered(20, "SKINS");
			drawTextCentered(80, skins[skinChoice].name);
			
			if(!skinUnlocked(skinChoice))drawTextCentered(96, skinRequirement(skinChoice));
			else if(skinChoice == selectedSkin)drawTextCentered(96, "EQUIPPED");
			else drawTextCentered(96, "A = EQUIP");
			
			drawTextCentered(125, toString(skinChoice+1) + " OF " + toString(NUM_SKINS));
			drawTextCentered(137, "B = BACK");
			break;
		}
		
		
		WaitVSync();
		UpdateObjects();
	}
	
	clearText();
	textBase = OBJ_TEXT_GAME;
	
	LoadPaletteBGData(0, backgroundPal, backgroundPalLen);
	applyArenaPalette(currentArena);
}

std::string toString(int number){
	int holder = number;
	
	int digits = 0; 
	
	do { holder /= 10; digits++; } while (holder > 0);
	char arr[digits+1];
	arr[digits] = 0;
	
	int divider = 1;
	for(int i = 0; i < digits; i++) divider *= 10;
	int value = 0;
	
	for(int i = 0; i < digits; i++){
		divider /= 10;
		
		int holder = number;
		value = holder/divider;
		
		arr[i] = (value%10) +48;
	}
	
	std::string str(arr);
	return str;
}

int activeLetters = 0;
void drawText(int x, int y, const std::string& string){
	int startPos = activeLetters;
	for(int i = 0; i < string.length(); i++){
		if(startPos+i+textBase >= NUM_OBJECTS)break;
		activeLetters++;
		SetObject(startPos+i+textBase,
	          ATTR0_SHAPE(0) | ATTR0_8BPP | ATTR0_REG | ATTR0_Y(y),
			  ATTR1_SIZE(0) | ATTR1_X(x+(i*8)),
			  ATTR2_ID8(string.at(i) + 64));
	}
}

void drawTextCentered(int y, const std::string& string){
	drawText((SCREEN_WIDTH - ((int) string.length() * 8)) / 2, y, string);
}

void clearText(){
	for(int i = 0; i < activeLetters; i++){
		SetObject(i+textBase, ATTR0_HIDE, 0, 0);
	}
	activeLetters = 0;
}

const char* weaponName(int weapon){
	switch(weapon){
		case 0:return "BAZOOKA";
		case 1:return "MACHINE GUN";
		case 2:return "SHOTGUN";
		case 3:return "REVOLVER";
		case 4:return "DISC GUN";
		case 5:return "MINE";
		case 6:return "GRENADE LAUNCHER";
		case 7:return "LASER GUN";
		case 8:return "MINIGUN";
		case 9:return "KATANA";
		case 10:return "FLAMETHROWER";
	}
	return "";
}

void drawMenuOption(int y, const std::string& label, bool selected){
	int width = (int) label.length() * 8;
	int x = (SCREEN_WIDTH - width) / 2;
	
	drawText(x, y, label);
	if(selected){
		drawText(x - 16, y, ">");
		drawText(x + width + 8, y, "<");
	}
}

//Draw the 64x64 disc straight into object VRAM. Objects are in 2D mapping, so
//each row of the sprite sits 16 tiles further along than the last.
void startBenchmark(){
	//Same seed every run, and every random number after this - the monsters,
	//the crates and the controls - comes off that one stream, so the whole run
	//replays identically on any build
	srand(BENCH_SEED);
	gameInit();
	
	benchFrames = BENCH_FRAMES;
	benchTickSum = 0;
	benchPeak = 0;
	benchLow = 0x7FFFFFF;
	benchOverruns = 0;
	benchShots = 0;
	benchExplosions = 0;
	benchPeakBullets = 0;
	benchPeakEnemies = 0;
	
	for(int i = 0; i < BENCH_BINS; i++)benchHist[i] = 0;
	benchDeaths = 0;
	benchCycles = 0;
	benchHeldKeys = 0x3FF;	//a set bit is a key that is not held
	benchHoldTimer = 0;
	
	REG_TM2CNT = 0;
	REG_TM3CNT = 0;
	REG_TM2D = 0;
	REG_TM3D = 0;
	REG_TM2CNT = TM_ENABLE | TM_FREQ_1024;
	REG_TM3CNT = TM_ENABLE | TM_CASCADE;
	
	//Stamp a placeholder over the results block. Emulators only create their
	//battery file once a game touches SRAM, and nothing else here writes until
	//the run is over - without this the file does not exist to be read back,
	//and a finished run looks identical to a crashed one. The reader wants
	//"BNC2", so a run that never finishes is still correctly rejected.
	SRAM_BASE[64] = 'B';
	SRAM_BASE[65] = 'N';
	SRAM_BASE[66] = 'C';
	SRAM_BASE[67] = '0';
}

void finishBenchmark(){
	benchCycles = (((unsigned int) REG_TM3D) << 16) | REG_TM2D;
	REG_TM2CNT = 0;
	REG_TM3CNT = 0;
	
	writeBenchResults();
	
#ifdef BENCH_AUTORUN
	//Nothing is driving this build, so keep rewriting the results: the
	//emulator flushes its save file periodically, and this guarantees the
	//finished figures are in it whenever that happens.
	while(true){
		WaitVSync();
		writeBenchResults();
	}
#endif
	
	ClearObjects();
	enterMenu(4);
}

//Smallest frame time that at least this percent of frames came in under
int benchPercentile(int percent){
	int want = ((BENCH_FRAMES * percent) + 99) / 100;
	int seen = 0;
	
	for(int i = 0; i < BENCH_BINS; i++){
		seen += benchHist[i];
		if(seen >= want)return (i << BENCH_BIN_SHIFT) + (1 << (BENCH_BIN_SHIFT-1));
	}
	return benchPeak;
}

//Results land at SRAM offset 64, clear of the saved progress at the bottom,
//so they can be read straight out of an emulator's battery file.
void writeBenchResults(){
	int avg = benchTickSum / BENCH_FRAMES;
	int p95 = benchPercentile(95);
	
	const int at = 64;
	SRAM_BASE[at+0] = 'B';
	SRAM_BASE[at+1] = 'N';
	SRAM_BASE[at+2] = 'C';
	SRAM_BASE[at+3] = '2';
	
	int field[14];
	field[0] = BENCH_FRAMES;
	field[1] = avg;
	field[2] = p95;
	field[3] = benchPeak;
	field[4] = benchLow;
	field[5] = benchOverruns;
	field[6] = benchDeaths;
	field[7] = FRAME_TICKS;
	field[8] = (int) benchCycles;
	field[9] = benchShots;
	field[10] = benchExplosions;
	field[11] = benchPeakBullets;
	field[12] = benchPeakEnemies;
	field[13] = score;
	
	for(int i = 0; i < 14; i++){
		SRAM_BASE[at + 4 + (i*4) + 0] = field[i] & 0xFF;
		SRAM_BASE[at + 4 + (i*4) + 1] = (field[i] >> 8) & 0xFF;
		SRAM_BASE[at + 4 + (i*4) + 2] = (field[i] >> 16) & 0xFF;
		SRAM_BASE[at + 4 + (i*4) + 3] = (field[i] >> 24) & 0xFF;
	}
}

int readKeys(){
	if(benchFrames <= 0)return REG_KEYINPUT;
	
	//Hold a direction for a while at a time, so the run reads as play rather
	//than a twitch and the player actually crosses the arena
	benchHoldTimer--;
	if(benchHoldTimer <= 0){
		benchHoldTimer = 6 + (rand() % 20);
		benchHeldKeys = 0x3FF;
		
		if(rand() % 4 != 0){
			if(rand() % 2)benchHeldKeys &= ~KEY_LEFT;
			else benchHeldKeys &= ~KEY_RIGHT;
		}
		if(rand() % 3 == 0)benchHeldKeys &= ~KEY_UP;
	}
	
	//Lean on the fire button; the reload timer decides what actually comes out
	int keys = benchHeldKeys;
	if(rand() % 4 != 0)keys &= ~KEY_A;
	
	return keys;
}

//A billowing fireball rather than a plain disc: a main ball with six lumps
//around it, in doubled coordinates so the centre falls between pixels. Each
//is x, y, radius; nothing reaches past 63, the edge of the 64x64 box.
const signed char BLAST_LUMPS[7][3] = {
	{  0,   0, 50},
	{ 36,   2, 26}, { 16,  30, 22}, {-18,  28, 27},
	{-33,  -2, 23}, {-16, -32, 25}, { 19, -28, 21},
};

void buildBlastSprite(){
	uint8_t tile[64];
	
	for(int ty = 0; ty < 8; ty++){
	for(int tx = 0; tx < 8; tx++){
		for(int y = 0; y < 8; y++){
		for(int x = 0; x < 8; x++){
			int px = (tx*8) + x;
			int py = (ty*8) + y;
			int dx = (2 * px) - 63;
			int dy = (2 * py) - 63;
			
			bool inside = false;
			for(int l = 0; l < 7; l++){
				int lx = dx - BLAST_LUMPS[l][0];
				int ly = dy - BLAST_LUMPS[l][1];
				int lr = BLAST_LUMPS[l][2];
				if((lx*lx) + (ly*ly) <= lr*lr){
					inside = true;
					break;
				}
			}
			
			uint8_t colour = 0;
			if(inside){
				//Bands by distance from the centre, with their edges
				//roughened by a cheap hash so they don't read as rings
				int jitter = (((px * 5) + (py * 3)) ^ (px * py)) & 7;
				int d2 = (dx*dx) + (dy*dy);
				int core = 22 + jitter;
				int mid  = 38 + jitter;
				
				if(d2 < core*core)colour = BLAST_CORE;
				else if(d2 < mid*mid)colour = BLAST_MID;
				else colour = BLAST_RIM;
			}
			tile[(y*8) + x] = colour;
		}
		}
		LoadTileData(4, BLAST_TILE + (ty*16) + tx, tile, 64);
	}
	}
}

//The fireball's colours over its life, as core, middle, rim, from the white
//flash to the last of the smoke. Each row is held for BLAST_STAGE_FRAMES.
const int BLAST_STAGES = 6;
const int BLAST_STAGE_FRAMES = 6;
const unsigned short BLAST_COLOURS[BLAST_STAGES][3] = {
	{RGB(31,31,31), RGB(31,31,20), RGB(31,24, 6)},
	{RGB(31,31,18), RGB(31,22, 4), RGB(29,10, 3)},
	{RGB(31,25, 6), RGB(31,14, 3), RGB(23, 5, 3)},
	{RGB(31,16, 4), RGB(24, 7, 3), RGB(14, 5, 4)},
	{RGB(22, 8, 4), RGB(13, 7, 6), RGB( 9, 7, 7)},
	{RGB(12, 9, 8), RGB( 9, 8, 8), RGB( 6, 6, 6)},
};

void buildAngrySprites(){
	const uint8_t* sheet = (const uint8_t*) spritesTiles;
	uint8_t tile[64];
	
	//Eight tiles of large-monster tops, their eight bottoms, then the six
	//small-monster frames
	for(int n = 0; n < 22; n++){
		int from, to;
		if(n < 8){
			from = 32 + n;
			to = ANGRY_LARGE_TILE + n;
		}else if(n < 16){
			from = 48 + (n - 8);
			to = ANGRY_LARGE_TILE + 16 + (n - 8);
		}else{
			from = 16 + (n - 16);
			to = ANGRY_SMALL_TILE + (n - 16);
		}
		
		for(int p = 0; p < 64; p++){
			uint8_t colour = sheet[(from * 64) + p];
			if(colour == 6)colour = 30;
			else if(colour == 8)colour = 31;
			else if(colour == 9)colour = 32;
			tile[p] = colour;
		}
		LoadTileData(4, to, tile, 64);
	}
}

int monsterTile(Monster& monster){
	int frame = monster.getFrame();
	if(!monster.getAngry())return frame;
	
	//A monster that has not animated yet is still on frame 0
	if(monster.getSize()){
		if(frame < 16)frame = 16;
		return ANGRY_SMALL_TILE + (frame - 16);
	}
	if(frame < 32)frame = 32;
	return ANGRY_LARGE_TILE + (frame - 32);
}

//Knock every background and every game object by a few pixels, fading out
//over the shake. Runs after all the objects have been placed for the frame,
//since everything visible has its position written fresh each frame.
void applyShake(){
	int sx = 0;
	int sy = 0;
	
	if(shakeTimer > 0){
		int size = (shakeTimer + 4) / 5;
		sx = (rand() % ((size * 2) + 1)) - size;
		sy = (rand() % ((size * 2) + 1)) - size;
		shakeTimer--;
	}
	
	//Backgrounds scroll the opposite way to the picture moving
	REG_BG0HOFS = -sx; REG_BG0VOFS = -sy;
	REG_BG1HOFS = -sx; REG_BG1VOFS = -sy;
	REG_BG2HOFS = -sx; REG_BG2VOFS = -sy;
	REG_BG3HOFS = cloudScroll - sx; REG_BG3VOFS = -sy;
	
	if(sx == 0 && sy == 0)return;
	
	for(int o = 0; o <= OBJ_BLAST; o++){
		if((ObjBuffer[o].attr0 & 0x300) == ATTR0_HIDE)continue;
		SetObjectX(o, (ObjBuffer[o].attr1 & ATTR1_X_MASK) + sx);
		SetObjectY(o, (ObjBuffer[o].attr0 & ATTR0_Y_MASK) + sy);
	}
}

//Rows of an 8x8 picture, one character per pixel
void loadPicture(int tileNum, const char* rows[8]){
	uint8_t tile[64];
	
	for(int y = 0; y < 8; y++){
		for(int x = 0; x < 8; x++){
			uint8_t colour = 0;
			switch(rows[y][x]){
				case 'y': colour = FLAME_YELLOW; break;
				case 'o': colour = FLAME_ORANGE; break;
				case 'r': colour = FLAME_RED;    break;
				case 'D': colour = GUN_DARK;     break;
				case 'L': colour = GUN_LIGHT;    break;
				case 's': colour = FLAME_SMOKE;  break;
			}
			tile[(y*8) + x] = colour;
		}
	}
	LoadTileData(4, tileNum, tile, 64);
}

void buildFlameSprites(){
	//Drawn facing right, like the rest of the weapons
	const char* icon[8] = {
		"........",
		"........",
		"......o.",
		"DDDDDDDy",
		"DLLLLLD.",
		"DDDDDD..",
		".DD.D...",
		".DD.....",
	};
	const char* big[8] = {
		"..oo....",
		".ooyoo..",
		"ooyyyoo.",
		"oyyyyyor",
		"oyyyyyor",
		"ooyyyoo.",
		".ooyoo..",
		"..oo....",
	};
	const char* middle[8] = {
		"........",
		"...rr...",
		"..rooor.",
		".rooyor.",
		".rooyor.",
		"..rooor.",
		"...rr...",
		"........",
	};
	const char* small[8] = {
		"........",
		"........",
		"...ss...",
		"..srrs..",
		"..sros..",
		"...ss...",
		"........",
		"........",
	};
	
	loadPicture(FLAME_TILE,     big);
	loadPicture(FLAME_TILE + 1, middle);
	loadPicture(FLAME_TILE + 2, small);
	loadPicture(FLAME_ICON_TILE, icon);
}

int weaponTile(int weapon){
	if(weapon == 10)return FLAME_ICON_TILE;
	return 22 + weapon;
}

//The benchmark plays invincibly with scripted input, so it doesn't count
void countKill(Monster& monster){
	if(benchFrames > 0)return;
	lifetimeKills++;
	if(monster.getAngry())lifetimeAngryKills++;
	progressDirty = true;
}

//Freeze everything where it is until START again. Returns true if the player
//chose to quit to the main menu instead.
bool pauseGame(){
	bool startHeld = true;	//it was just pressed to get here
	bool selectHeld = (REG_KEYINPUT & KEY_SELECT) == 0;
	
	while(true){
		bool startDown = (REG_KEYINPUT & KEY_START) == 0;
		bool selectDown = (REG_KEYINPUT & KEY_SELECT) == 0;
		
		if(startDown && !startHeld)break;
		if(selectDown && !selectHeld){
			clearText();
			return true;
		}
		startHeld = startDown;
		selectHeld = selectDown;
		
		clearText();
		drawTextCentered(60, "PAUSED");
		drawTextCentered(84, "START = RESUME");
		drawTextCentered(98, "SELECT = QUIT");
		
		WaitVSync();
		UpdateObjects();
	}
	
	clearText();
	return false;
}

void updateExplosion(){
	if(explosionTimer <= 0){
		ObjBuffer[OBJ_BLAST].attr0 = ATTR0_HIDE;
		return;
	}
	
	int age = EXPLOSION_FRAMES - explosionTimer;
	int radius = explosionRadius;
	
	if(age < EXPLOSION_GROW){
		radius = (explosionRadius * (age + 1)) / EXPLOSION_GROW;
	}else if(age >= EXPLOSION_GROW + EXPLOSION_HOLD){
		radius = (explosionRadius * (EXPLOSION_FRAMES - age)) / EXPLOSION_FADE;
	}
	if(radius < 2)radius = 2;
	
	int stage = age / BLAST_STAGE_FRAMES;
	if(stage >= BLAST_STAGES)stage = BLAST_STAGES - 1;
	SetPaletteObj(BLAST_CORE, BLAST_COLOURS[stage][0]);
	SetPaletteObj(BLAST_MID,  BLAST_COLOURS[stage][1]);
	SetPaletteObj(BLAST_RIM,  BLAST_COLOURS[stage][2]);
	
	//Whatever the disc has swallowed dies. Compared squared, to keep a square
	//root out of the frame. Nothing is killed on the way back down.
	if(age < EXPLOSION_GROW + EXPLOSION_HOLD){
		for(int i = 0; i < enemies.size(); i++){
			if(enemies.at(i).isDead())continue;
			
			int dx = (enemies.at(i).getX() + (enemies.at(i).getWidth()/2)) - explosionX;
			int dy = (enemies.at(i).getY() + (enemies.at(i).getHeight()/2)) - explosionY;
			
			if((dx*dx) + (dy*dy) <= radius * radius){
				enemies.at(i).setDead(true);
				countKill(enemies.at(i));
			}
		}
	}
	
	//pa and pd are the inverse scale in 8.8 fixed point, so the 32 pixel
	//source radius is magnified to the radius we want. Affine set 0 lives in
	//the spare halfword of the first four objects.
	int scale = (256 * 32) / radius;
	
	ObjBuffer[0].pad = scale;	//pa
	ObjBuffer[1].pad = 0;		//pb
	ObjBuffer[2].pad = 0;		//pc
	ObjBuffer[3].pad = scale;	//pd
	
	//The double-size flag gives the sprite a 128 pixel box to grow into, and
	//centres the disc within it
	SetObject(OBJ_BLAST,
	          ATTR0_AFF_DBL | ATTR0_8BPP | ATTR0_SQUARE | ATTR0_Y((explosionY - 64) & 0xFF),
	          ATTR1_AFF(0) | ATTR1_SIZE(3) | ATTR1_X((explosionX - 64) & 0x1FF),
	          ATTR2_ID8(BLAST_TILE) | ATTR2_PRIO(0));
	
	explosionTimer--;
}

//Paint an arena's four layers into their screenblocks. Only ever called
//between games - it writes 4096 tiles.
void loadArena(int index){
	if(index < 0 || index >= NUM_ARENAS)index = 0;
	const Arena& arena = arenas[index];
	
	for(int i = 0; i < girderMapLen; i++){
		SetTile(27, i%32, i/32, arena.clouds[i]);
		SetTile(28, i%32, i/32, arena.building[i]);
		SetTile(29, i%32, i/32, arena.fence[i]);
		SetTile(30, i%32, i/32, arena.girders[i]);
	}
}

//Has to run after anything that loads backgroundPal wholesale - leaving a menu
//does exactly that, which would otherwise put the girders back to red.
void applyArenaPalette(int index){
	if(index < 0 || index >= NUM_ARENAS)index = 0;
	const Arena& arena = arenas[index];
	
	for(int i = 0; i < arena.paletteCount; i++){
		SetPaletteBG(arena.palette[i].entry, arena.palette[i].colour);
	}
}

bool arenaUnlocked(int index){
	if(index <= 0)return true;
	return arenaBest[index-1] >= arenas[index].unlockScore;
}

int skinTileBase(int skin){
	if(skin <= 0)return 0;	//the first skin is the original sprite art
	return SKIN_TILE_BASE + ((skin - 1) * PLAYER_SKIN_FRAMES);
}

void applySkin(int skin){
	SetPaletteObj(16, skins[skin].colours[0]);
	SetPaletteObj(17, skins[skin].colours[1]);
	SetPaletteObj(19, skins[skin].colours[2]);
	SetPaletteObj(20, skins[skin].colours[3]);
}

bool skinUnlocked(int skin){
	if(skins[skin].unlockCrates > 0)return lifetimeCrates >= skins[skin].unlockCrates;
	return highScore >= skins[skin].unlockScore;
}

std::string skinRequirement(int skin){
	if(skins[skin].unlockCrates > 0)return toString(skins[skin].unlockCrates) + " CRATES";
	return "SCORE " + toString(skins[skin].unlockScore);
}

//Saved progress in SRAM:
//  [0..3]  tag "SCB5"
//  [4..5]  best score anywhere
//  [6..9]  crates collected across every game
//  [10]    equipped skin
//  [11]    chosen arena
//  [12..]  best score in each arena, two bytes each
//  then    deaths across every game, four bytes
//  then    monsters killed, then angry monsters killed, four bytes each
const int SRAM_DEATHS = 12 + (NUM_ARENAS * 2);
const int SRAM_KILLS = SRAM_DEATHS + 4;
const int SRAM_ANGRY_KILLS = SRAM_KILLS + 4;

int readSram32(int at){
	return SRAM_BASE[at] | (SRAM_BASE[at+1] << 8)
	     | (SRAM_BASE[at+2] << 16) | (SRAM_BASE[at+3] << 24);
}

void writeSram32(int at, int value){
	SRAM_BASE[at]   = value & 0xFF;
	SRAM_BASE[at+1] = (value >> 8) & 0xFF;
	SRAM_BASE[at+2] = (value >> 16) & 0xFF;
	SRAM_BASE[at+3] = (value >> 24) & 0xFF;
}

void loadProgress(){
	highScore = 0;
	lifetimeCrates = 0;
	lifetimeDeaths = 0;
	lifetimeKills = 0;
	lifetimeAngryKills = 0;
	selectedSkin = 0;
	currentArena = 0;
	for(int i = 0; i < NUM_ARENAS; i++)arenaBest[i] = 0;
	
	bool haveTag = (SRAM_BASE[0] == 'S' && SRAM_BASE[1] == 'C' && SRAM_BASE[2] == 'B');
	
	if(haveTag && SRAM_BASE[3] >= '3' && SRAM_BASE[3] <= '5'){
		highScore = SRAM_BASE[4] | (SRAM_BASE[5] << 8);
		lifetimeCrates = SRAM_BASE[6] | (SRAM_BASE[7] << 8)
		               | (SRAM_BASE[8] << 16) | (SRAM_BASE[9] << 24);
		selectedSkin = SRAM_BASE[10];
		currentArena = SRAM_BASE[11];
		
		for(int i = 0; i < NUM_ARENAS; i++){
			arenaBest[i] = SRAM_BASE[12 + (i*2)] | (SRAM_BASE[13 + (i*2)] << 8);
		}
		
		//Deaths only started being counted in version 4, kills in 5
		if(SRAM_BASE[3] >= '4')lifetimeDeaths = readSram32(SRAM_DEATHS);
		if(SRAM_BASE[3] >= '5'){
			lifetimeKills = readSram32(SRAM_KILLS);
			lifetimeAngryKills = readSram32(SRAM_ANGRY_KILLS);
		}else{
			progressDirty = true;
		}
	}else if(haveTag && SRAM_BASE[3] == '2'){
		//Before the arenas existed, so every score was set in the first one
		highScore = SRAM_BASE[4] | (SRAM_BASE[5] << 8);
		lifetimeCrates = SRAM_BASE[6] | (SRAM_BASE[7] << 8)
		               | (SRAM_BASE[8] << 16) | (SRAM_BASE[9] << 24);
		selectedSkin = SRAM_BASE[10];
		arenaBest[0] = highScore;
		progressDirty = true;
	}else if(SRAM_BASE[0] == 'S' && SRAM_BASE[1] == 'C'){
		//A save from the version that only kept a high score
		highScore = SRAM_BASE[2] | (SRAM_BASE[3] << 8);
		arenaBest[0] = highScore;
		progressDirty = true;
	}
	
	if(selectedSkin < 0 || selectedSkin >= NUM_SKINS)selectedSkin = 0;
	if(!skinUnlocked(selectedSkin))selectedSkin = 0;
	if(currentArena < 0 || currentArena >= NUM_ARENAS)currentArena = 0;
	if(!arenaUnlocked(currentArena))currentArena = 0;
	
	saveProgress();
}

void saveProgress(){
	if(!progressDirty)return;
	
	SRAM_BASE[0] = 'S';
	SRAM_BASE[1] = 'C';
	SRAM_BASE[2] = 'B';
	SRAM_BASE[3] = '5';
	SRAM_BASE[4] = highScore & 0xFF;
	SRAM_BASE[5] = (highScore >> 8) & 0xFF;
	SRAM_BASE[6] = lifetimeCrates & 0xFF;
	SRAM_BASE[7] = (lifetimeCrates >> 8) & 0xFF;
	SRAM_BASE[8] = (lifetimeCrates >> 16) & 0xFF;
	SRAM_BASE[9] = (lifetimeCrates >> 24) & 0xFF;
	SRAM_BASE[10] = selectedSkin;
	SRAM_BASE[11] = currentArena;
	
	for(int i = 0; i < NUM_ARENAS; i++){
		SRAM_BASE[12 + (i*2)] = arenaBest[i] & 0xFF;
		SRAM_BASE[13 + (i*2)] = (arenaBest[i] >> 8) & 0xFF;
	}
	
	writeSram32(SRAM_DEATHS, lifetimeDeaths);
	writeSram32(SRAM_KILLS, lifetimeKills);
	writeSram32(SRAM_ANGRY_KILLS, lifetimeAngryKills);
	
	progressDirty = false;
}

void shoot(){
	benchShots++;
	
	switch(weapon){
	case 0://bazooka
		spawnBullet(0);
	break;
	case 1://machine gun
		spawnBullet(1);
	break;
	case 2://shotgun
		for(int i = 0; i < 8; i++){
			int bullet = spawnBullet(1);
			if(bullet < 0)break;
			bullets.at(bullet).setLift(rand() % 3 + (-1));
		}
	break;
	case 3://revolver
		spawnBullet(2);
	break;
	case 4://disc gun
		spawnBullet(3);
	break;
	case 5://mine
		spawnBullet(4);
	break;
	case 6://grenade launcher
		{
			//Lobbed up and forwards; updateGrenade takes it from there
			int bullet = spawnBullet(5);
			if(bullet >= 0)bullets.at(bullet).setLift(-2);
		}
	break;
	case 7://laser gun
		spawnBullet(6);
	break;
	case 8://minigun
		{
			int bullet = spawnBullet(1);
			if(bullet >= 0)bullets.at(bullet).setLift(rand() % 3 + (-1));
		}
	break;
	case 9://katana
		spawnBullet(8);
	break;
	case 10://flamethrower
		{
			//Out of the nozzle, with a little spread up and down
			int bullet = spawnBullet(10);
			if(bullet >= 0){
				if(player.getDir())bullets.at(bullet).move(player.getX()+10, player.getY()+1);
				else bullets.at(bullet).move(player.getX()-8, player.getY()+1);
				bullets.at(bullet).setLift(rand() % 3 + (-1));
			}
		}
	break;
	}
}

//A grenade arcs under gravity and bounces off anything solid - the fire
//included - losing a little each time, until its fuse runs out. Scenery never
//sets it off; only the fuse or a monster does. Lift is its vertical speed.
void updateGrenade(Bullet& grenade){
	if(grenade.charge()){
		grenade.setDead(true);
		explode(grenade.getX()+2, grenade.getY()+2, GRENADE_RADIUS);
		return;
	}
	
	int w = grenade.getWidth();
	int h = grenade.getHeight();
	
	//Sideways. checkMapCollision reports fire as 2; a grenade treats that
	//as solid, where tryMove would quietly let the fire eat it.
	if(grenade.getSpeed() > 0){
		int step = grenade.getDir() ? grenade.getSpeed() : -grenade.getSpeed();
		int edge = grenade.getDir() ? w : 0;
		
		if(checkMapCollision(grenade.getX()+step, grenade.getY(), edge, (h/2)+1) == 1){
			grenade.move(grenade.getX()+step, grenade.getY());
		}else{
			grenade.setDir(!grenade.getDir());
		}
	}
	
	//Gravity, a pixel a frame faster every fourth frame
	int vy = grenade.getLift();
	if((grenade.getCharger() % 4) == 0 && vy < 3)vy++;
	
	//Up and down a pixel at a time, so a fast drop can't skip a girder
	int dirY = (vy > 0) ? 1 : -1;
	for(int n = 0; n < vy * dirY; n++){
		int edge = (dirY > 0) ? h : 0;
		if(checkMapCollision(grenade.getX(), grenade.getY()+dirY, w/2, edge) == 1){
			grenade.move(grenade.getX(), grenade.getY()+dirY);
		}else if(dirY > 0){
			//Bounce back up at a bit over half the speed, and scrub off
			//sideways speed until it settles and sits there
			vy = (vy > 1) ? -(vy - 1) : 0;
			if(grenade.getSpeed() > 1 || (grenade.getSpeed() > 0 && vy == 0)){
				grenade.setSpeed(grenade.getSpeed() - 1);
			}
			break;
		}else{
			vy = 0;
			break;
		}
	}
	grenade.setLift(vy);
	
	//Out through a gap in the arena - gone, without a bang nobody would see
	if(grenade.getY() > SCREEN_HEIGHT || grenade.getX() < -8 || grenade.getX() > SCREEN_WIDTH){
		grenade.setDead(true);
	}
}

void explode(int x, int y, int radius){
	benchExplosions++;
	
	//Only starts the blast off; updateExplosion does the killing and the
	//drawing, so the damage arrives with the disc rather than all at once
	explosionX = x;
	explosionY = y;
	explosionRadius = radius;
	shakeTimer = SHAKE_FRAMES;
	explosionTimer = EXPLOSION_FRAMES;
	
	//Debris is thrown out purely for the look of the thing - it carries no
	//damage, and only a handful is used so the bullet pool stays available
	for(int i = 0; i < EXPLOSION_DEBRIS; i++){
		int bullet = spawnBullet(9);
		if(bullet < 0)break;
		
		bullets.at(bullet).setLift(rand() % 7 + (-3));
		bullets.at(bullet).setDir(rand() % 2);
		bullets.at(bullet).move(x, y);
	}
}

int spawnBullet(int type){
	int i = -1;
	int count = 0;
	
	if(bullets.size() < MAX_BULLETS){
		bullets.push_back(Bullet(player.getX(), player.getY(), type, weapon));
		i = bullets.size()-1;
	}else{
		for(int k = 0; k < MAX_BULLETS; k++){
			if(bullets.at(k).getType() == 7)count++;
			if(bullets.at(k).isDead()){
				if(type == 7){
					if(player.getDir())bullets[k] = Bullet(player.getX()+(count*8),player.getY(),type, weapon);
					else bullets[k] = Bullet(player.getX()-(count*8),player.getY(),type, weapon);
					i = k;
				}else{
					bullets[k] = Bullet(player.getX(),player.getY(),type, weapon);
					i = k;
				}
				break;
			}
		}
	}
	
	//Every bullet is still in flight, so there is nowhere to put a new one.
	//Without this the caller would be handed slot 0 and turn a live bullet around.
	if(i < 0)return -1;
	
	SetObject(i+OBJ_BULLET_BASE,
			ATTR0_SHAPE(0) | ATTR0_8BPP | ATTR0_REG | ATTR0_Y(bullets.at(i).getY() & 0xFF),
			ATTR1_SIZE(0) | ATTR1_X(bullets.at(i).getX() & 0x1FF),
			ATTR2_ID8(bullets.at(i).getFrame()));
			
	bullets.at(i).setDir(player.getDir());
	if(!bullets.at(i).getDir()){
		ObjBuffer[i+OBJ_BULLET_BASE].attr1 |= ATTR1_HFLIP;
	}
	
	return i;
}

void spawnMonster(){
	bool type = true;
	bool dir = true;
	if(rand()%3 == 0)type = false;
	if(rand()%2 == 0)dir = false;
	
	int startFrame = 32;
	if(type)startFrame = 16;
	
	int i = -1;
	
	int spawnX = arenas[currentArena].monsterX;
	int spawnY = arenas[currentArena].monsterY;
	
	if(enemies.size() < MAX_ENEMIES){
		enemies.push_back(Monster(spawnX,spawnY,type));
		i = enemies.size()-1;
	}else{
		for(int k = 0; k < MAX_ENEMIES; k++){
			if(enemies.at(k).isDead()){
				enemies[k] = Monster(spawnX,spawnY,type);
				i = k;
				break;
			}
		}
	}
	
	//Every monster is alive. Now the fire sends them round again rather than
	//removing them, a full house is common, and without this slot 0's sprite
	//would be reset to the new monster's size while the monster stayed put.
	if(i < 0)return;
	
	SetObject(i+OBJ_ENEMY_BASE,
			ATTR0_SHAPE(0) | ATTR0_8BPP | ATTR0_REG | ATTR0_Y(enemies.at(i).getY()),
			ATTR1_SIZE(!type) | ATTR1_X(enemies.at(i).getX()),
			ATTR2_ID8(startFrame));
			
	enemies.at(i).setDir(dir);
}

bool tryMove(int x, int y, int testX, int testY, Entity& entity){
	if(checkMapCollision(entity.getX()+x, entity.getY()+y, testX, testY) == 1){
		entity.move(entity.getX()+x, entity.getY()+y);
		return true;
	}else if(checkMapCollision(entity.getX()+x, entity.getY()+y, testX, testY) == 2){
		entity.setDead(true);
		return true;
	}
	return false;
}

int checkMapCollision(int x, int y, int testX, int testY){
	testY -= 1;
	if(GetTile(30,((x+testX)/8), ((y+testY)/8)) == 0){
		return 1;
	}
	if(GetTile(30, ((x+testX)/8), ((y+testY)/8)) >= 21 
	&& GetTile(30, ((x+testX)/8), ((y+testY)/8)) <= 25){
		return 1;
	}
	if(GetTile(30, ((x+testX)/8), ((y+testY)/8)) >= 16 
	&& GetTile(30, ((x+testX)/8), ((y+testY)/8)) <= 20){
		return 2;
	}
	return 0;
}

bool checkEntityCollision(Entity e1, Entity e2){
	int x1 = e1.getX();
	int x2 = e2.getX();
	int y1 = e1.getY();
	int y2 = e2.getY();
	
	bool xCollide = false;
	bool yCollide = false;

	if(x1 <= x2){
		if(x2 - x1 <= e1.getWidth()){
			xCollide = true;
		}
	}else{
		if(x1 - x2 <= e2.getWidth()){
			xCollide = true;
		}
	}
	
	if(y1 <= y2){
		if(y2 - y1 <= e1.getHeight()){
			yCollide = true;
		}
	}else{
		if(y1 - y2 <= e2.getHeight()){
			yCollide = true;
		}
	}
	
	if(xCollide && yCollide){
		return true;
	}
	
	return false;
}