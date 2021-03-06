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

#include "lua_private.h"

#include "lua/ironbee.h"

#include "lua_modules_private.h"
#include "lua_rules_private.h"
#include "lua_runtime_private.h"

#include <ironbee/array.h>
#include <ironbee/cfgmap.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/escape.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>
#include <ironbee/queue.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 *  * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* -- Module Setup -- */

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        lua
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Define the public module symbol. */
IB_MODULE_DECLARE();

/**
 * Callback type for functions executed protected by global lock.
 *
 * This callback should take a @c ib_engine_t* which is used
 * for logging, a @c lua_State* which is used to create the
 * new thread, and a @c lua_State** which will be assigned a
 * new @c lua_State*.
 */
typedef ib_status_t(*critical_section_fn_t)(ib_engine_t *ib,
                                            lua_State *parent,
                                            lua_State **out_new);

/* -- Lua Routines -- */

#define IB_FFI_MODULE_WRAPPER     _IRONBEE_CALL_MODULE_HANDLER
#define IB_FFI_MODULE_WRAPPER_STR IB_XSTRINGIFY(IB_FFI_MODULE_WRAPPER)

#define IB_FFI_MODULE_CFG_WRAPPER     _IRONBEE_CALL_CONFIG_HANDLER
#define IB_FFI_MODULE_CFG_WRAPPER_STR IB_XSTRINGIFY(IB_FFI_MODULE_CFG_WRAPPER)

#define IB_FFI_MODULE_EVENT_WRAPPER     _IRONBEE_CALL_EVENT_HANDLER
#define IB_FFI_MODULE_EVENT_WRAPPER_STR IB_XSTRINGIFY(IB_FFI_MODULE_EVENT_WRAPPER)

/**
 * Evaluate the Lua stack and report errors about directive processing.
 *
 * @param[in] L Lua state.
 * @param[in] ib IronBee engine.
 * @param[in] module The Lua module structure.
 * @param[in] name The function. This is used for logging only.
 * @param[in] args_in The number of arguments to the Lua function being called.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC If Lua interpretation fails with an LUA_ERRMEM error.
 *   - IB_EINVAL on all other failures.
 */
static ib_status_t modlua_config_cb_eval(
    lua_State *L,
    ib_engine_t *ib,
    ib_module_t *module,
    const char *name,
    int args_in)
{
    int lua_rc = lua_pcall(L, args_in, 1, 0);
    ib_status_t rc;
    switch(lua_rc) {
        case 0:
            /* NOP */
            break;
        case LUA_ERRRUN:
            ib_log_error(
                ib,
                "Error processing call for module %s: %s",
                module->name,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            return IB_EINVAL;
        case LUA_ERRMEM:
            ib_log_error(
                ib,
                "Failed to allocate memory processing call for %s",
                module->name);
            return IB_EALLOC;
        case LUA_ERRERR:
            ib_log_error(
                ib,
                "Error fetching error message during call for %s",
                module->name);
            return IB_EINVAL;
#if LUA_VERSION_NUM > 501
        /* If LUA_ERRGCMM is defined, include a custom error for it as well.
          This was introduced in Lua 5.2. */
        case LUA_ERRGCMM:
            ib_log_error(
                ib,
                "Garbage collection error during call for %s.",
                module->name);
            return IB_EINVAL;
#endif
        default:
            ib_log_error(
                ib,
                "Unexpected error(%d) during call %s for %s: %s",
                lua_rc,
                name,
                module->name,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            return IB_EINVAL;
    }

    if (!lua_isnumber(L, -1)) {
        ib_log_error(ib, "Directive handler did not return integer.");
        rc = IB_EINVAL;
    }
    else {
        rc = lua_tonumber(L, -1);
    }

    lua_pop(L, 1);

    return rc;
}

ib_status_t modlua_push_config_path(
    ib_engine_t *ib,
    ib_context_t *ctx,
    lua_State *L)
{
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(L != NULL);

    int table;

    lua_createtable(L, 10, 0); /* Make a list to store our context names in. */
    table = lua_gettop(L);     /* Store where in the stack it is. */

    /* Until the main context is found, push the name of this ctx and fetch. */
    while (ctx != ib_context_main(ib)) {
        lua_pushstring(L, ib_context_name_get(ctx));
        ctx = ib_context_parent_get(ctx);
    }

    /* Push the main context's name. */
    lua_pushstring(L, ib_context_name_get(ctx));

    /* While there is a string on the stack, append it to the table. */
    for (int i = 1; lua_isstring(L, -1); ++i) {
        lua_pushinteger(L, i);  /* Insert k. */
        lua_insert(L, -2);      /* Make the stack [table, ..., k, v]. */
        lua_settable(L, table); /* Set t[k] = v. */
    }

    return IB_OK;
}

/**
 * @param[in] cp Configuration parser.
 * @param[in] name Directive name for the block that is being closed.
 * @param[in,out] cbdata Callback data.
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC on memory errors in the Lua VM.
 *   - IB_EINVAL if lua evaluation fails.
 */
static ib_status_t modlua_config_cb_blkend(
    ib_cfgparser_t *cp,
    const char *name,
    void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(name != NULL);

    ib_status_t rc;
    lua_State *L = NULL;
    modlua_cfg_t *cfg = NULL;
    ib_module_t *module = NULL;
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not find module \"" MODULE_NAME_STR ".\"");
        return rc;
    }

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    rc = ib_context_module_config(ctx, module, &cfg);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve module configuration.");
        return rc;
    }
    assert(cfg->L);
    L = cfg->L;

    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_blkend");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);

    rc = modlua_config_cb_eval(L, ib, module, name, 4);
    return rc;
}

/**
 * @param[in] cp Configuration parser.
 * @param[in] name Configuration directive name.
 * @param[in] onoff On or off setting.
 * @param[in] cbdata Callback data.
 */
static ib_status_t modlua_config_cb_onoff(
    ib_cfgparser_t *cp,
    const char *name,
    int onoff,
    void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(name != NULL);

    ib_status_t rc;
    modlua_cfg_t *cfg = NULL;
    lua_State *L;
    ib_module_t *module = NULL;
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not find module \"" MODULE_NAME_STR ".\"");
        return rc;
    }

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    rc = ib_context_module_config(ctx, module, &cfg);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve module configuration.");
        return rc;
    }
    assert(cfg->L);
    L = cfg->L;

    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_onoff");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);
    lua_pushinteger(L, onoff);

    rc = modlua_config_cb_eval(L, ib, module, name, 5);
    return rc;
}
/**
 * @param[in] cp Configuration parser.
 * @param[in] name Configuration directive name.
 * @param[in] p1 The only parameter.
 * @param[in] cbdata Callback data.
 */
static ib_status_t modlua_config_cb_param1(
    ib_cfgparser_t *cp,
    const char *name,
    const char *p1,
    void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(name != NULL);
    assert(p1 != NULL);

    ib_status_t rc;
    lua_State *L;
    modlua_cfg_t *cfg = NULL;
    ib_module_t *module = NULL;
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not find module \"" MODULE_NAME_STR ".\"");
        return rc;
    }

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    rc = ib_context_module_config(ctx, module, &cfg);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve module configuration.");
        return rc;
    }
    assert(cfg->L);
    L = cfg->L;

    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_param1");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);
    lua_pushstring(L, p1);

    rc = modlua_config_cb_eval(L, ib, module, name, 5);
    return rc;
}

/**
 * @param[in] cp Configuration parser.
 * @param[in] name Configuration directive name.
 * @param[in] p1 The first parameter.
 * @param[in] p2 The second parameter.
 * @param[in] cbdata Callback data.
 */
static ib_status_t modlua_config_cb_param2(
    ib_cfgparser_t *cp,
    const char *name,
    const char *p1,
    const char *p2,
    void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(name != NULL);
    assert(p1 != NULL);
    assert(p2 != NULL);

    ib_status_t rc;
    lua_State *L;
    ib_module_t *module = NULL;
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;
    modlua_cfg_t *cfg = NULL;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not find module \"" MODULE_NAME_STR ".\"");
        return rc;
    }

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    rc = ib_context_module_config(ctx, module, &cfg);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve module configuration.");
        return rc;
    }
    assert(cfg->L);
    L = cfg->L;

    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_param2");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);
    lua_pushstring(L, p1);
    lua_pushstring(L, p2);

    rc = modlua_config_cb_eval(L, ib, module, name, 6);
    return rc;
}
/**
 * @param[in] cp Configuration parser.
 * @param[in] name Configuration directive name.
 * @param[in] list List of values.
 * @param[in] cbdata Callback data.
 */
static ib_status_t modlua_config_cb_list(
    ib_cfgparser_t *cp,
    const char *name,
    const ib_list_t *list,
    void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(name != NULL);
    assert(list != NULL);

    ib_status_t rc;
    lua_State *L;
    modlua_cfg_t *cfg = NULL;
    ib_module_t *module = NULL;
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not find module \"" MODULE_NAME_STR ".\"");
        return rc;
    }

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    rc = ib_context_module_config(ctx, module, &cfg);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve module configuration.");
        return rc;
    }
    assert(cfg->L);
    L = cfg->L;


    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_list");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);
    lua_pushlightuserdata(L, (void *)list);

    rc = modlua_config_cb_eval(L, ib, module, name, 5);
    return rc;
}

/**
 * @param[in] cp Configuration parser.
 * @param[in] name Configuration directive name.
 * @param[in] mask The flags we are setting.
 * @param[in] cbdata Callback data.
 */
static ib_status_t modlua_config_cb_opflags(
    ib_cfgparser_t *cp,
    const char *name,
    ib_flags_t mask,
    void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(name != NULL);

    ib_status_t rc;
    lua_State *L;
    modlua_cfg_t *cfg = NULL;
    ib_module_t *module = NULL;
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not find module \"" MODULE_NAME_STR ".\"");
        return rc;
    }

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    rc = ib_context_module_config(ctx, module, &cfg);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve module configuration.");
        return rc;
    }
    assert(cfg->L);
    L = cfg->L;

    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_opflags");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);
    lua_pushinteger(L, mask);

    rc = modlua_config_cb_eval(L, ib, module, name, 5);
    return rc;
}

/**
 * @param[in] cp Configuration parser.
 * @param[in] name Configuration directive name.
 * @param[in] p1 The block name that we are exiting.
 * @param[in] cbdata Callback data.
 */
static ib_status_t modlua_config_cb_sblk1(
    ib_cfgparser_t *cp,
    const char *name,
    const char *p1,
    void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(name != NULL);
    assert(p1 != NULL);

    ib_status_t rc;
    lua_State *L;
    modlua_cfg_t *cfg = NULL;
    ib_module_t *module = NULL;
    ib_engine_t *ib = cp->ib;
    ib_context_t *ctx;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not find module \"" MODULE_NAME_STR ".\"");
        return rc;
    }

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve current context.");
        return rc;
    }

    rc = ib_context_module_config(ctx, module, &cfg);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Could not retrieve module configuration.");
        return rc;
    }
    assert(cfg->L);
    L = cfg->L;

    /* Push standard module directive arguments. */
    lua_getglobal(L, "modlua");
    lua_getfield(L, -1, "modlua_config_cb_sblk1");
    lua_replace(L, -2); /* Effectively remove then modlua table. */
    lua_pushlightuserdata(L, module->ib);
    lua_pushinteger(L, module->idx);
    rc = modlua_push_config_path(ib, ctx, L);
    if (rc != IB_OK) {
        lua_pop(L, 3);
        return rc;
    }

    /* Push config parameters. */
    lua_pushstring(L, name);
    lua_pushstring(L, p1);

    rc = modlua_config_cb_eval(L, ib, module, name, 5);
    return rc;
}


/**
 * Proxy function to ib_config_register_directive callable by Lua.
 *
 * @param[in] L Lua state. This stack should have on it 3 arguments
 *            to this function:
 *            - @c self - The Module API object.
 *            - @c name - The name of the directive to register.
 *            - @c type - The type of directive this is.
 *            - @c strvalmap - Optional string-value table map.
 */
static int modlua_config_register_directive(lua_State *L)
{
    assert(L);

    ib_status_t rc = IB_OK;
    const char *rcmsg = "Success.";

    /* Variables pulled from the Lua state. */
    ib_engine_t *ib;                /* Fetched from self on *L */
    const char *name;               /* Name of the directive. 2nd arg. */
    ib_dirtype_t type;              /* Type of directive. 3rd arg. */
    int args = lua_gettop(L);       /* Arguments passed to this function. */
    ib_strval_t *strvalmap;         /* Optional 4th arg. Must be converted. */
    ib_void_fn_t cfg_cb;            /* Configuration callback. */
    ib_module_t *module;

    /* We choose assert here because if this value is incorrect,
     * then the lua module code (lua.c and ironbee/module.lua) are
     * inconsistent with each other. */
    assert(args==3 || args==4);

    if (lua_istable(L, 0-args)) {

        /* Get IB Engine. */
        lua_getfield(L, 0-args, "ib_engine");
        if ( !lua_islightuserdata(L, -1) ) {
            lua_pop(L, 1);
            rc = IB_EINVAL;
            rcmsg = "ib_engine is not defined in module.";
            goto exit;
        }
        ib = (ib_engine_t *)lua_topointer(L, -1);
        assert(ib);
        lua_pop(L, 1); /* Remove IB. */

        /* Get Module. */
        lua_getfield(L, 0-args, "ib_module");
        if ( !lua_islightuserdata(L, -1) ) {
            lua_pop(L, 1);
            rc = IB_EINVAL;
            rcmsg = "ib_engine is not defined in module.";
            goto exit;
        }
        module = (ib_module_t *)lua_topointer(L, -1);
        assert(module);
        lua_pop(L, 1); /* Remove Module. */

    }
    else {
        rc = IB_EINVAL;
        rcmsg = "1st argument is not self table.";
        goto exit;
    }

    if (lua_isstring(L, 1-args)) {
        name = lua_tostring(L, 1-args);
    }
    else {
        rc = IB_EINVAL;
        rcmsg = "2nd argument is not a string.";
        goto exit;
    }

    if (lua_isnumber(L, 2-args)) {
        type = lua_tonumber(L, 2-args);
    }
    else {
        rc = IB_EINVAL;
        rcmsg = "3rd argument is not a number.";
        goto exit;
    }

    if (args==4) {
        if (lua_istable(L, 3-args)) {
            int varmapsz = 0;

            /* Iterator. */
            int i;

            /* Count the size of the table. table.maxn is not sufficient. */
            while (lua_next(L, 3-args)) { /* Push string, int onto stack. */
                ++varmapsz;
                lua_pop(L, 1); /* Pop off value. Leave key. */
            }

            if (varmapsz > 0) {
                /* Allocate space for strvalmap. */
                strvalmap = malloc(sizeof(*strvalmap) * (varmapsz + 1));
                if (strvalmap == NULL) {
                    rc = IB_EALLOC;
                    rcmsg = "Cannot allocate strval map.";
                    goto exit;
                }

                /* Build map. */
                i = 0;
                while (lua_next(L, 3-args)) { /* Push string, int onto stack. */
                    strvalmap[i].str = lua_tostring(L, -2);
                    strvalmap[i].val = lua_tointeger(L, -1);
                    lua_pop(L, 1); /* Pop off value. Leave key. */
                    ++i;
                }

                /* Null terminate the list. */
                strvalmap[varmapsz].str = NULL;
                strvalmap[varmapsz].val = 0;
            }
            else {
                strvalmap = NULL;
            }
        }
        else {
            rc = IB_EINVAL;
            rcmsg = "4th argument is not a table.";
            goto exit;
        }
    }
    else {
        strvalmap = NULL;
    }

    /* Assign the cfg_cb pointer to hand the callback. */
    switch (type) {
        case IB_DIRTYPE_ONOFF:
            cfg_cb = (ib_void_fn_t) &modlua_config_cb_onoff;
            break;
        case IB_DIRTYPE_PARAM1:
            cfg_cb = (ib_void_fn_t) &modlua_config_cb_param1;
            break;
        case IB_DIRTYPE_PARAM2:
            cfg_cb = (ib_void_fn_t) &modlua_config_cb_param2;
            break;
        case IB_DIRTYPE_LIST:
            cfg_cb = (ib_void_fn_t) &modlua_config_cb_list;
            break;
        case IB_DIRTYPE_OPFLAGS:
            cfg_cb = (ib_void_fn_t) &modlua_config_cb_opflags;
            break;
        case IB_DIRTYPE_SBLK1:
            cfg_cb = (ib_void_fn_t) &modlua_config_cb_sblk1;
            break;
        default:
            rc = IB_EINVAL;
            rcmsg = "Invalid configuration type.";
            goto exit;
    }

    rc = ib_config_register_directive(
        ib,
        name,
        type,
        cfg_cb,
        &modlua_config_cb_blkend,
        NULL,
        NULL,
        strvalmap);
    if (rc != IB_OK) {
        rcmsg = "Failed to register directive.";
        goto exit;
    }

exit:
    lua_pop(L, args);
    lua_pushinteger(L, rc);
    lua_pushstring(L, rcmsg);

    return lua_gettop(L);
}

ib_status_t modlua_module_load_lua(
    ib_engine_t *ib,
    bool is_config_time,
    const char *file,
    ib_module_t *module,
    lua_State *L)
{
    assert(ib != NULL);
    assert(file != NULL);
    assert(module != NULL);
    assert(L != NULL);

    int lua_rc;

    lua_getglobal(L, "modlua"); /* Get the package. */
    if (lua_isnil(L, -1)) {
        ib_log_error(ib, "Module modlua is undefined.");
        return IB_EINVAL;
    }
    if (! lua_istable(L, -1)) {
        ib_log_error(ib, "Module modlua is not a table/module.");
        lua_pop(L, 1); /* Pop modlua global off stack. */
        return IB_EINVAL;
    }

    lua_getfield(L, -1, "load_module"); /* Push load_module */
    if (lua_isnil(L, -1)) {
        ib_log_error(ib, "Module function load_module is undefined.");
        lua_pop(L, 1); /* Pop modlua global off stack. */
        return IB_EINVAL;
    }
    if (! lua_isfunction(L, -1)) {
        ib_log_error(ib, "Module function load_module is not a function.");
        lua_pop(L, 1); /* Pop modlua global off stack. */
        return IB_EINVAL;
    }

    lua_pushlightuserdata(L, ib); /* Push ib engine. */
    lua_pushlightuserdata(L, module); /* Push module engine. */
    lua_pushstring(L, file);
    lua_pushinteger(L, module->idx);

    if (is_config_time) {
        lua_pushcfunction(L, &modlua_config_register_directive);
    }
    else {
        lua_pushnil(L);
    }

    lua_rc = luaL_loadfile(L, file);
    switch(lua_rc) {
        case 0:
            /* NOP */
            break;
        case LUA_ERRRUN:
            ib_log_error(
                ib,
                "Error evaluating %s: %s",
                file,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
        case LUA_ERRMEM:
            ib_log_error(
                ib,
                "Failed to allocate memory during evaluation of %s",
                file);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
        case LUA_ERRERR:
            ib_log_error(
                ib,
                "Error fetching error message during evaluation of %s",
                file);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
#if LUA_VERSION_NUM > 501
        /* If LUA_ERRGCMM is defined, include a custom error for it as well.
          This was introduced in Lua 5.2. */
        case LUA_ERRGCMM:
            ib_log_error(
                ib,
                "Garbage collection error during evaluation of %s.",
                file);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
#endif
        default:
            ib_log_error(
                ib,
                "Unexpected error(%d) during evaluation of %s: %s",
                lua_rc,
                file,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
    }

    /**
     * The stack now is...
     * +-------------------------------------------+
     * | load_module                               |
     * | ib                                        |
     * | ib_module                                 |
     * | module name (file name)                   |
     * | module index                              |
     * | modlua_config_register_directive (or nil) |
     * | module script                             |
     * +-------------------------------------------+
     *
     * Next step is to call load_module which will, in turn, execute
     * the module script.
     */
    lua_rc = lua_pcall(L, 6, 1, 0);
    switch(lua_rc) {
        case 0:
            /* NOP */
            break;
        case LUA_ERRRUN:
            ib_log_error(
                ib,
                "Error loading module %s: %s",
                file,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
        case LUA_ERRMEM:
            ib_log_error(
                ib,
                "Failed to allocate memory during module load of %s",
                file);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
        case LUA_ERRERR:
            ib_log_error(
                ib,
                "Error fetching error message during module load of %s",
                file);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
#if LUA_VERSION_NUM > 501
        /* If LUA_ERRGCMM is defined, include a custom error for it as well.
          This was introduced in Lua 5.2. */
        case LUA_ERRGCMM:
            ib_log_error(
                ib,
                "Garbage collection error during module load of %s.",
                file);
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
#endif
        default:
            ib_log_error(
                ib,
                "Unexpected error(%d) during evaluation of %s: %s",
                lua_rc,
                file,
                lua_tostring(L, -1));
            lua_pop(L, 1); /* Get error string off of the stack. */
            lua_pop(L, 1); /* Pop modlua global off stack. */
            return IB_EINVAL;
    }

    lua_pop(L, 1); /* Pop modlua global off stack. */

    return IB_OK;
}


/**
 * Commit any pending configuration items, such as rules.
 *
 * @param[in] ib IronBee engine.
 * @param[in] cfg Module configuration.
 *
 * @returns
 *   - IB_OK
 *   - IB_EOTHER on Rule adding errors. See log file.
 */
static ib_status_t modlua_commit_configuration(
    ib_engine_t *ib,
    modlua_cfg_t *cfg)
{
    assert(ib != NULL);
    assert(cfg != NULL);
    assert(cfg->L != NULL);

    ib_status_t rc;
    int lua_rc;
    lua_State *L = cfg->L;

    lua_getglobal(L, "ibconfig");
    if ( ! lua_istable(L, -1) ) {
        ib_log_error(ib, "ibconfig is not a module table.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_getfield(L, -1, "build_rules");
    if ( ! lua_isfunction(L, -1) ) {
        ib_log_error(ib, "ibconfig.include is not a function.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_pushlightuserdata(L, ib);
    lua_rc = lua_pcall(L, 1, 1, 0);
    if (lua_rc == LUA_ERRFILE) {
        ib_log_error(ib, "Configuration Error: %s", lua_tostring(L, -1));
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }
    else if (lua_rc) {
        ib_log_error(ib, "Configuration Error: %s", lua_tostring(L, -1));
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }
    else if (lua_tonumber(L, -1) != IB_OK) {
        rc = lua_tonumber(L, -1);
        lua_pop(L, lua_gettop(L));
        ib_log_error(
            ib,
            "Configuration error reported: %d:%s",
            rc,
            ib_status_to_string(rc));
        return IB_EOTHER;
    }

    /* Clear stack. */
    lua_pop(L, lua_gettop(L));

    return IB_OK;
}


/* -- Event Handlers -- */

/**
 * Callback to initialize the per-connection Lua stack.
 *
 * Every Lua Module shares the same Lua stack for execution. This stack is
 * "owned" not by the module written in Lua by the user, but by this
 * module, the Lua Module.
 *
 * @param ib Engine.
 * @param conn Connection.
 * @param event Event type.
 * @param cbdata Unused.
 *
 * @return
 *   - IB_OK
 *   - IB_EALLOC on memory allocation failure.
 *   - Other upon failure of callback registration.
 */
static ib_status_t modlua_conn_init_lua_runtime(
    ib_engine_t *ib,
    ib_conn_t *conn,
    ib_state_event_type_t event,
    void *cbdata)
{
    assert(event == conn_started_event);
    assert(ib != NULL);
    assert(conn != NULL);
    assert(conn->ctx != NULL);

    ib_status_t rc;
    modlua_runtime_t *runtime;
    modlua_cfg_t *cfg = NULL;
    ib_context_t *ctx = conn->ctx;
    ib_module_t *module = NULL;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not find module \"" MODULE_NAME_STR ".\"");
        return rc;
    }

    rc = ib_context_module_config(ctx, module, &cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve modlua configuration.");
        return rc;
    }

    rc = modlua_acquirestate(ib, cfg, &runtime);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to acquire Lua runtime resource.");
        return rc;
    }

    rc = modlua_reload_ctx_except_main(ib, module, ctx, runtime->L);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not configure Lua stack.");
        return rc;
    }

    rc = modlua_runtime_set(conn, module, runtime);
    if (rc != IB_OK) {
        ib_log_alert(
            ib,
            "Could not store connection Lua stack in connection.");
        return rc;
    }

    return IB_OK;
}

/**
 * Using only the context, fetch the module configuration.
 *
 * @param[in] ib IronBee engine.
 * @param[in] ctx The current configuration context.
 * @param[out] cfg Where to store the configuration. **cfg must be NULL.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EEXIST if the module cannot be found in the engine.
 */
ib_status_t modlua_cfg_get(
    ib_engine_t *ib,
    ib_context_t *ctx,
    modlua_cfg_t **cfg)
{
    assert(ib != NULL);
    assert(ctx != NULL);

    ib_status_t rc;
    ib_module_t *module = NULL;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not find module \"" MODULE_NAME_STR ".\"");
        return rc;
    }

    rc = ib_context_module_config(ctx, module, cfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve modlua configuration.");
        return rc;
    }

    return IB_OK;
}

/**
 * Callback to destroy the per-connection Lua stack.
 *
 * Every Lua Module shares the same Lua stack for execution. This stack is
 * "owned" not by the module written in Lua by the user, but by this
 * module, the Lua Module.
 *
 * This is registered when the main context is closed to ensure that it
 * executes after all the Lua modules call backs (callbacks are executed
 * in the order of their registration).
 *
 * @param ib Engine.
 * @param conn Connection.
 * @param event Event type.
 * @param cbdata Unused.
 *
 * @return
 *   - IB_OK
 *   - IB_EALLOC on memory allocation failure.
 *   - IB_EOTHER if the stored Lua stack in the module's connection data
 *               is NULL.
 *   - Other upon failure of callback registration.
 */
static ib_status_t modlua_conn_fini_lua_runtime(ib_engine_t *ib,
                                                ib_conn_t *conn,
                                                ib_state_event_type_t event,
                                                void *cbdata)
{
    assert(event == conn_finished_event);
    assert(ib != NULL);
    assert(conn != NULL);

    ib_status_t rc;
    ib_module_t *module = NULL;
    modlua_runtime_t *modlua_rt = NULL;
    modlua_cfg_t *cfg = NULL;

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve module.");
        return rc;
    }

    rc = modlua_cfg_get(ib, conn->ctx, &cfg);
    if (rc != IB_OK) {
        ib_log_alert(
            ib,
            "Failed to get configuration for context %s.",
            ib_context_name_get(conn->ctx));
        return rc;
    }

    rc = modlua_runtime_get(conn, module, &modlua_rt);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Could not fetch per-connection Lua execution stack.");
        return rc;
    }
    if (modlua_rt == NULL) {
        ib_log_alert(ib, "Stored Lua execution stack was unexpectedly NULL.");
        return IB_EOTHER;
    }

    rc = modlua_releasestate(ib, cfg, modlua_rt);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to release Lua runtime back to pool.");
        return rc;
    }

    rc = modlua_runtime_clear(conn, module);

    return rc;
}

/* -- External Rule Driver -- */

/* -- Module Routines -- */


/**
 * Make an empty reloads list for the configuration for @a ctx.
 *
 * param[in] ib IronBee engine.
 * param[in] ctx The configuration context.
 * param[in] event The type of event. This must always be a
 *           @ref context_close_event.
 * param[in] cbdata Callback data. Unused.
 *
 * @returns
 *   - IB_OK on success.
 *   - Non-IB_OK on an unexpected internal engine failure.
 */
static ib_status_t modlua_context_open(
    ib_engine_t *ib,
    ib_context_t *ctx,
    ib_state_event_type_t event,
    void *cbdata)
{
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(event == context_open_event);

    ib_status_t     rc;
    modlua_cfg_t   *cfg;
    ib_mpool_t     *mp;

    /* In the case where we open the main context, we're done. */
    if ( ctx == ib_context_main(ib)) {
        return IB_OK;
    }

    mp = ib_engine_pool_main_get(ib);
    assert(mp != NULL);

    cfg = NULL;
    rc = modlua_cfg_get(ib, ctx, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_list_create(&(cfg->reloads), mp);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

/**
 * Context close callback. Registers outstanding rule configurations
 * if the context being closed in the main context.
 *
 * param[in] ib IronBee engine.
 * param[in] ctx The configuration context.
 * param[in] event The type of event. This must always be a
 *           @ref context_close_event.
 * param[in] cbdata Callback data. Unused.
 *
 * @returns
 *   - IB_OK on success.
 *   - Non-IB_OK on an unexpected internal engine failure.
 */
static ib_status_t modlua_context_close(
    ib_engine_t *ib,
    ib_context_t *ctx,
    ib_state_event_type_t event,
    void *cbdata)
{
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(event == context_close_event);

    ib_status_t rc;

    /* Close of the main context signifies configuration finished. */
    if (ib_context_type(ctx) == IB_CTYPE_MAIN) {
        modlua_cfg_t *cfg = NULL;

        rc = modlua_cfg_get(ib, ctx, &cfg);
        if (rc != IB_OK) {
            return rc;
        }

        /* Register this callback after the main context is closed.
         * This allows it to be executed LAST allowing all the Lua
         * modules created during configuration to be executed
         * in FILO ordering. */
        rc = ib_hook_conn_register(
            ib,
            conn_finished_event,
            modlua_conn_fini_lua_runtime,
            NULL);
        if (rc != IB_OK) {
            ib_log_error(
                ib,
                "Failed to register conn_finished_event hook: %s",
                ib_status_to_string(rc));
        }

        /* Commit any pending configuration items. */
        rc = modlua_commit_configuration(ib, cfg);
        if (rc != IB_OK) {
            return rc;
        }

        ib_log_debug(ib, "Releasing configuration Lua stack.");
        ib_resource_release(cfg->lua_resource);
        cfg->lua_resource = NULL;
        cfg->L = NULL;

        rc = ib_resource_pool_flush(cfg->lua_pool);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

/**
 * Context destroy callback.
 *
 * Destroys Lua stack and pointer when the main context is destroyed.
 *
 * param[in] ib IronBee engine.
 * param[in] ctx The configuration context.
 * param[in] event The type of event. This must always be a
 *           @ref context_close_event.
 * param[in] cbdata Callback data. Unused.
 *
 * @returns
 *   - IB_OK on success.
 *   - Non-IB_OK on an unexpected internal engine failure.
 */
static ib_status_t modlua_context_destroy(
    ib_engine_t *ib,
    ib_context_t *ctx,
    ib_state_event_type_t event,
    void *cbdata)
{
    assert(ib != NULL);
    assert(ctx != NULL);
    assert(event == context_destroy_event);

    /* Close of the main context signifies configuration finished. */
    if (ib_context_type(ctx) == IB_CTYPE_MAIN) {
        ib_status_t rc;
        modlua_cfg_t *cfg = NULL;

        rc = modlua_cfg_get(ib, ctx, &cfg);
        if (rc != IB_OK) {
            return rc;
        }

        ib_lock_destroy(&(cfg->lua_pool_lock));
    }

    return IB_OK;
}

/**
 * Initialize the ModLua Module.
 *
 * This will create a common "global" runtime into which various APIs
 * will be loaded.
 */
static ib_status_t modlua_init(ib_engine_t *ib,
                               ib_module_t *module,
                               void        *cbdata)
{
    assert(ib != NULL);
    assert(module != NULL);

    ib_mpool_t *mp = ib_engine_pool_main_get(ib);
    ib_status_t rc;
    modlua_cfg_t *cfg = NULL;

    cfg = ib_mpool_calloc(mp, 1, sizeof(*cfg));
    if (cfg == NULL) {
        ib_log_error(ib, "Failed to allocate lua module configuration.");
        return IB_EALLOC;
    }
    ib_log_debug(ib, "Allocated main configuration at %p.", cfg);

    rc = ib_list_create(&(cfg->reloads), mp);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to allocate reloads list.");
        return rc;
    }

    rc = ib_module_config_initialize(module, cfg, sizeof(*cfg));
    if (rc != IB_OK) {
        ib_log_error(ib, "Module already has configuration data?");
        return rc;
    }

    rc = ib_lock_init(&(cfg->lua_pool_lock));
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to configure Lua resource pool lock.");
        return rc;
    }

    rc = modlua_runtime_resource_pool_create(&(cfg->lua_pool), ib, module, mp);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not create Lua resource pool.");
        return rc;
    }

    /* Set up defaults */
    ib_log_debug(ib, "Making shared Lua state.");

    rc = ib_resource_acquire(cfg->lua_pool, &(cfg->lua_resource));
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not create Lua stack.");
        return rc;
    }
    cfg->L = ((modlua_runtime_t *)ib_resource_get(cfg->lua_resource))->L;

    /* Hook to initialize the lua runtime with the connection.
     * There is a modlua_conn_fini_lua_runtime which is only registered
     * when the main configuration context is being closed. This ensures
     * that it is the last hook to fire relative to the various
     * Lua-implemented modules this module may created and register. */
    rc = ib_hook_conn_register(
        ib,
        conn_started_event,
        modlua_conn_init_lua_runtime,
        NULL);
    if (rc != IB_OK) {
        ib_log_error(
            ib,
            "Failed to register conn_started_event hook: %s",
            ib_status_to_string(rc));
        return rc;
    }

    /* Hook the context close event.
     * New contexts must copy their parent context's reload list. */
    rc = ib_hook_context_register(
        ib,
        context_open_event,
        modlua_context_open,
        NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register context_open_event hook: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Hook the context close event. */
    rc = ib_hook_context_register(
        ib,
        context_close_event,
        modlua_context_close,
        NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register context_close_event hook: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Hook the context destroy event to deallocate the Lua stack and lock. */
    rc = ib_hook_context_register(
        ib,
        context_destroy_event,
        modlua_context_destroy,
        NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register context_destroy_event hook: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Set up rule support. */
    rc = rules_lua_init(ib, module);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

static ib_status_t modlua_dir_commit_rules(
    ib_cfgparser_t *cp,
    const char *name,
    const ib_list_t *list,
    void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);

    ib_context_t *ctx = NULL;
    modlua_cfg_t *cfg = NULL;
    ib_engine_t *ib = cp->ib;
    ib_status_t rc;

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        return rc;
    }

    rc = modlua_cfg_get(ib, ctx, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    return modlua_commit_configuration(ib, cfg);
}

/* -- Module Configuration -- */

static IB_CFGMAP_INIT_STRUCTURE(modlua_config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".pkg_path",
        IB_FTYPE_NULSTR,
        modlua_cfg_t,
        pkg_path
    ),
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".pkg_cpath",
        IB_FTYPE_NULSTR,
        modlua_cfg_t,
        pkg_cpath
    ),

    IB_CFGMAP_INIT_LAST
};


/* -- Configuration Directives -- */

/**
 * Implements the LuaInclude directive.
 *
 * Use the common Lua Configuration stack to configure IronBee using Lua.
 *
 * @param[in] cp Configuration parser and state.
 * @param[in] name The directive.
 * @param[in] p1 The file to include.
 * @param[in] cbdata The callback data. NULL. None is needed.
 *
 * @returns
 *   - IB_OK on success.
 *   - IB_EALLOC if an allocation cannot be performed, such as a Lua Stack.
 *   - IB_EOTHER if any other error is encountered.
 *   - IB_EINVAL if there is a Lua interpretation problem. This
 *               will almost always indicate a problem with the user's code
 *               and the user should examine their script.
 */
static ib_status_t modlua_dir_lua_include(ib_cfgparser_t *cp,
                                          const char *name,
                                          const char *p1,
                                          void *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(name != NULL);
    assert(p1 != NULL);

    ib_status_t rc;
    int lua_rc;
    ib_engine_t *ib = cp->ib;
    ib_core_cfg_t *corecfg = NULL;
    modlua_cfg_t *cfg = NULL;
    lua_State *L = NULL;
    ib_context_t *ctx;

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to retrieve current context.");
        return rc;
    }

    if (ctx != ib_context_main(ib)) {
        ib_cfg_log_error(
            cp,
            "Directive %s may only be used in the main context.",
            name);
        return IB_EOTHER;
    }

    rc = modlua_cfg_get(ib, ctx, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    L = cfg->L;

    rc = ib_core_context_config(ib_context_main(ib), &corecfg);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to retrieve core configuration.");
        lua_pop(L, lua_gettop(L));
        return rc;
    }

    lua_getglobal(L, "ibconfig");
    if ( ! lua_istable(L, -1) ) {
        ib_log_error(ib, "ibconfig is not a module table.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_getfield(L, -1, "include");
    if ( ! lua_isfunction(L, -1) ) {
        ib_log_error(ib, "ibconfig.include is not a function.");
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }

    lua_pushlightuserdata(L, cp);
    lua_pushstring(L, p1);
    lua_rc = lua_pcall(L, 2, 1, 0);
    if (lua_rc == LUA_ERRFILE) {
        ib_log_error(ib, "Could not access file %s.", p1);
        ib_log_error(ib, "Configuration Error: %s", lua_tostring(L, -1));
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }
    else if (lua_rc) {
        ib_log_error(ib, "Configuration Error: %s", lua_tostring(L, -1));
        lua_pop(L, lua_gettop(L));
        return IB_EOTHER;
    }
    else if (lua_tonumber(L, -1) != IB_OK) {
        rc = lua_tonumber(L, -1);
        lua_pop(L, lua_gettop(L));
        ib_log_error(
            ib,
            "Configuration error reported: %d:%s",
            rc,
            ib_status_to_string(rc));
        return IB_EOTHER;
    }

    lua_pop(L, lua_gettop(L));
    return IB_OK;
}

/**
 * @param[in] cp Configuration parser.
 * @param[in] name The name of the directive.
 * @param[in] p1 The argument to the directive parameter.
 * @param[in,out] cbdata Unused callback data.
 *
 * @returns
 *   - IB_OK
 *   - IB_EALLOC on memory error.
 *   - Others if engine registration or util functions fail.
 */
static ib_status_t modlua_dir_param1(ib_cfgparser_t *cp,
                                     const char *name,
                                     const char *p1,
                                     void *cbdata)
{
    ib_engine_t *ib = cp->ib;
    ib_module_t *module;
    ib_status_t rc;
    ib_core_cfg_t *corecfg = NULL;
    size_t p1_len = strlen(p1);
    size_t p1_unescaped_len;
    char *p1_unescaped;
    ib_context_t *ctx = NULL;
    modlua_cfg_t *cfg = NULL;

    rc = ib_cfgparser_context_current(cp, &ctx);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Cannot get current configuration context.");
        return rc;
    }

    rc = modlua_cfg_get(ib, ctx, &cfg);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_core_context_config(ib_context_main(ib), &corecfg);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to retrieve core configuration.");
        return rc;
    }

    rc = ib_engine_module_get(ib, MODULE_NAME_STR, &module);
    if (rc != IB_OK) {
        ib_log_error(ib, "Could not find module \"" MODULE_NAME_STR ".\"");
        return rc;
    }

    p1_unescaped = malloc(p1_len+1);
    if ( p1_unescaped == NULL ) {
        return IB_EALLOC;
    }

    rc = ib_util_unescape_string(
        p1_unescaped,
        &p1_unescaped_len,
        p1,
        p1_len,
        IB_UTIL_UNESCAPE_NULTERMINATE |
        IB_UTIL_UNESCAPE_NONULL);
    if (rc != IB_OK) {
        const char *msg = (rc == IB_EBADVAL)?
            "Value for parameter \"%s\" may not contain NULL bytes: %s":
            "Value for parameter \"%s\" could not be unescaped: %s";
        ib_cfg_log_debug(cp, msg, name, p1);
        free(p1_unescaped);
        return rc;
    }

    if (strcasecmp("LuaLoadModule", name) == 0) {
        /* Absolute path. */
        if (p1_unescaped[0] == '/') {
            rc = modlua_module_load(ib, module, p1_unescaped, cfg);
            if (rc != IB_OK) {
                ib_cfg_log_error(
                    cp,
                    "Failed to load Lua module with error %s: %s",
                    ib_status_to_string(rc),
                    p1_unescaped);
                return rc;
            }
        }
        else {
            char *path;

            /* Length of prefix, file name, and +1 for the joining '/'. */
            size_t path_len =
                strlen(corecfg->module_base_path) +
                strlen(p1_unescaped) +
                1;

            path = malloc(path_len+1);
            if (!path) {
                ib_cfg_log_error(cp, "Cannot allocate memory for module path.");
                free(p1_unescaped);
                return IB_EALLOC;
            }

            /* Build the full path. */
            snprintf(
                path,
                path_len+1,
                "%s/%s",
                corecfg->module_base_path,
                p1_unescaped);

            rc = modlua_module_load(ib, module, path, cfg);
            if (rc != IB_OK) {
                ib_log_error(
                    ib,
                    "Failed to load Lua module with error %s: %s",
                    ib_status_to_string(rc),
                    path);
                free(path);
                return rc;
            }
            free(path);
        }
    }
    else if (strcasecmp("LuaPackagePath", name) == 0) {
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);
        rc = ib_context_set_string(ctx, MODULE_NAME_STR ".pkg_path", p1_unescaped);
        free(p1_unescaped);
        return rc;
    }
    else if (strcasecmp("LuaPackageCPath", name) == 0) {
        ib_log_debug2(ib, "%s: \"%s\" ctx=%p", name, p1_unescaped, ctx);
        rc = ib_context_set_string(ctx, MODULE_NAME_STR ".pkg_cpath", p1_unescaped);
        free(p1_unescaped);
        return rc;
    }
    else {
        ib_log_error(ib, "Unhandled directive: %s %s", name, p1_unescaped);
        free(p1_unescaped);
        return IB_EINVAL;
    }

    free(p1_unescaped);
    return IB_OK;
}

static IB_DIRMAP_INIT_STRUCTURE(modlua_directive_map) = {
    IB_DIRMAP_INIT_PARAM1(
        "LuaLoadModule",
        modlua_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "LuaPackagePath",
        modlua_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "LuaPackageCPath",
        modlua_dir_param1,
        NULL
    ),
    IB_DIRMAP_INIT_PARAM1(
        "LuaInclude",
        modlua_dir_lua_include,
        NULL
    ),
    IB_DIRMAP_INIT_LIST(
        "LuaCommitRules",
        modlua_dir_commit_rules,
        NULL
    ),

    /* End */
    IB_DIRMAP_INIT_LAST
};

/**
 * Destroy global lock and Lua state.
 */
static ib_status_t modlua_fini(
    ib_engine_t *ib,
    ib_module_t *module,
    void *cbdata)
{
    assert(ib != NULL);
    assert(module != NULL);

    return IB_OK;
}

/* -- Module Definition -- */

/**
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /**< Default metadata */
    MODULE_NAME_STR,                     /**< Module name */
    IB_MODULE_CONFIG_NULL,               /**< modlua_init sets this. */
    modlua_config_map,                   /**< Configuration field map */
    modlua_directive_map,                /**< Config directive map */
    modlua_init,                         /**< Initialize function */
    NULL,                                /**< Callback data */
    modlua_fini,                         /**< Finish function */
    NULL,                                /**< Callback data */
);
