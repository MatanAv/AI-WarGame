#include "glut.h"
#include <time.h>
#include <vector>
#include <queue>
#include <iostream>
#include "Player.h"
#include "Soldier.h"
#include "Supporter.h"

using namespace std;

Room* rooms[NUM_ROOMS];
Room* med_rooms[NUM_MED_ROOMS];
Room* ammo_rooms[NUM_AMMO_ROOMS];

int maze[MSZ][MSZ];
double security_map[MSZ][MSZ] = {0};

void InitMaze();
void InitRooms();
void InitObstacles();
void DigTunnels();
void CreateSubRooms();
void CreateSecurityMap();
void InitTeams();

Bullet* pb = nullptr;
Bullet* pre_explosion = nullptr;
Grenade* pg = nullptr;

Player* red_team[NUM_PLAYERS];
Player* blue_team[NUM_PLAYERS];
vector<Player*> reds_vec;
vector<Soldier*> red_soldiers;
vector<Player*> blues_vec;
vector<Soldier*> blue_soldiers;
Supporter* red_supporter;
Supporter* blue_supporter;

int player_turn = 0;

bool startGame = false;

void init()
{
	glClearColor(0.3, 0.3, 0.3, 0);// color of window background
	//           RED GREEN BLUE

	glOrtho(0, MSZ, 0, MSZ, -1, 1);
	//     left right bottom top near, far

	srand(time(0)); // initialize random seed

	InitMaze();
	InitRooms();
	InitObstacles();
	DigTunnels();
	CreateSubRooms();	// set random med-kit & ammo rooms

	for (int i = 0; i < NUM_SECURITY_MAP_SIMULATIONS; i++)
		CreateSecurityMap();

	InitTeams();	// Placing the teams
}

void FillRoom(int index) 
{
	int i, j;
	int cr, cc, w, h;

	w = rooms[index]->getW();
	h = rooms[index]->getH();
	cr = rooms[index]->getCenterRow();
	cc = rooms[index]->getCenterCol();

	for (i = cr - h / 2; i <= cr + h / 2; i++)
		for (j = cc - w / 2; j <= cc + w / 2; j++)
			maze[i][j] = SPACE;
}

bool hasOverlapping(int index,int w, int h, int crow, int ccol)
{
	int i;
	int dx, dy;
	bool overlap = false;
	for (i = 0; i < index && !overlap; i++)
	{
		dx = abs(ccol - rooms[i]->getCenterCol());
		dy = abs(crow - rooms[i]->getCenterRow());
		if (dx <w/2 + rooms[i]->getW()/2 +3 && 
				dy< h/2 +rooms[i]->getH()/2+3)
			overlap = true;
	}
	return overlap;
}

void InitRooms() 
{
	int crow, ccol, w, h;
	for (int i = 0; i < NUM_ROOMS; i++)
	{
		// init a room
		do {
			w = MIN_ROOM_WIDTH + rand() % (MAX_ROOM_WIDTH - MIN_ROOM_WIDTH);
			h = MIN_ROOM_HEIGHT + rand() % (MAX_ROOM_HEIGHT - MIN_ROOM_HEIGHT);
			crow = 2 + h / 2 + rand() % (MSZ - (5 + h));
			ccol = 2 + w / 2 + rand() % (MSZ - (5 + w));
		} while (hasOverlapping(i, w, h, crow, ccol));
		rooms[i] = new Room(crow, ccol, w, h);
		FillRoom(i);
	}
}

void InitObstacles()
{
	// set random obstacles
	for (int i = 0; i < 400; i++)
	{
		int x = rand() % MSZ, y = rand() % MSZ;
		// Validate it isn't a border
		if (x == 0) x += 1;
		if (x == MSZ - 1) x -= 1;
		if (y == 0) y += 1;
		if (y == MSZ - 1) y -= 1;

		// Making sure it won't block a tunnel
		if (maze[y + 1][x] == SPACE && maze[y - 1][x] == SPACE
			|| maze[y][x + 1] == SPACE && maze[y][x - 1] == SPACE)
			maze[y][x] = WALL;
	}
}

void InitMaze() 
{
	int i, j;
	// setup inner space of maze
	for(i=0;i<MSZ;i++)
		for (j = 0; j < MSZ ; j++)
			maze[i][j] = WALL; 


}

void RefillStock()
{
	// Put Ammo & Med-kits
	for (int i = 0; i < NUM_MED_ROOMS; i++)
		if (maze[med_rooms[i]->getCenterRow()][med_rooms[i]->getCenterCol()] == SPACE)
			maze[med_rooms[i]->getCenterRow()][med_rooms[i]->getCenterCol()] = MED;

	for (int i = 0; i < NUM_AMMO_ROOMS; i++)
		if (maze[ammo_rooms[i]->getCenterRow()][ammo_rooms[i]->getCenterCol()] == SPACE)
			maze[ammo_rooms[i]->getCenterRow()][ammo_rooms[i]->getCenterCol()] = AMMO;
}

void CreateSubRooms()
{
	// We would like to have only one sub room in a super room at most
	bool subroom_loc[NUM_ROOMS] = { 0 };
	int totalSubRooms = NUM_MED_ROOMS + NUM_AMMO_ROOMS;

	for (int i = 0; i < totalSubRooms; i++)
	{
		int r;
		do {
			r = rand() % NUM_ROOMS;
		} while (subroom_loc[r]);

		subroom_loc[r] = true;	// Room is containing a sub room
		int crow, ccol;
		crow = rooms[r]->getCenterRow();
		ccol = rooms[r]->getCenterCol();

		//	Fill Borders
		for (int j = crow - SUB_ROOM_SIZE / 2; j <= crow + SUB_ROOM_SIZE / 2; j++)
		{
			maze[j][ccol - SUB_ROOM_SIZE / 2] = WALL;
			maze[j][ccol + SUB_ROOM_SIZE / 2] = WALL;
		}
		for (int j = ccol - SUB_ROOM_SIZE / 2; j < ccol + SUB_ROOM_SIZE / 2; j++)
		{
			if (j == ccol)	// Making sure there's no obstacle blocking the door
			{
				maze[crow - SUB_ROOM_SIZE / 2 - 1][j] = SPACE;
				maze[crow - SUB_ROOM_SIZE / 2][j] = SPACE;
				maze[crow - SUB_ROOM_SIZE / 2 + 1][j] = SPACE;
			}
			else
				maze[crow - SUB_ROOM_SIZE / 2][j] = WALL;
			maze[crow + SUB_ROOM_SIZE / 2][j] = WALL;
		}

		if (i < NUM_MED_ROOMS)
			med_rooms[i] = new Room(crow, ccol, SUB_ROOM_SIZE, SUB_ROOM_SIZE);
		else
			ammo_rooms[i - NUM_MED_ROOMS] = new Room(crow, ccol, SUB_ROOM_SIZE, SUB_ROOM_SIZE);
	}

	RefillStock();
}

void PlaceInRooms(int r, bool isRedTeam)
{
	for (int i = 0; i < NUM_PLAYERS; i++)
	{
		int x, y, range;
		if (isRedTeam) range = NUM_PLAYERS + 2;
		else range = NUM_PLAYERS * 3;

		do {
			if (i % 2 == 0)	// Randomizing places in a room
			{
				x = rooms[r]->getCenterCol() + rand() % range;
				y = abs(rooms[r]->getCenterRow() - rand() % range);
			}
			else
			{
				x = abs(rooms[r]->getCenterCol() - rand() % range);
				y = rooms[r]->getCenterRow() + rand() % range;
			}
		} while (maze[y][x] != SPACE);

		if (isRedTeam)
		{
			red_team[i]->setRow(y);
			red_team[i]->setCol(x);
			red_team[i]->setRoomNumber(r);

			if (i < NUM_SOLDIERS)
				maze[y][x] = RED_SOLDIER;
			else
				maze[y][x] = RED_SUPPORTER;
		}
		else
		{
			blue_team[i]->setRow(y);
			blue_team[i]->setCol(x);
			blue_team[i]->setRoomNumber(r);

			if (i < NUM_SOLDIERS)
				maze[y][x] = BLUE_SOLDIER;
			else
				maze[y][x] = BLUE_SUPPORTER;
		}
	}
}

void InitTeams()
{
	// Creating the players
	for (int i = 0; i < NUM_PLAYERS; i++)
	{
		if (i < NUM_SOLDIERS)
		{
			if (i < NUM_AGGRESSIVE_SOLDIERS)
			{
				red_team[i] = new Soldier(RED, i + 1, AGGRESSIVE);
				blue_team[i] = new Soldier(BLUE, i + 1, AGGRESSIVE);

			}
			else
			{
				red_team[i] = new Soldier(RED, i + 1, COVERING);
				blue_team[i] = new Soldier(BLUE, i + 1, COVERING);
			}

			red_soldiers.push_back((Soldier*)red_team[i]);
			blue_soldiers.push_back((Soldier*)blue_team[i]);
		}
		else
		{
			red_team[i] = new Supporter(RED, i + 1);
			blue_team[i] = new Supporter(BLUE, i + 1);
			red_supporter = (Supporter*)red_team[i];
			blue_supporter = (Supporter*)blue_team[i];
		}

		reds_vec.push_back(red_team[i]);
		blues_vec.push_back(blue_team[i]);
	}

	// Placing them in a rooms
	int r = rand() % NUM_ROOMS;
	PlaceInRooms(r, true);
	r = rand() % NUM_ROOMS;
	PlaceInRooms(r, false);
}


void RestorePath(Cell* pCurrent,int start_row, int start_col)
{
	
//	while (!(current.getRow() == start_row && current.getCol() == start_col))
	while(pCurrent->getParent()!=nullptr)
	{
		if (maze[pCurrent->getRow()][pCurrent->getCol()] == WALL)
			maze[pCurrent->getRow()][pCurrent->getCol()] = SPACE;
		pCurrent = pCurrent->getParent();
	}
}
// who is the neighbor at nrow ncol? If it is white then paint it gray and add to pq
// If it is gray then check two cases: 
//	1. if F of this neighbor is now better then what was before then we need to update the neighbor
//	2. if it is not better then do nothing
// If it is black do nothing
// If it is Target then we have two cases (actually this is one of the previous cases, white or gray):
//	1. if F of this target is now better then what was before then we need to update the target
//	2. if it is not better then do nothing
void CheckNeighbor(Cell* pCurrent, int nrow, int ncol,
	priority_queue <Cell, vector<Cell>, CompareCells> &pq,
	vector <Cell> &grays, vector <Cell> &blacks)
{
	vector<Cell>::iterator it_gray;
	vector<Cell>::iterator it_black;
	double space_cost = 0.1, wall_cost = 2,cost;
	if (maze[nrow][ncol] == SPACE)
		cost = space_cost;
	else cost = wall_cost;
	// this is the neighbor
	Cell* pn = new Cell(nrow, ncol, pCurrent->getTargetRow(), pCurrent->getTargetCol(),
		pCurrent->getG()+cost, pCurrent);
	// check the color of this neighbor
	it_black = find(blacks.begin(), blacks.end(), *pn);
	if (it_black != blacks.end()) // it was found i.e. it is black
		return;
	// white
	it_gray = find(grays.begin(), grays.end(), *pn);
	if (it_gray == grays.end()) // it wasn't found => it is white
	{
		grays.push_back(*pn); // paint it gray
		pq.push(*pn);
	}
	else // it is gray
	{
		//   new F       ?      old F
		if (pn->getF() < it_gray->getF()) // then update it (F of neighbor)!!!
		{
			// we need toupdate it in two places:
			// 1. in vector grays
			it_gray->setG(pn->getG()); 
			it_gray->setF(pn->getF());
			// 2. in PQ
			// to update a Cell in pq we need to remove it from pq, to update it and to push it back
			vector<Cell> tmp;
			Cell tmpCell; 
			do
			{
				if (pq.empty())
				{
					cout << "ERROR! PQ is empty in update PQ\n";
					exit(1);
				}
				tmpCell = pq.top();
				pq.pop();
				if(!(tmpCell == (*pn))) // do not copy the Cell we were looking for to tmp!(see line 173) 
					tmp.push_back(tmpCell);
			} while (!(tmpCell==(*pn)));
			// now we are aout of do-while because we have found the neighbor in PQ. So change it to *pn.
			pq.push(*pn);
			// now push back all the elements that are in tmp
			while (!tmp.empty())
			{
				pq.push(tmp.back());
				tmp.pop_back();
			}
		}
	}
}

// runs A* from room i to room j
void DigPath(int i, int j) 
{
	int row, col;
	vector <Cell> grays;
	vector <Cell> blacks;
	priority_queue <Cell,vector<Cell>,CompareCells> pq;
	Cell* pstart = new Cell(rooms[i]->getCenterRow(), rooms[i]->getCenterCol(),
		rooms[j]->getCenterRow(), rooms[j]->getCenterCol(), 0, nullptr);

	Cell* pCurrent;
	bool targetFound = false;
	// initializes grays and pq
	grays.push_back(*pstart);
	pq.push(*pstart);
	vector<Cell>::iterator it_gray;

	while (!pq.empty())
	{
		pCurrent = new Cell(pq.top());
		pq.pop();
		// If current is actually a target then we stop A*
		if (pCurrent->getRow() == rooms[j]->getCenterRow() &&
			pCurrent->getCol() == rooms[j]->getCenterCol()) // then it is target
		{ //in this case there cannot be a better path to target!!!
			RestorePath(pCurrent, rooms[i]->getCenterRow(), rooms[i]->getCenterCol());
			return;
		}
		// paint current black
		blacks.push_back(*pCurrent);
		it_gray = find(grays.begin(), grays.end(), *pCurrent); // we have to define operator ==
		if (it_gray != grays.end()) // current was found
			grays.erase(it_gray); // and removed from grays
		// now check the neighbors of current
		row = pCurrent->getRow();
		col = pCurrent->getCol();
		// try to go UP (row -1)
		if (row > 0) // we can go UP
			CheckNeighbor(pCurrent, row - 1, col, pq, grays, blacks);

		if(row<MSZ-1) // DOWN
			CheckNeighbor(pCurrent, row + 1, col, pq, grays, blacks);

		if (col < MSZ - 1) // RIGHT
			CheckNeighbor(pCurrent, row , col+ 1, pq, grays, blacks);

		if (col > 0)//LEFT
			CheckNeighbor(pCurrent, row, col - 1, pq, grays, blacks);

	}
}

void DigTunnels() 
{
	int i, j;
	for (i = 0; i < NUM_ROOMS; i++)
		for (j = i + 1; j < NUM_ROOMS; j++)
		{
			DigPath(i, j); // A*
			cout << "The path from room " << i << " to room " << j << " has been digged\n";
		}
}

void ShowMaze() 
{
	int i, j;
	for(i=0;i<MSZ;i++)
		for(j=0;j<MSZ;j++)
		{
			switch (maze[i][j]) 
			{
			case WALL: 
				glColor3d(0, 0, 0);	// set color black
				break;
			case SPACE:
				glColor3d(1-security_map[i][j], 1 - security_map[i][j], 1 - security_map[i][j]);
				break;
			case MED:
				//1.00 green 0.43 blue 0.78
				//glColor3d(0.8, 0.3, 0.6);	// set color pink
				glColor3d(0.5, 1, 0.5);	// set color pink
				break;
			case AMMO:
				glColor3d(0.623529, 0.673529, 0.272549);// set color khaki
				break;
			case RED_SOLDIER:
				glColor3d(1, 0, 0);	// set color RED
				break;
			case RED_SUPPORTER:
				glColor3d(1, 0.6, 0.6);	// set color LIGHT RED
				break;
			case BLUE_SOLDIER:
				glColor3d(0, 0.25, 1);	// set color BLUE
				break;
			case BLUE_SUPPORTER:
				glColor3d(0.3, 0.7, 1);	// set color LIGHT BLUE
				break;
			case DEAD:
				glColor3d(0.33, 0.33, 0.33);	// set color GRAY
				break;
			} // switch
			// now show the cell of maze
			glBegin(GL_POLYGON);
			glVertex2d(j,i); // left-bottom vertex
			glVertex2d(j,i+1); // left-top vertex
			glVertex2d(j+1,i+1); // right-top vertex
			glVertex2d(j+1,i); // right-bottom vertex
			glEnd();
		}
}




void CreateSecurityMap()
{
	int num_simulations = 1000;
	double damage = 0.001;
	int x, y;

	for (int i = 0; i < num_simulations; i++)
	{
		x = rand() % MSZ;
		y = rand() % MSZ;
		Grenade* g = new Grenade(x, y);
		g->SimulateExplosion(maze, security_map, damage);
	}
}

bool CheckIfCellOnTarget(int row, int col, int trow, int tcol)
{
	Bullet* b = new Bullet(col, row, CalculateDirectedAngle(row, col, trow, tcol));
	b->setIsFired(true);

	return b->SimulateFireOnTarget(maze, trow, tcol);
}

void CreateVisibillityMapForPlayer(Soldier* sd, bool visibillity_map[MSZ][MSZ])
{
	Room room = *rooms[sd->getRoomNumber()];
	int enemy_team = sd->getTeamColor() ? RED : BLUE;	// if this player color is blue --> enemy = RED
	int sold_color, supp_color;
	
	if (enemy_team == RED)
	{
		sold_color = RED_SOLDIER;
		supp_color = RED_SUPPORTER;
	}
	else
	{
		sold_color = BLUE_SOLDIER;
		supp_color = BLUE_SUPPORTER;
	}

	for (int i = room.getCenterRow() - room.getH() / 2; i < room.getCenterRow() + room.getH() / 2; i++)
		for (int j = room.getCenterCol() - room.getW() / 2; j < room.getCenterCol() + room.getW() / 2; j++)
			if (maze[i][j] == SPACE || maze[i][j] == sold_color || maze[i][j] == supp_color
				|| maze[i][j] == MED || maze[i][j] == AMMO)
				visibillity_map[i][j] = CheckIfCellOnTarget(sd->getRow(), sd->getCol(), i, j);
}

bool IsStockEmpty()
{
	for (int i = 0; i < NUM_MED_ROOMS; i++)
		if (maze[med_rooms[i]->getCenterRow()][med_rooms[i]->getCenterCol()] == MED)
			return false;

	for (int i = 0; i < NUM_AMMO_ROOMS; i++)
		if (maze[ammo_rooms[i]->getCenterRow()][ammo_rooms[i]->getCenterCol()] == AMMO)
			return false;

	return true;
}

bool CheckInsideARoom(Player* p, Room* r)
{
	return p->getRow() <= r->getCenterRow() + (r->getH() / 2) &&
		p->getRow() >= r->getCenterRow() - (r->getH() / 2) && 
		p->getCol() <= r->getCenterCol() + (r->getW() / 2) &&
		p->getCol() >= r->getCenterCol() - (r->getW() / 2);
}

void UpdatePlayerRoom(Player* p)
{
	p->setRoomNumber(-1);	// First, resetting player's room to -1
	// Now check each room, if the player is in the room it will be updated, else -> -1 means in tunnel
	for (int i = 0; i < NUM_ROOMS && p->getRoomNumber() == -1; i++)
		if (CheckInsideARoom(p, rooms[i]))
		{
			p->setRoomNumber(i);
			return;
		}
}

// In case every aggressive soldiers are dead -> change any cautious soldier to aggressive
void ChangeCautiousToAggressive(vector<Soldier*> team_soldiers)
{
	for (auto& s : team_soldiers)
		if (s->getSoldierType() == AGGRESSIVE)
			return;

	team_soldiers.front()->setSoldierType(AGGRESSIVE);
}

void MakeSupporterStep(Supporter* sp, vector <Soldier*>& teammates, vector<Player*> enemies)
{
	int prev_row = sp->getRow(), prev_col = sp->getCol();

	sp->CalculateTask(enemies, teammates);

	if (sp->getTask() == SUPPORTER_BATTLE_MODE)
		sp->BattleMode(maze, security_map, teammates);	// Choosing battle-task

	// Perform supporter task
	switch (sp->getTask())
	{
		case FILL_MEDKIT_STOCK:
			sp->FillMedkitStock(maze, med_rooms, security_map);
			break;
		case FILL_AMMO_STOCK:
			sp->FillAmmoStock(maze, med_rooms, security_map);
			break;
		case PROVIDE_MED_TO_URGENT:
		{
			for (auto& s : teammates)
				if (sp->getSoldierProvided() == s->getId())
				{
					
					break;
				}
			break;
		}
		case PROVIDE_AMMO_TO_URGENT:
		{
			for (auto& s : teammates)
				if (sp->getSoldierProvided() == s->getId())
				{
					sp->ProvideAmmoToSoldier(maze, s, security_map);
					break;
				}
			break;
		}
		case USE_MEDKIT:
			sp->UseMedkit();
			break;
		case FOLLOW_TEAMMATES:
			sp->FollowTeammates(maze, teammates, security_map);
			break;
		case HIDE:
			sp->Hide(maze, security_map);
			break;
		case RUN_AWAY:
			sp->RunAway(maze, rooms, security_map);
			break;
	}
	
	// Update map
	if (sp->getRow() != prev_row || sp->getCol() != prev_col)
	{
		if (sp->getTeamColor() == RED)
			maze[sp->getRow()][sp->getCol()] = RED_SUPPORTER;
		else
			maze[sp->getRow()][sp->getCol()] = BLUE_SUPPORTER;

		maze[prev_row][prev_col] = SPACE;
	}
}

void MakeSoldierStep(Soldier* sd, vector <Soldier*>& team_soldiers, Supporter* team_supporter, vector <Player*>& enemies)
{
	int prev_row = sd->getRow(), prev_col = sd->getCol();
	bool visibillity_map[MSZ][MSZ] = { 0 };

	// Task Choosing
	sd->CalculateTask(maze, security_map, rooms, enemies, visibillity_map, team_supporter);
	if (sd->getTask() == SOLDIER_BATTLE_MODE)
	{
		CreateVisibillityMapForPlayer(sd, visibillity_map);
		sd->BattleMode(maze, security_map, rooms, enemies, visibillity_map);	// Choosing battle-task
	}

	// Task Execution
	switch (sd->getTask())
	{
		case CALL_FOR_MEDKIT:
			sd->CallForMedkit();
			break;
		case CALL_FOR_AMMO:
			sd->CallForAmmo();
			break;
		case MOVE_TO_SUPPORTER:
			sd->MoveOnTowardsSupporter(maze, team_supporter, security_map);
			break;
		case FOLLOW_AGGRESSIVE_TEAMMATE:
			sd->FollowAggressiveTeammate(maze, team_soldiers, security_map);
			break;
		case SEARCH_ENEMIES:
			sd->SearchTheEnemies(maze, rooms, enemies, security_map);
			break;
		case GET_CLOSER_TO_ENEMY:
			sd->GetCloserToEnemy(maze, security_map);
			break;
		case RUN_AWAY:
			sd->RunAway(maze, rooms, security_map);
			break;
		case SHOOT_BULLET:
			pb = sd->ShootBullet();
			break;
		case THROW_GRENADE:
			pre_explosion = sd->ThrowGrenade();
			break;
		case HIDE:
			sd->Hide(maze, security_map);
			break;
	}

	// Update Map
	if (sd->getRow() != prev_row || sd->getCol() != prev_col)
	{
		if (sd->getTeamColor() == RED)
			maze[sd->getRow()][sd->getCol()] = RED_SOLDIER;
		else
			maze[sd->getRow()][sd->getCol()] = BLUE_SOLDIER;

		maze[prev_row][prev_col] = SPACE;
	}
}

void HandleBulletHit(Bullet* pb)
{
	int h_row = pb->getHitRow(), h_col = pb->getHitCol();

	if (h_row > -1)	// Bullet hits a player
	{
		if (pb->getShooterTeam() == BLUE)
		{
			for (int i = 0; i < NUM_PLAYERS; i++)
				if (red_team[i]->getRow() == h_row && red_team[i]->getCol() == h_col)
				{
					red_team[i]->setHP(red_team[i]->getHP() - pb->getHitDamage());
					cout << "Player " << red_team[i]->getId() << " from red team got hit by a bullet!\n";
					if (red_team[i]->getHP() <= 0)
					{
						red_team[i]->setIsAlive(false);
						maze[red_team[i]->getRow()][red_team[i]->getCol()] = DEAD;
						cout << "Player " << red_team[i]->getId() << " from red team is dead!\n";
					}
					return;
				}
		}
		else
		{
			for (int i = 0; i < NUM_PLAYERS; i++)
				if (blue_team[i]->getRow() == h_row && blue_team[i]->getCol() == h_col)
				{
					blue_team[i]->setHP(blue_team[i]->getHP() - pb->getHitDamage());
					cout << "Player " << blue_team[i]->getId() << " from blue team got hit by a bullet!\n";
					if (blue_team[i]->getHP() <= 0)
					{
						blue_team[i]->setIsAlive(false);
						maze[blue_team[i]->getRow()][blue_team[i]->getCol()] = DEAD;
						cout << "Player " << blue_team[i]->getId() << " from blue team is dead!\n";
					}
					return;
				}
		}
	}
}

void HandleGrenadeHit(Grenade* pg)
{
	Bullet** g_bullets = pg->getBullets();

	for (int i = 0; i < NUM_BULLETS; i++)
		if (!g_bullets[i]->getIsFired())
			HandleBulletHit(g_bullets[i]);
}

// Move bullets
void HandleBulletsFiring()
{
	if (pb && pb->getIsFired())
	{
		pb->move(maze);
		HandleBulletHit(pb);
		if (!pb->getIsFired())
			pb = nullptr;
	}
}
// Move grenades
void HandleGrenadeExplosion()
{
	if (pre_explosion != nullptr && pre_explosion->getIsFired())
	{
		pre_explosion->moveGrenadeBullet(maze);
		if (!pre_explosion->getIsFired())
		{
			pg = new Grenade(pre_explosion->getX(), pre_explosion->getY(), pre_explosion->getShooterTeam());
			pg->setIsExploded(true);
			pre_explosion = nullptr;
		}
	}
	else if (pg && pg->getIsExploded())
	{
		pg->explode(maze);
		HandleGrenadeHit(pg);
		if (!pg->getIsExploded())
			pg = nullptr;
	}
}

void HandleAttacks()
{
	HandleBulletsFiring();
	HandleGrenadeExplosion();
}

void CheckIfAnyTeamWins()
{
	bool anyRedAlive = false;
	bool anyBlueAlive = false;

	for (int i = 0; i < NUM_PLAYERS; i++)
	{
		if (!anyRedAlive)
			anyRedAlive = red_team[i]->getIsAlive();
		if (!anyBlueAlive)
			anyBlueAlive = blue_team[i]->getIsAlive();
		if (anyRedAlive && anyBlueAlive)
			return;
	}

	if (!anyRedAlive && !anyBlueAlive)
	{
		cout << "Game is finished in draw!";
		startGame = false;
		return;
	}
	else if (!anyRedAlive)
	{
		cout << "Blue team have won the game!";
		startGame = false;
		return;
	}
	else
	{
		cout << "Red team have won the game!";
		startGame = false;
		return;
	}
}

void HandlePlayersTurns()
{
	Player* p = nullptr;

	switch (player_turn)
	{
	case 0:	// Red Soldier (aggressive)
		p = red_team[0];	
		if (p->getIsAlive())
			MakeSoldierStep((Soldier*)p, red_soldiers, red_supporter, blues_vec);
		break;
	case 1:	// Blue Soldier (aggressive)
		p = blue_team[0];	
		if (p->getIsAlive())
			MakeSoldierStep((Soldier*)p, blue_soldiers, blue_supporter, reds_vec);
		break;
	case 2:	// Red Soldier (cautious)
		p = red_team[1];	
		if (p->getIsAlive())
			MakeSoldierStep((Soldier*)p, red_soldiers, red_supporter, blues_vec);
		break;
	case 3:	// Blue Soldier (cautious)
		p = blue_team[1];	
		if (p->getIsAlive())
			MakeSoldierStep((Soldier*)p, blue_soldiers, blue_supporter, reds_vec);
		break;
	case 4:	// Red Supporter
		p = red_supporter;	
		if (p->getIsAlive())
			MakeSupporterStep(red_supporter, red_soldiers, blues_vec);
		break;
	case 5:	// Blue Supporter
		p = blue_supporter;	
		if (p->getIsAlive())
			MakeSupporterStep(blue_supporter, blue_soldiers, reds_vec);
		break;
	}

	if (p != nullptr) UpdatePlayerRoom(p);

	player_turn == 5 ? player_turn = 0 : player_turn++;
}

void RunGameFlow()
{
	HandlePlayersTurns();

	if (IsStockEmpty())
		RefillStock();

	CheckIfAnyTeamWins();
}

void UpdatePlayersState()
{
	vector<Player*> new_reds;
	vector<Player*> new_blues;
	vector<Soldier*> new_red_soldiers;
	vector<Soldier*> new_blue_soldiers;

	for (int i = 0; i < NUM_PLAYERS; i++)
	{
		if (red_team[i]->getIsAlive())
			new_reds.push_back(red_team[i]);
		if (blue_team[i]->getIsAlive())
			new_blues.push_back(blue_team[i]);
	}

	for (auto& sd : red_soldiers)
		if (sd->getIsAlive())
			new_red_soldiers.push_back(sd);
	for (auto& sd : blue_soldiers)
		if (sd->getIsAlive())
			new_blue_soldiers.push_back(sd);

	if (!new_red_soldiers.empty())
		ChangeCautiousToAggressive(new_red_soldiers);
	else
		red_supporter->setIsLastSurvivor(true);

	if (!new_blue_soldiers.empty())
		ChangeCautiousToAggressive(new_blue_soldiers);
	else
		blue_supporter->setIsLastSurvivor(true);

	reds_vec = new_reds;
	blues_vec = new_blues;
	red_soldiers = new_red_soldiers;
	blue_soldiers = new_blue_soldiers;
}


void display()
{
	glClear(GL_COLOR_BUFFER_BIT); // clean frame buffer

	ShowMaze();

	// Show Bullet
	if (pb != nullptr)
		pb->show();

	// Show Pre-Explosion
	if (pre_explosion != nullptr)
		pre_explosion->show();

	// Show Grenade
	if (pg != nullptr)
		pg->show();

	glutSwapBuffers(); // show all
}

// runs all the time in the background
void idle()
{
	//if (pb || pre_explosion || pg)
	if (pb || pg)
		HandleAttacks();
	else if (startGame)
		RunGameFlow();

	UpdatePlayersState();

	Sleep(10);

	glutPostRedisplay(); // indirect call to display
}

void menu(int choice) 
{
	if (choice == 1)
		startGame = true;
}

void mouse(int button, int state, int x, int y) 
{
}

void main(int argc, char* argv[])
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(600, 600);
	glutInitWindowPosition(200, 100);
	glutCreateWindow("Dungeon");

	glutDisplayFunc(display);
	glutIdleFunc(idle);
	glutMouseFunc(mouse);
	// add menu
	glutCreateMenu(menu);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
	glutAddMenuEntry("Start Game", 1);

	init();

	glutMainLoop();
}