class Entity{
protected:
	int x;
	int y;
	int width;
	int height;
	bool dir;
	bool dead;
	int frame;
	
public:
	void move(int x, int y){
		this->x = x;
		this->y = y;
	}
	
	int getX(){
		return x;
	}
	
	int getY(){
		return y;
	}
	
	int getWidth(){
		return width;
	}
	
	int getHeight(){
		return height;
	}
	
	bool getDir(){
		return dir;
	}
	
	void setDir(bool direction){
		dir = direction;
	}
	
	bool isDead(){
		return dead;
	}
	
	void setDead(bool dead){
		this->dead = dead;
	}
	
	int getFrame(){
		return frame;
	}

};


class Player: public Entity{
private:	
	bool running;
	bool jumping;
	bool landed;
	int jumpStartY;
	int jumpHeight;
	int state;
	
public:
	Player(int x, int y){
		this->x = x;
		this->y = y;
		width = 8;
		height = 8;
		dir = true;
		running = false;
		jumping = false;
		state = 0;
		dead = false;
		frame = 0;
	}
	
	~Player(){
	}
	
	void update(){
		running = false;
	
		if(jumping){
			if(jumpHeight >= 50)jumpHeight = 50;
			if(y <= jumpStartY - jumpHeight || y <= 5){
				jumping = false;
				landed = false;
			}
		}
	}
	
	void jump(){
		jumping = true;
		landed = false;
		jumpHeight = 0;
		jumpStartY = y;
	}
	
	bool getRunning(){
		return running;
	}
	
	void setRunning(bool run){
		running = run;
	}
	
	void setLanded(bool landed){
		this->landed = landed;
	}
	
	bool getLanded(){
		return landed;
	}
	
	bool getJumping(){
		return jumping;
	}
	
	void setJumping(bool jump){
		jumping = jump;
	}
	
	int getJumpHeight(){
		return jumpHeight;
	}
	
	void setJumpHeight(int height){
		jumpHeight = height;
	}
	
	int getState(){
		return state;
	}
	
	void setState(int state){
		this->state = state;
	}

	void updateFrame(){
		frame++;
		switch(state){
			case 0:if(frame > 4)frame = 0;
					break;
			case 1:if(frame > 9 || frame < 5)frame = 5;
					break;
			case 2:if(frame > 13 || frame < 10)frame = 11;
					break;
		}
	}

};

class Monster: public Entity{
private:	
	bool size;
	bool angry;
	int health;
	
public:
	Monster(int x, int y, bool size){
		this->x = x;
		this->y = y;
		this->size = size;
		if(size){
			width = 8;
			height = 8;
			health = 2;
		}else{
			width = 16;
			height = 16;
			health = 5;
		}
		angry = false;
		dir = true;
		dead = false;
		frame = 0;
	}
	
	~Monster(){
	}
	
	void hurt(int amount){
		health -= amount;
		if(health <= 0)dead = true;
	}
	
	int getHealth(){
		return health;
	}
	
	bool getSize(){
		return size;
	}
	
	bool getAngry(){
		return angry;
	}
	
	//Out of the fire and back in at the top, red, faster and at full health
	void enrage(int x, int y){
		this->x = x;
		this->y = y;
		angry = true;
		dead = false;
		health = size ? 2 : 5;
	}
	
	void updateFrame(){
		if(size){
			frame++;
			if(frame > 21|| frame < 16)frame = 16;	
		}else {
			frame+=2;
			if(frame > 38|| frame < 32)frame = 32;
		}
	}

};

class Bullet: public Entity{
private:	
	int type;
	int originWeapon;
	int lift;
	int charger;
	int chargeTime;
	int speed;		//grenades: sideways pixels per frame, slowed by bounces
	int hitMask;	//katana: monsters this swing has already struck
	bool bounce;
public:
	Bullet(int x, int y, int type, int weapon){
		this->x = x;
		this->y = y;
		this->type = type;
		originWeapon = weapon;
		lift = 0;
		charger = 0;
		speed = 2;
		hitMask = 0;
		if(type == 6)chargeTime = 60;
		else if(type == 5)chargeTime = 90;	//grenade fuse, a second and a half
		else if(type == 8)chargeTime = 8;	//a katana swing is brief
		else if(type == 9)chargeTime = 12;	//blast debris burns out quickly
		else if(type == 10)chargeTime = 18;	//flames burn out after ~40px
		else chargeTime = 15;
		bounce = false;
		width = 4;
		height = 4;
		//The slash reaches well in front of the player and a little above,
		//so it meets a monster before the monster meets the player
		if(type == 8){
			width = 16;
			height = 12;
		}else if(type == 10){
			width = 6;
			height = 6;
		}
		dead = false;
		//The slash borrows the blade sprite and debris the flame frames;
		//tiles 48 and up belong to the monsters
		if(type == 8)frame = 31;
		else if(type == 9)frame = 290;	//FLAME_TILE
		else if(type == 10)frame = 290;	//FLAME_TILE, drawn at start-up
		else frame = 40+type;
	}
	
	~Bullet(){
	}
	
	int getType(){
		return type;
	}

	void setType(int type){
		this->type = type;
	}
	
	int getDamage(){
		if(type == 0 || type == 4 || type == 5 || type == 6 || type == 7)return 5;
		if(type == 1 || type == 3)return 1;
		if(type == 2)return 3;
		if(type == 8)return 3;	//one swing kills a small monster, two a large one
		if(type == 10)return 1;	//two flames for a small monster, five a large
		return 0;
	}
	
	int getOriginWeapon(){
		return originWeapon;
	}
	
	int getLift(){
		return lift;
	}
	
	void setLift(int lift){
		this->lift = lift;
	}
	
	bool charge(){
		charger++;
		if(charger == chargeTime)return true;
		return false;
	}
	
	int getCharger(){
		return charger;
	}
	
	int getSpeed(){
		return speed;
	}
	
	void setSpeed(int speed){
		this->speed = speed;
	}
	
	//True the first time this swing reaches monster i, false after that
	bool strike(int i){
		if(hitMask & (1 << i))return false;
		hitMask |= (1 << i);
		return true;
	}
	
	bool getBounce(){
		return bounce;
	}
	
	void setBounce(bool bounce){
		this->bounce = bounce;
	}
};

class Crate: public Entity{
private:	
	int weapon;
public:
	Crate(int x, int y, int weapon){
		this->x = x;
		this->y = y;
		this->weapon = weapon;
		width = 8;
		height = 8;
		dead = true;
		frame = 14;
	}
	
	~Crate(){
	}
	
	int getWeapon(){
		return weapon;
	}

	void setWeapon(int weapon){
		this->weapon = weapon;
	}
};