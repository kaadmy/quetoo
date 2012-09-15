/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __CL_TYPES_H__
#define __CL_TYPES_H__

#include "renderer/r_types.h"
#include "sound/s_types.h"
#include "ui/ui_types.h"

typedef struct cl_frame_s {
	bool valid; // cleared if delta parsing was invalid
	uint32_t server_frame;
	uint32_t server_time; // server time the message is valid for (in milliseconds)
	int32_t delta_frame; // negatives indicate no delta
	byte area_bits[MAX_BSP_AREAS >> 3]; // portal area visibility bits
	player_state_t ps;
	uint16_t num_entities;
	uint32_t entity_state; // non-masked index into cl.entity_states array
} cl_frame_t;

typedef struct cl_entity_animation_s {
	entity_animation_t animation;
	uint32_t time;
	uint16_t frame;
	uint16_t old_frame;
	float lerp;
	float fraction;
} cl_entity_animation_t;

typedef struct cl_entity_s {
	entity_state_t baseline; // delta from this if not from a previous frame
	entity_state_t current;
	entity_state_t prev; // will always be valid, but might just be a copy of current

	uint32_t server_frame; // if not current, this entity isn't in the frame

	uint32_t time; // for intermittent effects

	cl_entity_animation_t animation1;
	cl_entity_animation_t animation2;

	r_lighting_t lighting; // cached static lighting info
} cl_entity_t;

typedef struct cl_client_info_s {
	char info[MAX_QPATH]; // the full info string, e.g. newbie\qforcer/blue
	char name[MAX_QPATH]; // the player name, e.g. newbie
	char model[MAX_QPATH]; // the model name, e.g. qforcer
	char skin[MAX_QPATH]; // the skin name, e.g. blue

	r_model_t *head;
	r_image_t *head_skins[MD3_MAX_MESHES];

	r_model_t *upper;
	r_image_t *upper_skins[MD3_MAX_MESHES];

	r_model_t *lower;
	r_image_t *lower_skins[MD3_MAX_MESHES];
} cl_client_info_t;

#define CMD_BACKUP 512 // allow a lot of command backups for very fast systems
#define CMD_MASK (CMD_BACKUP - 1)

// we accumulate parsed entity states in a rather large buffer so that they
// may be safely delta'd in the future
#define ENTITY_STATE_BACKUP (UPDATE_BACKUP * MAX_PACKET_ENTITIES)
#define ENTITY_STATE_MASK (ENTITY_STATE_BACKUP - 1)

// the cl_client_s structure is wiped completely at every map change
typedef struct cl_client_s {
	uint32_t time_demo_frames;
	uint32_t time_demo_start;

	uint32_t frame_counter;
	uint32_t packet_counter;
	uint32_t byte_counter;

	user_cmd_t cmds[CMD_BACKUP]; // each message will send several old cmds
	uint32_t cmd_time[CMD_BACKUP]; // time sent, for calculating pings

	vec_t predicted_step;
	uint32_t predicted_step_time;

	vec3_t predicted_origin; // generated by Cl_PredictMovement
	vec3_t predicted_offset;
	vec3_t predicted_angles;
	vec3_t prediction_error;
	struct g_edict_s *predicted_ground_entity;
	int16_t predicted_origins[CMD_BACKUP][3]; // for debug comparing against server

	cl_frame_t frame; // received from server
	cl_frame_t frames[UPDATE_BACKUP]; // for calculating delta compression

	cl_entity_t entities[MAX_EDICTS]; // client entities

	entity_state_t entity_states[ENTITY_STATE_BACKUP]; // accumulated each frame
	uint32_t entity_state; // index (not wrapped) into entity states

	uint16_t player_num; // our entity number

	uint32_t surpress_count; // number of messages rate suppressed

	uint32_t time; // this is the server time value that the client
	// is rendering at. always <= cls.real_time due to latency

	float lerp; // linear interpolation between frames

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame. It is cleared to 0 upon entering each level.
	// the server sends a delta when necessary which is added to the locally
	// tracked view angles to account for spawn and teleport direction changes
	vec3_t angles;

	uint32_t server_count; // server identification for precache
	uint16_t server_hz; // server frame rate (packets per second)

	bool demo_server; // we're viewing a demo
	bool third_person; // we're using a 3rd person camera

	char config_strings[MAX_CONFIG_STRINGS][MAX_STRING_CHARS];

	// locally derived information from server state
	r_model_t *model_draw[MAX_MODELS];
	c_model_t *model_clip[MAX_MODELS];

	s_sample_t *sound_precache[MAX_SOUNDS];
	r_image_t *image_precache[MAX_IMAGES];

	cl_client_info_t client_info[MAX_CLIENTS];
} cl_client_t;

// the client_static_t structure is persistent through an arbitrary
// number of server connections

typedef enum {
	CL_UNINITIALIZED, CL_DISCONNECTED, // not talking to a server
	CL_CONNECTING, // sending request packets to the server
	CL_CONNECTED, // netchan_t established, waiting for svc_server_data
	CL_ACTIVE
// game views should be displayed
} cl_state_t;

typedef enum {
	KEY_GAME, KEY_UI, KEY_CONSOLE, KEY_CHAT
} cl_key_dest_t;

#define KEY_HISTORY_SIZE 64
#define KEY_LINE_SIZE 256

typedef enum {
	SDLK_MOUSE1 = SDLK_LAST,
	SDLK_MOUSE2,
	SDLK_MOUSE3,
	SDLK_MOUSE4,
	SDLK_MOUSE5,
	SDLK_MOUSE6,
	SDLK_MOUSE7,
	SDLK_MOUSE8,
	SDLK_MLAST
} SDLButton;

typedef struct cl_key_state_s {
	cl_key_dest_t dest;

	char lines[KEY_HISTORY_SIZE][KEY_LINE_SIZE];
	uint16_t pos;

	bool insert;
	bool repeat;

	uint32_t edit_line;
	uint32_t history_line;

	char *binds[SDLK_MLAST];
	bool down[SDLK_MLAST];
} cl_key_state_t;

typedef struct cl_mouse_state_s {
	float x, y;
	float old_x, old_y;
	bool grabbed;
} cl_mouse_state_t;

typedef struct cl_chat_state_s {
	char buffer[KEY_LINE_SIZE];
	size_t len;
	bool team;
} cl_chat_state_t;

typedef struct cl_download_s {
	bool http;
	FILE *file;
	char tempname[MAX_OSPATH];
	char name[MAX_OSPATH];
} cl_download_t;

typedef enum {
	SERVER_SOURCE_INTERNET, SERVER_SOURCE_USER, SERVER_SOURCE_BCAST
} cl_server_source_t;

typedef struct cl_server_info_s {
	net_addr_t addr;
	cl_server_source_t source;
	char hostname[64];
	char name[32];
	char gameplay[32];
	uint16_t clients;
	uint16_t max_clients;
	uint32_t ping_time;
	uint16_t ping;
	struct cl_server_info_s *next;
} cl_server_info_t;

#define MAX_SERVER_INFOS 128

typedef struct cl_static_s {
	cl_state_t state;

	cl_key_state_t key_state;

	cl_mouse_state_t mouse_state;

	cl_chat_state_t chat_state;

	uint32_t real_time; // always increasing, no clamping, etc

	uint32_t packet_delta; // milliseconds since last outgoing packet
	uint32_t render_delta; // milliseconds since last renderer frame

	// connection information
	char server_name[MAX_OSPATH]; // name of server to connect to
	uint32_t connect_time; // for connection retransmits

	net_chan_t netchan; // network channel

	uint32_t challenge; // from the server to use for connecting
	uint32_t spawn_count;

	uint16_t loading; // loading percentage indicator

	char download_url[MAX_OSPATH]; // for http downloads
	cl_download_t download; // current download (udp or http)

	char demo_path[MAX_OSPATH];
	FILE *demo_file;

	cl_server_info_t *servers; // list of servers from all sources
	char *servers_text; // tabular data for servers menu

	uint32_t broadcast_time; // time when last broadcast ping was sent

	struct cg_export_s *cgame;
} cl_static_t;

#endif /* __CL_TYPES_H__ */
