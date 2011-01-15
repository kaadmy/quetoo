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

#include "client.h"


/*
 * Cl_ParseEntityBits
 *
 * Returns the entity number and the header bits
 */
unsigned int Cl_ParseEntityBits(unsigned int *bits){
	unsigned int b, total;
	unsigned int number;

	total = Msg_ReadByte(&net_message);
	if(total & U_MOREBITS1){
		b = Msg_ReadByte(&net_message);
		total |= b << 8;
	}
	if(total & U_MOREBITS2){
		b = Msg_ReadByte(&net_message);
		total |= b << 16;
	}
	if(total & U_MOREBITS3){
		b = Msg_ReadByte(&net_message);
		total |= b << 24;
	}

	if(total & U_NUMBER16)
		number = Msg_ReadShort(&net_message);
	else
		number = Msg_ReadByte(&net_message);

	*bits = total;

	return number;
}


/*
 * Cl_ParseDelta
 *
 * Can go from either a baseline or a previous packet_entity
 */
void Cl_ParseDelta(const entity_state_t *from, entity_state_t *to, int number, int bits){

	// set everything to the state we are delta'ing from
	*to = *from;

	if(!(from->effects & EF_BEAM))
		VectorCopy(from->origin, to->old_origin);

	to->number = number;

	if(bits & U_MODEL)
		to->model_index = Msg_ReadByte(&net_message);
	if(bits & U_MODEL2)
		to->model_index2 = Msg_ReadByte(&net_message);
	if(bits & U_MODEL3)
		to->model_index3 = Msg_ReadByte(&net_message);
	if(bits & U_MODEL4)
		to->model_index4 = Msg_ReadByte(&net_message);

	if(bits & U_FRAME)
		to->frame = Msg_ReadByte(&net_message);

	if(bits & U_SKIN8)
		to->skin_num = Msg_ReadByte(&net_message);
	else if(bits & U_SKIN16)
		to->skin_num = Msg_ReadShort(&net_message);

	if(bits & U_EFFECTS8)
		to->effects = Msg_ReadByte(&net_message);
	else if(bits & U_EFFECTS16)
		to->effects = Msg_ReadShort(&net_message);

	if(bits & U_ORIGIN1)
		to->origin[0] = Msg_ReadCoord(&net_message);
	if(bits & U_ORIGIN2)
		to->origin[1] = Msg_ReadCoord(&net_message);
	if(bits & U_ORIGIN3)
		to->origin[2] = Msg_ReadCoord(&net_message);

	if(bits & U_ANGLE1)
		to->angles[0] = Msg_ReadAngle(&net_message);
	if(bits & U_ANGLE2)
		to->angles[1] = Msg_ReadAngle(&net_message);
	if(bits & U_ANGLE3)
		to->angles[2] = Msg_ReadAngle(&net_message);

	if(bits & U_OLDORIGIN)
		Msg_ReadPos(&net_message, to->old_origin);

	if(bits & U_SOUND)
		to->sound = Msg_ReadByte(&net_message);

	if(bits & U_EVENT)
		to->event = Msg_ReadByte(&net_message);
	else
		to->event = 0;

	if(bits & U_SOLID)
		to->solid = Msg_ReadShort(&net_message);
}


/*
 * Cl_DeltaEntity
 *
 * Parses deltas from the given base and adds the resulting entity
 * to the current frame
 */
static void Cl_DeltaEntity(cl_frame_t *frame, int newnum, entity_state_t *old, int bits){
	cl_entity_t *ent;
	entity_state_t *state;

	ent = &cl.entities[newnum];

	state = &cl.entity_states[cl.entity_state & ENTITY_STATE_MASK];
	cl.entity_state++;
	frame->num_entities++;

	Cl_ParseDelta(old, state, newnum, bits);

	// some data changes will force no lerping
	if(state->model_index != ent->current.model_index
			|| state->model_index2 != ent->current.model_index2
			|| state->model_index3 != ent->current.model_index3
			|| state->model_index4 != ent->current.model_index4
			|| abs(state->origin[0] - ent->current.origin[0]) > 512
			|| abs(state->origin[1] - ent->current.origin[1]) > 512
			|| abs(state->origin[2] - ent->current.origin[2]) > 512
			|| state->event == EV_TELEPORT
	  ){
		ent->server_frame = -99;
	}

	if(ent->server_frame != cl.frame.server_frame - 1){
		// wasn't in last update, so initialize some things
		// duplicate the current state so lerping doesn't hurt anything
		ent->prev = *state;
		ent->lighting.dirty = true;
		VectorCopy(state->old_origin, ent->prev.origin);
	} else {  // shuffle the last state to previous
		ent->prev = ent->current;
	}

	ent->server_frame = cl.frame.server_frame;
	ent->current = *state;

	// bump animation time for frame interpolation
	if(ent->current.frame != ent->prev.frame){
		ent->anim_time = cl.time;
		ent->anim_frame = ent->prev.frame;
	}
}


/*
 * Cl_ParseEntities
 *
 * An svc_packetentities has just been parsed, deal with the rest of the data stream.
 */
static void Cl_ParseEntities(const cl_frame_t *old_frame, cl_frame_t *new_frame){
	unsigned int bits;
	entity_state_t *old_state = NULL;
	int old_index, old_num;

	new_frame->entity_state = cl.entity_state;
	new_frame->num_entities = 0;

	// delta from the entities present in old_frame
	old_index = 0;

	if(!old_frame)
		old_num = 99999;
	else {
		if(old_index >= old_frame->num_entities)
			old_num = 99999;
		else {
			old_state = &cl.entity_states[(old_frame->entity_state + old_index) & ENTITY_STATE_MASK];
			old_num = old_state->number;
		}
	}

	while(true){

		const int new_num = Cl_ParseEntityBits(&bits);

		if(new_num >= MAX_EDICTS){
			Com_Error(ERR_DROP, "Cl_ParseEntities: bad number: %i.\n", new_num);
		}

		if(net_message.read > net_message.size){
			Com_Error(ERR_DROP, "Cl_ParseEntities: end of message.\n");
		}

		if(!new_num)
			break;

		while(old_num < new_num){  // one or more entities from old_frame are unchanged

			if(cl_shownet->value == 3)
				Com_Print("   unchanged: %i\n", old_num);

			Cl_DeltaEntity(new_frame, old_num, old_state, 0);

			old_index++;

			if(old_index >= old_frame->num_entities)
				old_num = 99999;
			else {
				old_state = &cl.entity_states[(old_frame->entity_state + old_index) & ENTITY_STATE_MASK];
				old_num = old_state->number;
			}
		}

		if(bits & U_REMOVE){  // the entity present in the old_frame, and is not in the current frame

			if(cl_shownet->value == 3)
				Com_Print("   remove: %i\n", new_num);

			if(old_num != new_num)
				Com_Warn("Cl_ParseEntities: U_REMOVE: old_num != newnum.\n");

			old_index++;

			if(old_index >= old_frame->num_entities)
				old_num = 99999;
			else {
				old_state = &cl.entity_states[(old_frame->entity_state + old_index) & ENTITY_STATE_MASK];
				old_num = old_state->number;
			}
			continue;
		}

		if(old_num == new_num){  // delta from previous state

			if(cl_shownet->value == 3)
				Com_Print("   delta: %i\n", new_num);

			Cl_DeltaEntity(new_frame, new_num, old_state, bits);

			old_index++;

			if(old_index >= old_frame->num_entities)
				old_num = 99999;
			else {
				old_state = &cl.entity_states[(old_frame->entity_state + old_index) & ENTITY_STATE_MASK];
				old_num = old_state->number;
			}
			continue;
		}

		if(old_num > new_num){  // delta from baseline

			if(cl_shownet->value == 3)
				Com_Print("   baseline: %i\n", new_num);

			Cl_DeltaEntity(new_frame, new_num, &cl.entities[new_num].baseline, bits);
			continue;
		}

	}

	// any remaining entities in the old frame are copied over
	while(old_num != 99999){  // one or more entities from the old packet are unchanged

		if(cl_shownet->value == 3)
			Com_Print("   unchanged: %i\n", old_num);

		Cl_DeltaEntity(new_frame, old_num, old_state, 0);

		old_index++;

		if(old_index >= old_frame->num_entities)
			old_num = 99999;
		else {
			old_state = &cl.entity_states[(old_frame->entity_state + old_index) & ENTITY_STATE_MASK];
			old_num = old_state->number;
		}
	}
}


/*
 * Cl_ParsePlayerstate
 */
static void Cl_ParsePlayerstate(const cl_frame_t *old_frame, cl_frame_t *new_frame){
	player_state_t *ps;
	byte flags;
	int i;
	int statbits;

	ps = &new_frame->ps;

	// copy old value before delta parsing
	if(old_frame)
		*ps = old_frame->ps;
	else  // or start clean
		memset(ps, 0, sizeof(*ps));

	flags = Msg_ReadByte(&net_message);

	// parse the pmove_state_t

	if(flags & PS_M_TYPE)
		ps->pmove.pm_type = Msg_ReadByte(&net_message);

	if(cl.demo_server)
		ps->pmove.pm_type = PM_FREEZE;

	if(flags & PS_M_ORIGIN){
		ps->pmove.origin[0] = Msg_ReadShort(&net_message);
		ps->pmove.origin[1] = Msg_ReadShort(&net_message);
		ps->pmove.origin[2] = Msg_ReadShort(&net_message);
	}

	if(flags & PS_M_VELOCITY){
		ps->pmove.velocity[0] = Msg_ReadShort(&net_message);
		ps->pmove.velocity[1] = Msg_ReadShort(&net_message);
		ps->pmove.velocity[2] = Msg_ReadShort(&net_message);
	}

	if(flags & PS_M_TIME)
		ps->pmove.pm_time = Msg_ReadByte(&net_message);

	if(flags & PS_M_FLAGS)
		ps->pmove.pm_flags = Msg_ReadShort(&net_message);

	if(flags & PS_M_DELTA_ANGLES){
		ps->pmove.delta_angles[0] = Msg_ReadShort(&net_message);
		ps->pmove.delta_angles[1] = Msg_ReadShort(&net_message);
		ps->pmove.delta_angles[2] = Msg_ReadShort(&net_message);
	}

	if(flags & PS_VIEWANGLES){  // demo, chasecam, recording
		ps->angles[0] = Msg_ReadAngle16(&net_message);
		ps->angles[1] = Msg_ReadAngle16(&net_message);
		ps->angles[2] = Msg_ReadAngle16(&net_message);
	}

	// parse stats
	statbits = Msg_ReadLong(&net_message);
	for(i = 0; i < MAX_STATS; i++)
		if(statbits & (1 << i))
			ps->stats[i] = Msg_ReadShort(&net_message);
}


/*
 * Cl_EntityEvents
 */
static void Cl_EntityEvents(cl_frame_t *frame){
	int pnum;

	for(pnum = 0; pnum < frame->num_entities; pnum++){
		const int num = (frame->entity_state + pnum) & ENTITY_STATE_MASK;
		Cl_EntityEvent(&cl.entity_states[num]);
	}
}


/*
 * Cl_ParseFrame
 */
void Cl_ParseFrame(void){
	size_t len;
	cl_frame_t *old_frame;

	// TODO: remove this properly
	if(!cl.server_frame_rate){  // avoid unstable reconnects
		Com_Warn("Cl_ParseFrame: Unstable reconnect detected.\n");
		Cl_Reconnect_f();
		return;
	}

	cl.frame.server_frame = Msg_ReadLong(&net_message);
	cl.frame.server_time = cl.frame.server_frame * 1000 / cl.server_frame_rate;

	cl.frame.delta_frame = Msg_ReadLong(&net_message);

	cl.surpress_count = Msg_ReadByte(&net_message);

	if(cl_shownet->value == 3)
		Com_Print ("   frame:%i  delta:%i\n", cl.frame.server_frame, cl.frame.delta_frame);

	if(cl.frame.delta_frame <= 0){  // uncompressed frame
		cls.demo_waiting = false;
		cl.frame.valid = true;
		old_frame = NULL;
	}
	else {  // delta compressed frame
		old_frame = &cl.frames[cl.frame.delta_frame & UPDATE_MASK];

		if(!old_frame->valid)
			Com_Warn("Cl_ParseFrame: Delta from invalid frame.\n");
		else if(old_frame->server_frame != cl.frame.delta_frame)
			Com_Warn("Cl_ParseFrame: Delta frame too old.\n");
		else if(cl.entity_state - old_frame->entity_state > ENTITY_STATE_BACKUP - UPDATE_BACKUP)
			Com_Warn("Cl_ParseFrame: Delta parse_entities too old.\n");
		else
			cl.frame.valid = true;
	}

	len = Msg_ReadByte(&net_message); // read area_bits
	Msg_ReadData(&net_message, &cl.frame.area_bits, len);

	Cl_ParsePlayerstate(old_frame, &cl.frame);

	Cl_ParseEntities(old_frame, &cl.frame);

	// save the frame off in the backup array for later delta comparisons
	cl.frames[cl.frame.server_frame & UPDATE_MASK] = cl.frame;

	if(cl.frame.valid){
		// getting a valid frame message ends the connection process
		if(cls.state != ca_active){
			cls.state = ca_active;

			VectorCopy(cl.frame.ps.pmove.origin, cl.predicted_origin);
			VectorScale(cl.predicted_origin, 0.125, cl.predicted_origin);

			VectorCopy(cl.frame.ps.angles, cl.predicted_angles);
		}

		Cl_EntityEvents(&cl.frame); // fire entity events

		Cl_CheckPredictionError();
	}
}


/*
 * Cl_AddWeapon
 */
static void Cl_AddWeapon(const vec3_t shell){
	static r_entity_t ent;
	static r_lighting_t lighting;
	int w;

	if(!cl_weapon->value)
		return;

	if(!((int)cl_addentities->value & 2))
		return;

	if(cl_thirdperson->value)
		return;

	if(!cl.frame.ps.stats[STAT_HEALTH])
		return;  // deadz0r

	if(cl.frame.ps.stats[STAT_SPECTATOR])
		return;  // spectating

	if(cl.frame.ps.stats[STAT_CHASE])
		return;  // chasecam

	w = cl.frame.ps.stats[STAT_WEAPON];
	if(!w)  // no weapon, e.g. level intermission
		return;

	memset(&ent, 0, sizeof(ent));

	ent.flags = EF_WEAPON | EF_NO_SHADOW;

	VectorCopy(r_view.origin, ent.origin);
	VectorCopy(r_view.angles, ent.angles);

	VectorCopy(shell, ent.shell);

	ent.model = cl.model_draw[w];

	if(!ent.model)  // for development
		return;

	ent.lerp = 1.0;
	VectorSet(ent.scale, 1.0, 1.0, 1.0);

	ent.lighting = &lighting;
	ent.lighting->dirty = true;

	R_AddEntity(&ent);
}


static const vec3_t rocket_light = {
	1.0, 0.5, 0.3
};
static const vec3_t ctf_blue_light = {
	0.3, 0.3, 1.0
};
static const vec3_t ctf_red_light = {
	1.0, 0.3, 0.3
};
static const vec3_t quad_light = {
	0.3, 0.7, 0.7
};
static const vec3_t hyperblaster_light = {
	0.4, 0.7, 0.9
};
static const vec3_t lightning_light = {
	0.6, 0.6, 1.0
};
static const vec3_t bfg_light = {
	0.4, 1.0, 0.4
};

/*
 * Cl_AddEntities
 *
 * Iterate all entities in the current frame, adding models, particles,
 * coronas, and anything else associated with them.  Client-side animations
 * like bobbing and rotating are also resolved here.
 *
 * Entities with models are conditionally added to the view based on the
 * cl_addentities bitmask.
 */
void Cl_AddEntities(cl_frame_t *frame){
	r_entity_t ent;
	vec3_t start, end;
	vec3_t wshell;
	int pnum, mask;

	if(!cl_addentities->value)
		return;

	VectorClear(start);
	VectorClear(end);

	VectorClear(wshell);

	// resolve any models, animations, lerps, rotations, bobbing, etc..
	for(pnum = 0; pnum < frame->num_entities; pnum++){

		entity_state_t *state = &cl.entity_states[(frame->entity_state + pnum) & ENTITY_STATE_MASK];

		cl_entity_t *cent = &cl.entities[state->number];

		// beams have two origins, most ents have just one
		if(state->effects & EF_BEAM){

			// skin_num is overridden to specify owner of the beam
			if((state->skin_num == cl.player_num + 1) && !cl_thirdperson->value){
				// we own this beam (lightning, grapple, etc..)
				// project start position in front of view origin
				VectorCopy(r_view.origin, start);
				VectorMA(start, 24.0, r_view.forward, start);
				VectorMA(start, 6.0, r_view.right, start);
				start[2] -= 10.0;
			}
			else  // or simply lerp the start position
				VectorLerp(cent->prev.origin, cent->current.origin, cl.lerp, start);

			VectorLerp(cent->prev.old_origin, cent->current.old_origin, cl.lerp, end);
		}
		else {  // for most ents, just lerp the origin
			VectorLerp(cent->prev.origin, cent->current.origin, cl.lerp, ent.origin);
		}

		// bob items, shift them to randomize the effect in crowded scenes
		if(state->effects & EF_BOB)
			ent.origin[2] += 4.0 * sin((cl.time * 0.005) + ent.origin[0] + ent.origin[1]);

		// calculate angles
		if(state->effects & EF_ROTATE){  // some bonus items rotate
			ent.angles[0] = 0.0;
			ent.angles[1] = cl.time / 3.4;
			ent.angles[2] = 0.0;
		}
		else {  // lerp angles
			AngleLerp(cent->prev.angles, cent->current.angles, cl.lerp, ent.angles);
		}

		// lerp frames
		if(state->effects & EF_ANIMATE)
			ent.frame = cl.time / 500;
		else if(state->effects & EF_ANIMATE_FAST)
			ent.frame = cl.time / 100;
		else
			ent.frame = state->frame;

		ent.old_frame = cent->anim_frame;

		ent.lerp = (cl.time - cent->anim_time) / 100.0;

		if(ent.lerp < 0.0)  // clamp
			ent.lerp = 0.0;
		else if(ent.lerp > 1.0)
			ent.lerp = 1.0;

		ent.back_lerp = 1.0 - ent.lerp;

		VectorSet(ent.scale, 1.0, 1.0, 1.0);  // scale

		// resolve model and skin
		if(state->model_index == 255){  // use custom player skin
			const cl_clientinfo_t *ci = &cl.clientinfo[state->skin_num & 0xff];
			ent.skin_num = 0;
			ent.skin = ci->skin;
			ent.model = ci->model;
			if(!ent.skin || !ent.model){
				ent.skin = cl.baseclientinfo.skin;
				ent.model = cl.baseclientinfo.model;
			}
			VectorSet(ent.scale, PM_SCALE, PM_SCALE, PM_SCALE);
		} else {
			ent.skin_num = state->skin_num;
			ent.skin = NULL;
			ent.model = cl.model_draw[state->model_index];
		}

		if(state->effects & (EF_ROCKET | EF_GRENADE)){
			Cl_SmokeTrail(cent->prev.origin, ent.origin, cent);
		}

		if(state->effects & EF_ROCKET){
			R_AddCorona(ent.origin, 3.0, 0.25, rocket_light);
			R_AddLight(ent.origin, 1.5, rocket_light);
		}

		if(state->effects & EF_HYPERBLASTER){
			R_AddCorona(ent.origin, 12.0, 0.15, hyperblaster_light);
			R_AddLight(ent.origin, 1.25, hyperblaster_light);

			Cl_EnergyTrail(cent, 10.0, 107);
		}

		if(state->effects & EF_LIGHTNING){
			vec3_t dir;
			Cl_LightningTrail(start, end);

			R_AddLight(start, 1.25, lightning_light);
			VectorSubtract(end, start, dir);
			VectorNormalize(dir);
			VectorMA(end, -12.0, dir, end);
			R_AddLight(end, 1.0 + (0.1 * crand()), lightning_light);
		}

		if(state->effects & EF_BFG){
			R_AddCorona(ent.origin, 24.0, 0.05, bfg_light);
			R_AddLight(ent.origin, 1.5, bfg_light);

			Cl_EnergyTrail(cent, 20.0, 206);
		}

		VectorClear(ent.shell);

		if(state->effects & EF_CTF_BLUE){
			R_AddLight(ent.origin, 1.0, ctf_blue_light);
			VectorCopy(ctf_blue_light, ent.shell);
		}

		if(state->effects & EF_CTF_RED){
			R_AddLight(ent.origin, 1.0, ctf_red_light);
			VectorCopy(ctf_red_light, ent.shell);
		}

		if(state->effects & EF_QUAD){
			R_AddLight(ent.origin, 1.0, quad_light);
			VectorCopy(quad_light, ent.shell);
		}

		if(state->effects & EF_TELEPORTER)
			Cl_TeleporterTrail(ent.origin, cent);

		// if there's no model associated with the entity, we're done
		if(!state->model_index)
			continue;

		// filter by model type
		mask = ent.model && ent.model->type == mod_bsp_submodel ? 1 : 2;

		if(!((int)cl_addentities->value & mask))
			continue;

		// don't draw ourselves unless third person is set
		if(state->number == cl.player_num + 1){

			// retain our shell effect for view model
			VectorCopy(ent.shell, wshell);

			if(!cl_thirdperson->value)
				continue;
		}

		ent.flags = state->effects;

		// setup the write-through lighting cache
		ent.lighting = &cent->lighting;

		if(r_view.update){  // new lighting information is available
			memset(ent.lighting, 0, sizeof(*(ent.lighting)));
			ent.lighting->dirty = true;
		}

		if(ent.skin)  // always re-light players
			ent.lighting->dirty = true;

		if(!VectorCompare(cent->current.origin, cent->prev.origin))
			ent.lighting->dirty = true;  // or anything that moves

		if(ent.flags & EF_NO_LIGHTING){  // except projectiles
			ent.lighting->dirty = false;

			VectorClear(ent.lighting->point);
			VectorSet(ent.lighting->color, 1.0, 1.0, 1.0);
		}

		// add to view list
		R_AddEntity(&ent);

		/*
		 * Check for linked models; these start with the origin and angles
		 * of the owning entity.  There are a few special cases here to
		 * accommodate visual weapons, CTF flags, etc..
		 */
		ent.skin = NULL;
		ent.flags = 0;

		if(state->model_index2){
			if(state->model_index2 == 255){  // custom weapon
				// the weapon is masked on the skin_num
				const cl_clientinfo_t *ci = &cl.clientinfo[state->skin_num & 0xff];
				int i = (state->skin_num >> 8);  // 0 is default weapon model
				if(i > MAX_WEAPONMODELS - 1)
					i = 0;
				ent.model = ci->weaponmodel[i];
				ent.flags = state->effects;
			} else
				ent.model = cl.model_draw[state->model_index2];

			R_AddEntity(&ent);
		}
		if(state->model_index3){  // ctf flags
			vec3_t fwd, rgt;
			float f;

			// project back and to the right
			AngleVectors(ent.angles, fwd, rgt, NULL);
			VectorMA(ent.origin, -8.0 * PM_SCALE, fwd, ent.origin);
			VectorMA(ent.origin, 3.0 * PM_SCALE, rgt, ent.origin);

			f = sin(cl.time * 0.004) * 0.5;
			ent.origin[0] += f;
			ent.origin[1] += f;
			ent.origin[2] += 2.0;

			ent.angles[2] += 15.0;

			ent.frame = ent.old_frame = 0;
			ent.model = cl.model_draw[state->model_index3];

			VectorSet(ent.scale, 0.6, 0.6, 0.6);
			VectorScale(ent.scale, PM_SCALE, ent.scale);

			R_AddEntity(&ent);
		}
		if(state->model_index4){
			ent.model = cl.model_draw[state->model_index4];
			R_AddEntity(&ent);
		}
	}

	// lastly, add the view weapon
	Cl_AddWeapon(wshell);
}
