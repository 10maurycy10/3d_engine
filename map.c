// Structures and functions for working with 3d geometry
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "map.h"

// Allocated a room with capacity for n Rooms
struct Room* allocate_room(int length) {
	struct Room* room = malloc(sizeof(struct Room) + sizeof(struct WallVertex) * length);
	room->length = length;
	return room;
}

struct Map* allocate_map(int length) {
	struct Map* map = malloc(sizeof(struct Map) + sizeof(struct Room*) * length);
	map->length = length;
	return map;
}

void free_room(struct Room* room) {
	free(room);
}

void free_map(struct Map* map) {
	for (int i = 0; i < map->length; i++) {
		printf("Room Freed\n");
		free_room(map->rooms[i]);
	}
	printf("Map Freed\n");
	free(map);
}

struct Map* new_test_map() {
	struct Room* room1 = allocate_room(4);
	room1->walls[0].location.x = 1;
	room1->walls[0].location.y = 2;

	room1->walls[1].location.x = 1;
	room1->walls[1].location.y = -2;
	
	room1->walls[2].location.x = -1;
	room1->walls[2].location.y = -2;
	
	room1->walls[3].location.x = -1;
	room1->walls[3].location.y = 2;
	
	room1->walls[0].r = 255;
	room1->walls[0].g = 0;
	room1->walls[0].b = 0;

	room1->walls[1].r = 0;
	room1->walls[1].g = 255;
	room1->walls[1].b = 0;

	room1->walls[2].r = 0;
	room1->walls[2].g = 0;
	room1->walls[2].b = 255;

	room1->walls[3].r = 255;
	room1->walls[3].g = 255;
	room1->walls[3].b = 0;

	struct Map* map = allocate_map(1);
	map->rooms[0] = room1;
	return map;
}

// TODO Sainly handle errors insead of exiting.
struct Map* load_map_from_file(FILE* file) {
	char* line;
	int length;
	int read;

	struct Map* map = NULL;
	int next_room = 0;
	struct Room* room = NULL;
	int next_wall = 0;

	// For every line in the file...
	while (1) {
		line = NULL;
		read = getline(&line, &length, file);
		printf("%s", line);
		if (read == -1) break;

		// Check for prefixes
		if (!strncmp("#", line, strlen("#"))) {
			// Comment, ignore.
		} else if (!strncmp("MAP ", line, strlen("MAP "))) {
			// Ensure a map has not already been created
			assert(!map);
			int map_size;
			assert(sscanf(line, "MAP %d\n", &map_size) == 1);
			map = allocate_map(map_size);
		} else if (!strncmp("ROOM ", line, strlen("ROOM "))) {
			// Ensure there is space in the map
			assert(map);
			assert(map->length != next_room);
			// Ensure the last room was filled
			if (room) assert(room->length == next_wall);

			// Allocate the room
			int room_size;
			assert(sscanf(line, "ROOM %d\n", &room_size) == 1);
			room = allocate_room(room_size);

			// Append room to the map
			map->rooms[next_room++] = room;	
			next_wall = 0;
		} else if (!strncmp("WALL ", line, strlen("WALL "))) {
			// Ensure there is space in the current room
			assert(room);
			assert(room->length != next_wall);
			
			// Add the wall
			float x, y;
			int r,g,b;
			assert(sscanf(line, "WALL %f %f %d %d %d\n", &x, &y, &r, &g, &b) == 5);
			room->walls[next_wall].r = r;
			room->walls[next_wall].g = g;
			room->walls[next_wall].b = b;
			room->walls[next_wall].portal_idx = -1;
			room->walls[next_wall].location.x = x;
			room->walls[next_wall].location.y = y;
			next_wall++; 
		} else if (!strncmp("PORTAL ", line, strlen("PORTAL "))) {
			// Ensure there is space in the current room
			assert(room);
			assert(room->length != next_wall);
			
			// Add the portal
			float x, y;
			int portalidx;
			assert(sscanf(line, "PORTAL %f %f %d\n", &x, &y, &portalidx) == 3);
			room->walls[next_wall].r = 255;
			room->walls[next_wall].g = 0;
			room->walls[next_wall].b = 255;

			room->walls[next_wall].portal_idx = portalidx;
			room->walls[next_wall].location.x = x;
			room->walls[next_wall].location.y = y;
			next_wall++; 
		} else {
			printf("Got garbage in map file:\n");
			printf("%s", line);
			return NULL;
		}
		free(line);

	}

	// Insure map was filled
	assert(map);
	assert(map->length == next_room);

	// Ensure room was filled
	if (room) assert(room->length == next_wall);

	// All done!
	return map;
}
