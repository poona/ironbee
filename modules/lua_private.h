/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

/**
 * @file
 * @brief IronBee --- LUA Module
 *
 * This module integrates with luajit, allowing lua modules to be loaded.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#ifndef __MODULES__LUA_H
#define __MODULES__LUA_H

#include "lua/ironbee.h"

#include <ironbee/core.h>
#include <ironbee/lock.h>
#include <ironbee/module.h>
#include <ironbee/release.h>
#include <ironbee/resource_pool.h>


/**
 * Module configuration.
 */
struct modlua_cfg_t {
    char               *pkg_path;      /**< Package path Lua Configuration. */
    char               *pkg_cpath;     /**< Cpath Lua Configuration. */
    ib_list_t          *reloads;       /**< modlua_reload_t list. */
    ib_resource_pool_t *lua_pool;      /**< Pool of Lua stacks. */
    ib_lock_t           lua_pool_lock; /**< Pool lock. */
    ib_resource_t      *lua_resource;  /**< Resource modlua_cfg_t::L. */
    lua_State          *L;             /**< Lua stack used for config. */
};
typedef struct modlua_cfg_t modlua_cfg_t;

/**
 * Get the @ref modlua_cfg_t configuration from the configuration context.
 *
 * @param[in] ib IronBee engine.
 * @param[in] ctx The configuration context.
 * @param[out] cfg The configuration stored in the context.
 *
 * @returns
 * - IB_OK On success.
 * - Failure codes for ib_context_module_config() or ib_engine_module_get().
 */
ib_status_t modlua_cfg_get(
    ib_engine_t *ib,
    ib_context_t *ctx,
    modlua_cfg_t **cfg);

/**
 * Called by modlua_module_load to load the lua script into the Lua runtime.
 *
 * If @a is_config_time is true, then this will also register
 * configuration directives and handle them.
 *
 * @param[in] ib IronBee engine.
 * @param[in] is_config_time Is this executing at configuration time.
 *            If yes, then effects on the ironbee engine may be performed,
 *            such as rules and directives.
 * @param[in] file The file we are loading.
 * @param[in] module The registered module structure.
 * @param[in,out] L The lua context that @a file will be loaded into as
 *                @a module.
 * @returns
 *   - IB_OK on success.
 */
ib_status_t modlua_module_load_lua(
    ib_engine_t *ib,
    bool is_config_time,
    const char *file,
    ib_module_t *module,
    lua_State *L
);

/**
 * Push a Lua table onto the stack that contains a path of configurations.
 *
 * IronBee supports nested configuration contexts. Configuration B may
 * occur in configuration A. This function will push
 * the Lua table { "A", "B" } such that t[1] = "A" and t[2] = "B".
 *
 * This allows the module to fetch or build the configuration table
 * required to store any user configurations to be done.
 *
 * Lazy configuration table creation is done to avoid a large, unused
 * memory footprint in situations of simple global Lua module configurations
 * but hundreds of sites, each of which has no unique configuration.
 *
 * This is also used when finding the closest not-null configuration to
 * pass to a directive handler.
 *
 * @param[in] ib IronBee Engine.
 * @param[in] ctx The current / starting context.
 * @param[in,out] L Lua stack onto which is pushed the table of configurations.
 *
 * @returns
 *   - IB_OK is currently always returned.
 */
ib_status_t modlua_push_config_path(
    ib_engine_t *ib,
    ib_context_t *ctx,
    lua_State *L
);
#endif /* __MODULES__LUA_H */
