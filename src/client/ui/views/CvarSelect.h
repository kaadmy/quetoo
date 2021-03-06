/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
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

#pragma once

#include <ObjectivelyMVC/Select.h>

#include "cvar.h"

/**
 * @file
 *
 * @brief A Select exposing a cvar_t.
 */

typedef struct CvarSelect CvarSelect;
typedef struct CvarSelectInterface CvarSelectInterface;

/**
 * @brief The CvarSelect type.
 *
 * @extends Select
 */
struct CvarSelect {
	
	/**
	 * @brief The parent.
	 *
	 * @private
	 */
	Select select;
	
	/**
	 * @brief The typed interface.
	 *
	 * @private
	 */
	CvarSelectInterface *interface;

	/**
	 * @brief The variable.
	 */
	cvar_t *var;

	/**
	 * @brief Set to true if the variable expects a string value, false for integer.
	 */
	_Bool expectsStringValue;
};

/**
 * @brief The CvarSelect interface.
 */
struct CvarSelectInterface {
	
	/**
	 * @brief The parent interface.
	 */
	SelectInterface selectInterface;
	
	/**
	 * @fn CvarSelect *CvarSelect::initWithVariable(CvarSelect *self, cvar_t *var)
	 *
	 * @brief Initializes this Select with the given variable.
	 *
	 * @param var The variable.
	 *
	 * @return The initialized CvarSelect, or `NULL` on error.
	 *
	 * @memberof CvarSelect
	 */
	CvarSelect *(*initWithVariable)(CvarSelect *self, cvar_t *var);
};

/**
 * @brief The CvarSelect Class.
 */
extern Class _CvarSelect;

