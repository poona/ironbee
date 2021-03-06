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

#ifndef _IB_ENGINE_MANAGER_H_
#define _IB_ENGINE_MANAGER_H_

/**
 * @file
 * @brief IronBee --- Engine Manager
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/engine.h>
#include <ironbee/log.h>
#include <ironbee/types.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeEngineManager IronBee Engine Manager
 * @ingroup IronBee
 *
 * The engine manager provides services to manage multiple IronBee engines.
 *
 * In the current implementation, all of these engines run in the same process
 * space.
 *
 * @{
 */

/* Local definitions */
#define IB_MANAGER_DEFAULT_MAX_ENGINES  8  /**< Default max # of engines */


/* Engine Manager type declarations */

/**
 * The engine manager.
 *
 * An engine manager is created via ib_manager_create().
 *
 * Servers which use the engine manager will typically create a single engine
 * manager at startup, and then use the engine manager to create engines when
 * the configuration has changed via ib_manager_engine_create().
 *
 * The engine manager will then manage the IronBee engines, with the most
 * recent one successfully created being the "current" engine.  An engine
 * managed by the manager is considered active if it is current or its
 * reference count is non-zero.
 *
 * ib_manager_engine_acquire() is used to acquire the current engine.  A
 * matching call to ib_manager_engine_release() is required to release it.  If
 * the released engine becomes inactive (e.g., the engine is not current and
 * its reference count becomes zero), the manager will destroy all inactive
 * engines.
 *
 */
typedef struct ib_manager_t ib_manager_t;

/**
 * Callback function to create a module structure using a given 
 *
 * This should not call ib_module_init() as the manager will do that.
 *
 * @param[out] module The module to create or return.
 * @param[in] ib The IronBee engine the module is being created for.
 * @param[in] cbdata Callback data.
 *
 * @returns
 * - IB_OK On success.
 * - IB_DECLINE If no module was created but the fucntion is defined.
 * - Other on error. Creation of the IronBee engine fails.
 */
typedef ib_status_t (*ib_manager_module_create_fn_t)(
    ib_module_t **module,
    ib_engine_t  *ib,
    void         *cbdata
);

/* Engine Manager API */

/**
 * Create an engine manager.
 *
 * @param[out] pmanager Pointer to IronBee engine manager object
 * @param[in] server IronBee server object
 * @param[in] max_engines Maximum number of simultaneous engines
 *
 * If @a logger_va_fn is provided, the engine manager's IronBee logger will not
 * format the log message, but will instead pass the format (@a fmt) and args
 * (@a ap) directly to the logger function.
 *
 * If @a logger_buf_fn is provided, the engine manager will do all of
 * the formatting, and will then call the logger with a formatted buffer.
 *
 * One of @a logger_va_fn or @a logger_buf_fn logger functions should be
 * specified, but not both.  Behavior is undefined if both are NULL or both
 * are non-NULL.
 *
 * If the server provides a @c va_list logging facility, the @a logger_va_fn
 * should be specified.  The alternate @c formatted buffer logger function @a
 * logger_buf_fn is provided for servers that don't provide a @c va_list
 * logging facility (e.g., Traffic Server).
 *
 * If specified, the @a logger_flush_fn function should flush the log file(s0
 *
 * @returns Status code
 * - IB_OK if all OK
 * - IB_EALLOC for allocation problems
 */
ib_status_t DLL_PUBLIC ib_manager_create(
    ib_manager_t                  **pmanager,
    const ib_server_t             *server,
    size_t                         max_engines
);

/**
 * Register a single module creation callback function.
 *
 * Currently only 1 module create function can be registered at a time.
 * Future callbacks will replace the previous one.
 *
 * The arguments @a module_fn and @a module_data may be NULL, in which case
 * the @a manager will have no callbacks.
 *
 * @param[out] pmanager Pointer to IronBee engine manager object
 * @param[in] module_fn Function to return a module structure for a server
 *            module that will be registered to a newly created IronBee
 *            engine. The module will be added before the engine is
 *            configured.
 * @param[in] module_data Callback data for @a module_fn.
 *
 * @returns Currently this always returns IB_OK.
 */
ib_status_t ib_manager_register_module_fn(
    ib_manager_t                  *manager,
    ib_manager_module_create_fn_t  module_fn,
    void                          *module_data
);

/**
 * Destroy an engine manager.
 *
 * Destroys IronBee engines managed by @a manager, and the engine manager
 * itself if all managed engines are destroyed.
 *
 * @note If server threads are still interacting with any of the IronBee
 * engines that are managed by @a manager, destroying @a manager can be
 * dangerous and can lead to undefined behavior.
 *
 * @param[in] manager IronBee engine manager
 */
void DLL_PUBLIC ib_manager_destroy(
    ib_manager_t *manager
);

/**
 * Create a new IronBee engine.
 *
 * The engine manager will destroy old engines when a call to
 * ib_manager_engine_release() results in a non-current engine having a
 * reference count of zero.
 *
 * If the engine creation is successful, this new engine becomes the current
 * engine.  This new engine is created with a zero reference count, but will
 * not be destroyed by ib_manager_engine_cleanup() operations as long as it
 * remains the current engine.
 *
 * @param[in] manager IronBee engine manager
 * @param[in] config_file Configuration file path
 *
 * @returns Status code
 *   - IB_OK All OK
 *   - IB_DECLINED Max # of engines reached, no engine created
 */
ib_status_t DLL_PUBLIC ib_manager_engine_create(
    ib_manager_t  *manager,
    const char    *config_file
);

/**
 * Acquire the current IronBee engine.
 *
 * This function increments the reference count associated with the current
 * engine, and then returns that engine.
 *
 * @note A matching call to ib_manager_engine_release() is required to
 * decrement the reference count.
 *
 * @param[in] manager IronBee engine manager
 * @param[out] pengine Pointer to the current engine
 *
 * @returns Status code
 *   - IB_OK All OK
 *   - IB_DECLINED No current IronBee engine exists
 */
ib_status_t DLL_PUBLIC ib_manager_engine_acquire(
    ib_manager_t  *manager,
    ib_engine_t  **pengine
);

/**
 * Release the specified IronBee engine when no longer required.
 *
 * This function decrements the reference count associated with the specified
 * engine.
 *
 * Behavior is undefined if @a engine is not known to the engine manager or
 * if the reference count of @a engine is zero.
 *
 * @note A matching prior call to ib_manager_engine_acquire() is required to
 * increment the reference count.
 *
 * @param[in] manager IronBee engine manager
 * @param[in] engine IronBee engine to release
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_manager_engine_release(
    ib_manager_t *manager,
    ib_engine_t  *engine
);

/**
 * Cleanup and destroy any inactive engines.
 *
 * This will destroy any non-current engines with a zero reference count.
 *
 * @param[in] manager IronBee engine manager
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_manager_engine_cleanup(
    ib_manager_t *manager
);

/**
 * Get the count of IronBee engines.
 *
 * @param[in] manager IronBee engine manager
 *
 * @returns Count of total IronBee engines
 */
size_t DLL_PUBLIC ib_manager_engine_count(
    ib_manager_t *manager
);

/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_MANAGER_H_ */
