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
 * @brief IronBee --- Engine Manager
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/engine_manager.h>

#include <ironbee/config.h>
#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/list.h>
#include <ironbee/lock.h>
#include <ironbee/log.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/release.h>
#include <ironbee/server.h>

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

/* The manager's engine wrapper type */
typedef struct ib_manager_engine_t ib_manager_engine_t;

/**
 * The Engine Manager.
 */
struct ib_manager_t {
    const ib_server_t    *server;          /**< Server object */
    ib_mpool_t           *mpool;           /**< Engine Manager's Memory pool */
    size_t                max_engines;     /**< The maximum number of engines */

    /*
     * List of all managed engines, and other related items.  These items are
     * all protected by the engine list lock.  To keep the implementation
     * simple, the latest engine is always stored at the end of the list, and
     * the list is compacted after removing elements from it.
     */
    ib_manager_engine_t **engine_list;     /**< Array of all engines */
    size_t                engine_count;    /**< Count of engines */
    ib_manager_engine_t  *engine_current;  /**< Current IronBee engine */
    ib_lock_t             manager_lck;     /**< Protect access to the mgr. */

    /* Engine Init Routine. */

    /**
     * Option module function to create a module to add to the engine.
     *
     * This is added before the engine is configured.
     */
    ib_manager_module_create_fn_t  module_fn;

    void                          *module_data; /**< Callback for module_fn. */
};

/**
 * The Engine Manager engine wrapper.
 *
 * There is one wrapper per engine instance.
 */
struct ib_manager_engine_t {
    ib_engine_t  *engine;           /**< The IronBee engine itself */
    uint64_t      ref_count;        /**< Engine's reference count */
};

/**
 * Memory pool cleanup function to destroy the locks attached to a manager.
 *
 * @param[in] cbdata Callback data (an @ref ib_manager_t).
 */
void cleanup_locks(
    void *cbdata
)
{
    assert(cbdata != NULL);

    ib_manager_t *manager = (ib_manager_t *)cbdata;

    ib_lock_destroy(&(manager->manager_lck));
}

/**
 * Report if an engine manager engine is active (there are references to it).
 *
 * @param[in] manager_engine The engine to check.
 *
 * @returns
 * - True if references are greater than 0.
 * - False if there are 0 references.
 */
static bool is_active(
    const ib_manager_engine_t *manager_engine
)
{
    assert(manager_engine != NULL);

    return manager_engine->ref_count > 0;
}

/**
 * Destroy IronBee engines
 *
 * This function assumes that the engine list lock has been locked by the
 * caller.
 *
 * Destroy all IronBee engines managed by @a manager.
 *
 * If @a op is IB_ENGINE_DESTROY_INACTIVE, only inactive, non-current engines
 * will be destroyed.  If @a op is IB_ENGINE_DESTROY_NON_CURRENT, all
 * non-current engines will be destroyed.  If @a op is IB_ENGINE_DESTROY_ALL,
 * all engines will be destroyed.
 *
 * @param[in,out] manager IronBee engine manager
 *
 * @returns Status code
 *  - IB_OK
 */
static ib_status_t destroy_inactive_engines(
    ib_manager_t  *manager
)
{
    assert(manager != NULL);
    const size_t list_sz = manager->engine_count;

    /* Destroy all non-current engines with zero reference count */
    for (size_t num = 0;  num < list_sz; ++num) {

        const ib_manager_engine_t *wrapper = manager->engine_list[num];
        ib_engine_t               *engine  = wrapper->engine;

        if (! is_active(wrapper))
        {
            --(manager->engine_count);

            /* Note: This will destroy the engine wrapper object, too */
            ib_engine_destroy(engine);

            /* NULL out it's place in the list -- we'll consolidate the
             * list at the bottom */
            manager->engine_list[num] = NULL;
        }
    }

    /* If any engines were destroyed, there is a NULL in the list
     * where they where. Collapse the list, removing NULLs. */
    if (list_sz > manager->engine_count) {

        /* Iterator i walks the list.
         * Non-null elements are copied to the iterator, j, which
         *     starts equal to i, but will lag behind i when the first
         *     NULL entry is seen.
         * When i increases to and beyond new_engine_count, we know
         *     that those array slots will not be used in the new list
         *     and should be cleared.
         */
        for (size_t i = 0, j = 0; i < list_sz; ++i) {

            /* If i is not null, copy it to j. */
            if (manager->engine_list[i] != NULL) {

                manager->engine_list[j] = manager->engine_list[i];
                ++j;

                /* If is is beyond the new list, NULL the source elements. */
                if (i >= manager->engine_count) {
                    manager->engine_list[i] = NULL;
                }
            }
        }
    }

    return IB_OK;
}

ib_status_t ib_manager_create(
    ib_manager_t                 **pmanager,
    const ib_server_t             *server,
    size_t                         max_engines
)
{
    assert(server != NULL);
    assert(pmanager != NULL);

    ib_status_t           rc;
    ib_mpool_t           *mpool;
    ib_manager_t         *manager;
    ib_manager_engine_t **engine_list;

    /* Max engines must be at least one */
    if (max_engines < 1) {
        return IB_EINVAL;
    }

    /* Create our memory pool. */
    rc = ib_mpool_create(&mpool, "Engine Manager", NULL);
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* Create the manager object. */
    manager = ib_mpool_calloc(mpool, sizeof(*manager), 1);
    if (manager == NULL) {
        rc = IB_EALLOC;
        goto cleanup;
    }

    /* Create the engine list. */
    engine_list = ib_mpool_calloc(mpool,
                                  max_engines,
                                  sizeof(ib_manager_engine_t *));
    if (engine_list == NULL) {
        rc = IB_EALLOC;
        goto cleanup;
    }

    /* Create the locks */
    rc = ib_lock_init(&(manager->manager_lck));
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* Cleanup locks when our memory pool is destroyed */
    rc = ib_mpool_cleanup_register(mpool, cleanup_locks, manager);
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* Populate the manager object. */
    manager->server         = server;
    manager->mpool          = mpool;
    manager->engine_list    = engine_list;
    manager->max_engines    = max_engines;
    manager->module_fn      = NULL;
    manager->module_data    = NULL;

    /* Hand the new manager off to the caller. */
    *pmanager = manager;

    /* We haven't acquired any locks, so we can just return. */
    return IB_OK;

cleanup:
    if (mpool != NULL) {
        ib_mpool_destroy(mpool);
    }

    return rc;
}

ib_status_t ib_manager_register_module_fn(
    ib_manager_t                  *manager,
    ib_manager_module_create_fn_t  module_fn,
    void                          *module_data
)
{
    assert(manager != NULL);

    manager->module_fn      = module_fn;
    manager->module_data    = module_data;

    return IB_OK;
}

void ib_manager_destroy(
    ib_manager_t *manager
)
{
    /* Destroy engines */
    for (size_t num = 0;  num < manager->engine_count;  ++num) {
        const ib_manager_engine_t *manager_engine = manager->engine_list[num];

        /* Note: This will destroy the engine wrapper object, too */
        ib_engine_destroy(manager_engine->engine);
    }

    /* Destroy the manager by destroying it's memory pool. */
    ib_mpool_destroy(manager->mpool);

    /* Note: Locks are destroyed by the memory pool cleanup */
}

/**
 * Register an engine.
 *
 * This requires that the caller hold the manager lock and that
 * ib_manager_t::engine_count be less than ib_manager_t::max_engines.
 *
 * - Add @a engine to @a manager's engine list. 
 * - Demote the current engine, removing the manager's reference count.
 * - Promote @a engine to current, adding a manager reference count.
 *
 * @param[in] manager Engine manager
 * @param[in] engine Engine wrapper object
 *
 */
static void register_engine(
    ib_manager_t        *manager,
    ib_manager_engine_t *engine
)
{
    assert(manager != NULL);
    assert(engine != NULL);
    assert(manager->engine_count < manager->max_engines);

    ib_manager_engine_t *previous_engine;

    /* Store the engine in the list of all engines. */
    manager->engine_list[manager->engine_count] = engine;
    ++(manager->engine_count);

    /* Store a reference to the previous engine. */
    previous_engine = manager->engine_current;

    /* Promote engine to the current engine (demoting the previous one. */
    manager->engine_current = engine;
    
    /* Add a reference count to the current engine for the manager. */
    ++(manager->engine_current->ref_count);

    /* If there was a previous engine, clean it up. */
    if (previous_engine != NULL) {

        /* Remove the engine manager's reference to the engine. */
        --(previous_engine->ref_count);
    }
}

/**
 * Determine if space is availbe for add another engine.
 *
 * This function assumes that the caller has locked the manager.
 *
 * This function will attempt a call to destroy_inactive_engines() if
 * no space is available. If no space is available after the cleanup attempt
 * IB_DECLINED is returned. 
 *
 * @param[in] manager The manager.
 *
 * @returns
 * - IB_OK If there is space, or if space has been reclaimed.
 * - IB_DECLINED If there is no space or an error occures.
 */
static ib_status_t has_engine_slots(ib_manager_t *manager)
{
    /* Are we already at the max # of engines? */
    if (manager->engine_count >= manager->max_engines) {

        /* Attempt to reclaim engine slots. */
        destroy_inactive_engines(manager);

        if (manager->engine_count >= manager->max_engines) {
            return IB_DECLINED;
        }
    }

    return IB_OK;
}

/**
 * This is how the manager creates a @ref ib_engine_t.
 *
 * It is wrapped in a @ref ib_manager_engine_t.
 *
 * This requires the caller to hold the manager lock.
 *
 * @param[in] manager The manager to use for creation.
 * @param[in] config_file The configuration file to pass the engine.
 * @param[out] engine_wrapper The engine wrapper to be constructed holding
 *             the engine and some meta data.
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC For memory errors.
 * - IB_DECLINED If there is no engine slot available. 
 */
static ib_status_t create_engine(
    ib_manager_t         *manager,
    const char           *config_file,
    ib_manager_engine_t **engine_wrapper
)
{
    assert(manager != NULL);
    assert(config_file != NULL);

    ib_status_t          rc;
    ib_status_t          rc2;
    ib_cfgparser_t      *parser = NULL;
    ib_context_t        *ctx;
    ib_manager_engine_t *wrapper;
    ib_engine_t         *engine;

    /* Create the engine */
    rc = ib_engine_create(&engine, manager->server);
    if (rc != IB_OK) {
        return rc;
    }

    /* Allocate an engine wrapper from the new engine's memory pool */
    wrapper = ib_mpool_calloc(
        ib_engine_pool_main_get(engine),
        1,
        sizeof(*wrapper));
    if (wrapper == NULL) {
        rc = IB_EALLOC;
        goto error;
    }

    /* If the user defined a module creation function, use and add to engine. */
    if (manager->module_fn != NULL) {

        /* Module the user creates. */
        ib_module_t *module       = NULL;

        /* Build a module structure per the plugin's request. */
        rc = manager->module_fn(&module, engine, manager->module_data);

        /* On OK, initialize the module in the engine. */
        if (rc == IB_OK) {

            ib_module_t *module_final = NULL;

            /* Copy the module structure. */
            rc = ib_module_dup(&module_final, module, engine);
            if (rc != IB_OK) {
                goto error;
            }

            /* Initialize the module into the engine. */
            rc = ib_module_init(module_final, engine);
            if (rc != IB_OK) {
                goto error;
            }
        }

        /* If module_fn is not OK and did not decline to make a module, fail. */
        else if (rc != IB_DECLINED) {
            goto error;
        }
    }

    /* Create the configuration parser */
    rc = ib_cfgparser_create(&parser, engine);
    if (rc != IB_OK) {
        goto error;
    }

    /* Tell the engine about the new parser.  Note that this creates the main
     * configuration context. */
    rc = ib_engine_config_started(engine, parser);
    if (rc != IB_OK) {
        goto error;
    }

    /* Get the main configuration context, set default log level. */
    ctx = ib_context_main(engine);
    ib_context_set_num(ctx, "logger.log_level", (ib_num_t)IB_LOG_WARNING);

    /* Parse the configuration */
    rc = ib_cfgparser_parse(parser, config_file);

    /* Report the status to the engine */
    rc2 = ib_engine_config_finished(engine);

    /* Merge rc2 into rc. */
    if ( (rc2 != IB_OK) && (rc == IB_OK) ) {
        rc = rc2;
    }

    /* All ok? */
    if (rc != IB_OK) {
        return rc;
    }

    /* Fill in the wrapper */
    wrapper->engine = engine;
    wrapper->ref_count = 0;

    *engine_wrapper = wrapper;
    return IB_OK;

error: 
    /* If a parser was created, destroy it */
    if (parser != NULL) {
        ib_cfgparser_destroy(parser);
    }

    if (engine != NULL) {
        ib_engine_destroy(engine);
    }
    return rc;
}

ib_status_t ib_manager_engine_create(
    ib_manager_t  *manager,
    const char    *config_file
)
{
    assert(manager != NULL);
    assert(config_file != NULL);

    ib_status_t          rc;
    ib_manager_engine_t *wrapper;

    /* Grab the engine creation lock to serialize engine creation. */
    rc = ib_lock_lock(&manager->manager_lck);
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* Check for or make space. */
    rc = has_engine_slots(manager);
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* If we have space, build an engine... */
    rc = create_engine(manager, config_file, &wrapper);
    if (rc != IB_OK) {
        goto cleanup;
    }

    /* ... and register that engine with the manager. */
    register_engine(manager, wrapper);

    /* Destroy any inactive engines. */
    destroy_inactive_engines(manager);

cleanup:

    /* Release any locks. */
    ib_lock_unlock(&manager->manager_lck);

    return rc;
}

ib_status_t ib_manager_engine_acquire(
    ib_manager_t  *manager,
    ib_engine_t  **pengine
)
{
    assert(manager != NULL);
    assert(pengine != NULL);

    ib_status_t          rc;
    ib_manager_engine_t *engine = NULL;

    /* Grab the engine list lock */
    rc = ib_lock_lock(&manager->manager_lck);
    if (rc != IB_OK) {
        return rc;
    }

    /* Get the current engine; If there is no current engine, decline. */
    engine = manager->engine_current;
    if (engine == NULL) {
        rc = IB_DECLINED;
        goto cleanup;
    }

    /* Increment the reference count */
    ++(engine->ref_count);
    rc = IB_OK;

    /* No need to update the inactive count; the current engine is
     * never inactive */
    *pengine = engine->engine;

cleanup:
    /* Release any locks */
    ib_lock_unlock(&manager->manager_lck);
    return rc;
}

ib_status_t ib_manager_engine_release(
    ib_manager_t *manager,
    ib_engine_t  *engine
)
{
    assert(manager != NULL);
    assert(engine != NULL);

    ib_status_t          rc;
    ib_manager_engine_t *managed_engine = NULL;

    /* Grab the engine list lock */
    rc = ib_lock_lock(&manager->manager_lck);
    if (rc != IB_OK) {
        return rc;
    }

    /* Happy path: The current engine is being released. */
    if ( (manager->engine_current != NULL) &&
         (engine == manager->engine_current->engine) )
    {
        managed_engine = manager->engine_current;
    }

    /* More work to find an old engine that's being released. */
    else {
        for (size_t num = 0;  num < manager->engine_count;  ++num) {
            ib_manager_engine_t *cur = manager->engine_list[num];

            /* Decrement the reference count if the engine matches. */
            if (engine == cur->engine) {
                managed_engine = cur;

                /* Leave the loop as we won't find engine a second time. */
                break;
            }
        }
    }

    /* Quick sanity check. Never release an unowned engine. */
    assert(manager->engine_current->ref_count > 0);

    /* Release the engine. */
    --(managed_engine->ref_count);

    /* Release the lock. */
    ib_lock_unlock(&manager->manager_lck);

    return rc;
}

ib_status_t ib_manager_engine_cleanup(
    ib_manager_t  *manager
)
{
    assert(manager != NULL);
    ib_status_t rc;

    /* Grab the engine list lock */
    rc = ib_lock_lock(&manager->manager_lck);
    if (rc != IB_OK) {
        return rc;
    }

    rc = destroy_inactive_engines(manager);
    if (rc != IB_OK) {
        ib_lock_unlock(&manager->manager_lck);
        return rc;
    }
    else {
        /* Grab the engine list lock */
        rc = ib_lock_unlock(&manager->manager_lck);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return rc;
}

size_t ib_manager_engine_count(
    ib_manager_t *manager
)
{
    assert(manager != NULL);
    return manager->engine_count;
}

