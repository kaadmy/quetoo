/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

#include <SDL.h>

#include "renderer.h"

/*
 * R_SetIcon
 */
static void R_SetIcon(void){
	SDL_Surface *surf;

	if(!Img_LoadImage("pics/icon", &surf))
		return;

	SDL_WM_SetIcon(surf, NULL);

	SDL_FreeSurface(surf);
}


static int desktop_w, desktop_h;  // desktop resolution

/*
 * R_InitContext
 */
qboolean R_InitContext(int width, int height, qboolean fullscreen){
	unsigned flags;
	int i;
	SDL_Surface *surface;

	if(SDL_WasInit(SDL_INIT_EVERYTHING) == 0){
		if(SDL_Init(SDL_INIT_VIDEO) < 0){
			Com_Warn("R_InitContext: %s.\n", SDL_GetError());
			return false;
		}
	} else if(SDL_WasInit(SDL_INIT_VIDEO) == 0){
		if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0){
			Com_Warn("R_InitContext: %s.\n", SDL_GetError());
			return false;
		}
	}

	if(!desktop_w){  // first time through, resolve desktop resolution

		const SDL_VideoInfo *vid = SDL_GetVideoInfo();

		desktop_w = vid->current_w;
		desktop_h = vid->current_h;
	}

	if(!width)
		width = desktop_w;

	if(!height)
		height = desktop_h;

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	i = (int)r_multisample->value;

	if(i < 0)
		i = 0;
	if(i > 4)
		i = 4;

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, i ? 1 : 0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, i);

	i = (int)r_swapinterval->value;
	if(i < 0)
		i = 0;
	if(i > 2)
		i = 2;

	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, i);

	flags = SDL_OPENGL;

	if(fullscreen)
		flags |= SDL_FULLSCREEN;

	if((surface = SDL_SetVideoMode(width, height, 0, flags)) == NULL)
		return false;

	r_state.width = surface->w;
	r_state.height = surface->h;
	r_state.fullscreen = (surface->flags & SDL_FULLSCREEN);

	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r_state.redbits);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &r_state.greenbits);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &r_state.bluebits);
	SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &r_state.alphabits);

	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &r_state.depthbits);
	SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &r_state.doublebits);

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
			SDL_DEFAULT_REPEAT_INTERVAL);

	SDL_WM_SetCaption("Quake2World", "Quake2World");

	SDL_EnableUNICODE(1);

	if(fullscreen)
		SDL_ShowCursor(false);

	R_SetIcon();

	return true;
}


/*
 * R_ShutdownContext
 */
void R_ShutdownContext(void){
	if(SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
}