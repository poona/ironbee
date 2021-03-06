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

#ifndef _IB_ENGINE_H_
#define _IB_ENGINE_H_

/**
 * @file
 * @brief IronBee --- Engine
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/build.h>
#include <ironbee/cfgmap.h>
#include <ironbee/clock.h>
#include <ironbee/engine_types.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>
#include <ironbee/logger.h>
#include <ironbee/parsed_content.h>
#include <ironbee/server.h>
#include <ironbee/stream.h>
#include <ironbee/uuid.h>

#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup IronBeeEngine IronBee Engine
 * @ingroup IronBee
 *
 * This is the API for the IronBee engine.
 *
 * @{
 */


/**
 * Audit log part generator function.
 *
 * This function is called repetitively to generate the data logged
 * in an audit log part. The function should return zero when there
 * is no more data to log.
 *
 * @param part Audit log part
 * @param chunk Address in which chunk is written
 *
 * @returns Size of the chunk or zero to indicate completion
 */
typedef size_t (*ib_auditlog_part_gen_fn_t)(ib_auditlog_part_t *part,
                                            const uint8_t **chunk);

/** Audit Log */
struct ib_auditlog_t {
    ib_engine_t        *ib;              /**< Engine handle */
    ib_mpool_t         *mp;              /**< Connection memory pool */
    ib_context_t       *ctx;             /**< Config context */
    ib_tx_t            *tx;              /**< Transaction being logged */
    void               *cfg_data;        /**< Implementation config data */
    ib_list_t          *parts;           /**< List of parts */
};

/** Audit Log Part */
struct ib_auditlog_part_t {
    ib_auditlog_t      *log;             /**< Audit log */
    const char         *name;            /**< Part name */
    const char         *content_type;    /**< Part content type */
    void               *part_data;       /**< Arbitrary part data */
    void               *gen_data;        /**< Data for generator function */
    ib_auditlog_part_gen_fn_t fn_gen;    /**< Data generator function */
};


/// @todo Maybe a ib_create_ex() that takes a config?

/**
 * Initialize IronBee (before engine creation)
 *
 * @returns Status code
 *    - IB_OK
 *    - Errors returned by ib_util_initialize()
 */
ib_status_t DLL_PUBLIC ib_initialize(void);

/**
 * Shutdown IronBee (After engine destruction)
 *
 * @returns Status code
 *    - IB_OK
 *    - Errors returned by ib_util_shutdown()
 */
ib_status_t DLL_PUBLIC ib_shutdown(void);

/**
 * Create an engine handle.
 *
 * After creating the engine, the caller must configure defaults, such as
 * initial logging parameters, and then call ib_engine_init() to
 * initialize the engine configuration context.
 *
 * @param pib Address which new handle is written
 * @param server Information on the server instantiating the engine
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_engine_create(ib_engine_t **pib,
                                        const ib_server_t *server);

/**
 * Initialize the engine configuration context.
 *
 * @param ib Engine handle
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_engine_init(ib_engine_t *ib);

/**
 * Return the logger object constructed for this engine.
 *
 * Use the returned object to add writers or change the log level.
 *
 * This will never return NULL as ib_engine_create() would have failed
 * with an IB_EALLOC.
 *
 * @returns Pointer to the logger structure for @a ib.
 */
ib_logger_t DLL_PUBLIC *ib_engine_logger_get(const ib_engine_t *ib);

/**
 * Get the engine's instance UUID
 *
 * @param ib Engine handle
 *
 * @returns Pointer to engine's instance UUID
 */
const ib_uuid_t DLL_PUBLIC *ib_engine_instance_uuid(const ib_engine_t *ib);

/**
 * Get the string form of the engine's instance UUID
 *
 * @param ib Engine handle
 *
 * @returns The engine's instance UUID string
 */
const char DLL_PUBLIC *ib_engine_instance_uuid_str(const ib_engine_t *ib);

/**
 * Inform the engine that the configuration phase is starting
 *
 * @param[in] ib Engine handle
 * @param[in] cp The configuration parser
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_engine_config_started(ib_engine_t *ib,
                                                ib_cfgparser_t *cp);

/**
 * Inform the engine that the configuration phase is complete
 *
 * @param[in] ib Engine handle
 *
 * @returns Status code:
 *  - IB_OK - All OK
 *  - Status from ib_context_close() (including the context close functions)
 */
ib_status_t DLL_PUBLIC ib_engine_config_finished(ib_engine_t *ib);

/**
 * Get the configuration parser
 *
 * @param[in] ib Engine handle
 * @param[out] pparser Pointer to the configuration parser.
 *
 * @returns IB_OK
 */
ib_status_t ib_engine_cfgparser_get(const ib_engine_t *ib,
                                    const ib_cfgparser_t **pparser);

/**
 * Create a main context to operate in.
 *
 * @param[in] ib IronBee engine that contains the ectx that we will use
 *            in creating the main context. The main context
 *            will be assigned to ib->ctx if it is successfully created.
 *
 * @returns IB_OK or the result of
 *          ib_context_create(ctx, ib, ib->ectx, NULL, NULL, NULL).
 */
ib_status_t ib_engine_context_create_main(ib_engine_t *ib);

/**
 * Get a module by name.
 *
 * @param ib Engine handle
 * @param name Module name
 * @param pm Address which module will be written
 *
 * @returns
 * - IB_OK On success.
 * - IB_ENOENT If the named module is not found.
 */
ib_status_t DLL_PUBLIC ib_engine_module_get(const ib_engine_t *ib,
                                            const char * name,
                                            ib_module_t **pm);

/**
 * Get the main engine memory pool.
 *
 * @param ib Engine handle
 *
 * @returns Memory pool
 */
ib_mpool_t DLL_PUBLIC *ib_engine_pool_main_get(const ib_engine_t *ib);

/**
 * Get the engine configuration memory pool.
 *
 * This pool should be used to configure the engine.
 *
 * @param ib Engine handle
 *
 * @returns Memory pool
 */
ib_mpool_t DLL_PUBLIC *ib_engine_pool_config_get(const ib_engine_t *ib);

/**
 * Get the engine temporary memory pool.
 *
 * This pool should be destroyed by the server after the
 * configuration phase. Therefore is should not be used for
 * anything except temporary allocations which are required
 * for performing configuration.
 *
 * @param ib Engine handle
 *
 * @returns Memory pool
 */
ib_mpool_t DLL_PUBLIC *ib_engine_pool_temp_get(const ib_engine_t *ib);

/**
 * Destroy the engine temporary memory pool.
 *
 * This should be called by the server after configuration is
 * completed. After this call, any allocations in the temporary
 * pool will be invalid and no future allocations can be made to
 * to this pool.
 *
 * @param ib Engine handle
 */
void DLL_PUBLIC ib_engine_pool_temp_destroy(ib_engine_t *ib);

/**
 * Destroy a memory pool.
 *
 * This destroys the memory pool @a mp.  If IB_DEBUG_MEMORY is defined,
 * it will validate and analyze the pool before destruction.
 *
 * Will do nothing if @a mp is NULL.
 *
 * @param[in] ib IronBee engine.
 * @param[in] mp Memory pool to destroy.
 */
void DLL_PUBLIC ib_engine_pool_destroy(ib_engine_t *ib, ib_mpool_t *mp);

/**
 * Get var configuration of engine.
 *
 * @param[in] ib IronBee engine.
 * @return Var configuration.
 **/
ib_var_config_t DLL_PUBLIC *ib_engine_var_config_get(
    ib_engine_t *ib
);

/**
 * Get var configuration of engine (const version).
 *
 * @param[in] ib IronBee engine.
 * @return var configuration.
 **/
const ib_var_config_t DLL_PUBLIC *ib_engine_var_config_get_const(
    const ib_engine_t *ib
);

/**
 * Destroy an engine.
 *
 * @param ib Engine handle
 */
void DLL_PUBLIC ib_engine_destroy(ib_engine_t *ib);

/**
 * Create a connection structure.
 *
 * @param ib Engine handle
 * @param pconn Address which new connection is written
 * @param server_ctx Server connection context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_conn_create(ib_engine_t *ib,
                                      ib_conn_t **pconn,
                                      void *server_ctx);

/**
 * Get per-module per-connection data.
 *
 * @param[in]  conn Connection
 * @param[in]  module Module.
 * @param[out] data Data.  Can be any handle, i.e., `T **`.

 * @returns
 *   - IB_OK on success.
 *   - IB_EINVAL if @a conn does not know about @a module.
 */
ib_status_t DLL_PUBLIC ib_conn_get_module_data(
    const ib_conn_t   *conn,
    const ib_module_t *module,
    void              *data
);

/**
 * Set per-module per-connection data.
 *
 * @param[in] conn Connection
 * @param[in] module Module.
 * @param[in] data Data.
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_conn_set_module_data(
    ib_conn_t         *conn,
    const ib_module_t *module,
    void              *data
);

/**
 * Set connection flags.
 *
 * @param conn Connection structure
 * @param flag Flags
 */
#define ib_conn_flags_set(conn, flag) do { (conn)->flags |= (flag); } while(0)

/**
 * Unset connection flags.
 *
 * @param conn Connection structure
 * @param flag Flags
 */
#define ib_conn_flags_unset(conn, flag) do { (conn)->flags &= ~(flag); } while(0)

/**
 * Check if connection flags are all set.
 *
 * @param conn Connection structure
 * @param flag Flags
 *
 * @returns All current flags
 */
#define ib_conn_flags_isset(conn, flag) ((conn)->flags & (flag) ? 1 : 0)

/**
 * Destroy a connection structure.
 *
 * @param conn Connection structure
 */
void DLL_PUBLIC ib_conn_destroy(ib_conn_t *conn);

/**
 * Merge the base_uuid with tx data and generate the tx id string.
 *
 * This function is normally executed by ib_tx_create(), but if the tx is
 * being created in other ways (e.g. in the tests), use this to generate the
 * TX's ID.
 *
 * @param[in,out] tx Transaction to populate
 * @param[in] mp Memory pool to use.
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tx_generate_id(ib_tx_t *tx,
                                         ib_mpool_t *mp);

/**
 * Create a transaction structure.
 *
 * @param ptx Address which new transaction is written
 * @param conn Connection structure
 * @param sctx Server transaction context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_tx_create(ib_tx_t **ptx,
                                    ib_conn_t *conn,
                                    void *sctx);

/**
 * Get per-module per-transaction data.
 *
 * @param[in]  tx Transaction.
 * @param[in]  module Module.
 * @param[out] pdata Address which data is written
 * @returns
 *   - IB_OK on success.
 *   - IB_EINVAL if @a tx does not know about @a module.
 */
ib_status_t DLL_PUBLIC ib_tx_get_module_data(
    const ib_tx_t *tx,
    const ib_module_t *module,
    void  *pdata
);

/**
 * Set per-module per-transaction data.
 *
 * @param[in] tx Transaction.
 * @param[in] module Module.
 * @param[in] data Data.
 * @returns Status code.
 */
ib_status_t DLL_PUBLIC ib_tx_set_module_data(
    ib_tx_t *tx,
    const ib_module_t *module,
    void *data
);

/**
 * Set server error status code.
 *
 * @param[in] tx The transaction.
 * @param[in] status The status code.
 *
 * @returns
 *   - IB_OK On success.
 *   - IB_ENOTIMPL If not implemented by the server plugin.
 */
ib_status_t DLL_PUBLIC ib_tx_server_error(
    ib_tx_t *tx,
    int status
);

/**
 * Set server error header.
 *
 * @param[in] tx The transaction.
 * @param[in] name The null-terminated name of the header.
 * @param[in] value The null-terminated value of the header.
 *
 * @returns
 *   - IB_OK On success.
 *   - IB_ENOTIMPL If not implemented by the server plugin.
 */
ib_status_t DLL_PUBLIC ib_tx_server_error_header(
    ib_tx_t *tx,
    const char *name,
    const char *value
);

/**
 * Set server error data.
 *
 * @param[in] tx The transaction.
 * @param[in] data The data to set.
 * @param[in] dlen The length of the data to send.
 *
 * @returns
 *   - IB_OK On success.
 *   - IB_ENOTIMPL If not implemented by the server plugin.
 */
ib_status_t DLL_PUBLIC ib_tx_server_error_data(
    ib_tx_t *tx,
    const uint8_t *data,
    size_t dlen
);

/**
 * Set a header to be either requested or responded to in the server.
 *
 * @param[in] tx The transaction.
 * @param[in] dir Direction. This indicates if this is for the request or
 *            the response of the HTTP transaction.
 * @param[in] action How to add this header. Add, delete, set, etc.
 * @param[in] hdr Null-terminated header name string.
 * @param[in] value Null-terminated header value string.
 * @param[in] rx The regular expression if the action is @ref IB_HDR_EDIT.
 *
 * @returns
 *   - IB_OK On success.
 *   - IB_ENOTIMPL If not implemented by the server plugin.
 */
ib_status_t DLL_PUBLIC ib_tx_server_header(
    ib_tx_t *tx,
    ib_server_direction_t dir,
    ib_server_header_action_t action,
    const char *hdr,
    const char *value,
    ib_rx_t *rx
);

/**
 * Set transaction flags.
 *
 * @param tx Transaction structure
 * @param flag Flags
 *
 * @returns All current flags
 */
#define ib_tx_flags_set(tx, flag) do { (tx)->flags |= (flag); } while(0)

/**
 * Unset transaction flags.
 *
 * @param tx Transaction structure
 * @param flag Flags
 *
 * @returns All current flags
 */
#define ib_tx_flags_unset(tx, flag) do { (tx)->flags &= ~(flag); } while(0)

/**
 * Check if transaction flags are all set.
 *
 * @param tx Transaction structure
 * @param flag Flags
 *
 * @returns All current flags
 */
#define ib_tx_flags_isset(tx, flag) ((tx)->flags & (flag) ? 1 : 0)

/**
 * Mark transaction as not having a body.
 *
 * @param tx Transaction structure
 */
#define ib_tx_mark_nobody(tx) ib_tx_flags_set(tx, IB_TX_FREQ_NOBODY)

/**
 * Destroy a transaction structure.
 *
 * The transaction @a tx MUST be the first transaction in the parent
 * connection's (tx->conn) transaction list.  This function also removes
 * @a tx from the parent connection's list, and updates the connection's
 * first (tx_first) and last (tx_last) transaction pointers.
 *
 * @param tx Transaction structure
 */
void DLL_PUBLIC ib_tx_destroy(ib_tx_t *tx);

/**
 * @} IronBeeEngineEvent
 */

/**
 * @defgroup IronBeeFilter Filter
 * @ingroup IronBeeEngine
 * @{
 */

#define IB_FILTER_FNONE          0        /**< No filter flags were set */
#define IB_FILTER_FMDATA        (1<<0)    /**< Filter modified the data */
#define IB_FILTER_FMDLEN        (1<<1)    /**< Filter modified data length */
#define IB_FILTER_FINPLACE      (1<<2)    /**< Filter action was in-place */

#define IB_FILTER_ONONE          0        /**< No filter options set */
#define IB_FILTER_OMDATA        (1<<0)    /**< Filter may modify data */
#define IB_FILTER_OMDLEN        (1<<1)    /**< Filter may modify data length */
#define IB_FILTER_OBUF          (1<<2)    /**< Filter may buffer data */

/**
 * Filter Function.
 *
 * This function is called with data that can be analyzed and then optionally
 * modified.  Various flags can be set via pflags.
 *
 * @param f Filter
 * @param fdata Filter data
 * @param ctx Config context
 * @param pool Pool to use, should allocation be required
 * @param pflags Address to write filter processing flags
 *
 * @returns Status code
 */
typedef ib_status_t (*ib_filter_fn_t)(ib_filter_t *f,
                                      ib_fdata_t *fdata,
                                      ib_context_t *ctx,
                                      ib_mpool_t *pool,
                                      ib_flags_t *pflags);

/** IronBee Filter */
struct ib_filter_t {
    ib_engine_t             *ib;        /**< Engine */
    const char              *name;      /**< Filter name */
    ib_filter_type_t         type;      /**< Filter type */
    ib_flags_t               options;   /**< Filter options */
    size_t                   idx;       /**< Filter index */
    ib_filter_fn_t           fn_filter; /**< Filter function */
    void                    *cbdata;    /**< Filter callback data */
};

/** IronBee Filter Data */
struct ib_fdata_t {
    union {
        void                *ptr;       /**< Generic pointer for set op */
        ib_conn_t           *conn;      /**< Connection (conn filters) */
        ib_tx_t             *tx;        /**< Transaction (tx filters) */
    } udata;
    ib_stream_t             *stream;    /**< Data stream */
    void                    *state;     /**< Arbitrary state data */
};

/**
 * IronBee Filter Controller.
 *
 * Data comes into the filter controller via the @ref source, gets
 * pushed through the list of data @ref filters, into the buffer filter
 * @ref fbuffer where the data may be held while it is being processed
 * and finally makes it to the @ref sink where it is ready to be sent.
 */
struct ib_fctl_t {
    ib_fdata_t               fdata;     /**< Filter data */
    ib_engine_t             *ib;        /**< Engine */
    ib_mpool_t              *mp;        /**< Filter memory pool */
    ib_list_t               *filters;   /**< Filter list */
    ib_filter_t             *fbuffer;   /**< Buffering filter (flow control) */
    ib_stream_t             *source;    /**< Data source (new data) */
    ib_stream_t             *sink;      /**< Data sink (processed data) */
};


/* -- Filter API -- */

/**
 * Register a filter.
 *
 * @param pf Address which filter handle is written
 * @param ib Engine handle
 * @param name Filter name
 * @param type Filter type
 * @param options Filter options
 * @param fn_filter Filter callback function
 * @param cbdata Filter callback data
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_filter_register(ib_filter_t **pf,
                                          ib_engine_t *ib,
                                          const char *name,
                                          ib_filter_type_t type,
                                          ib_flags_t options,
                                          ib_filter_fn_t fn_filter,
                                          void *cbdata);

/**
 * Add a filter to a context.
 *
 * @param f Filter
 * @param ctx Config context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_filter_add(ib_filter_t *f,
                                     ib_context_t *ctx);

/**
 * Create a filter controller for a transaction.
 *
 * @param pfc Address which filter controller handle is written
 * @param tx Transaction
 * @param pool Memory pool
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_tx_create(ib_fctl_t **pfc,
                                         ib_tx_t *tx,
                                         ib_mpool_t *pool);

/**
 * Configure a filter controller for a given context.
 *
 * @param fc Filter controller
 * @param ctx Config context
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_config(ib_fctl_t *fc,
                                      ib_context_t *ctx);

/**
 * Process any pending data through the filters.
 *
 * @param fc Filter controller
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_process(ib_fctl_t *fc);

/**
 * Add data to the filter controller.
 *
 * This will pass through all the filters and then be fetched
 * with calls to @ref ib_fctl_drain.
 *
 * @param fc Filter controller
 * @param data Data
 * @param dlen Data length
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_data_add(ib_fctl_t *fc,
                                        void *data,
                                        size_t dlen);

/**
 * Add meta data to the filter controller.
 *
 * This will pass through all the filters and then be fetched
 * with calls to @ref ib_fctl_drain.
 *
 * @param fc Filter controller
 * @param stype Stream data type
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_meta_add(ib_fctl_t *fc,
                                        ib_sdata_type_t stype);

/**
 * Drain processed data from the filter controller.
 *
 * @param fc Filter controller
 * @param pstream Address which output stream is written
 *
 * @returns Status code
 */
ib_status_t DLL_PUBLIC ib_fctl_drain(ib_fctl_t *fc,
                                     ib_stream_t **pstream);


/**
 * @} IronBeeFilter
 */

/**
 * @} IronBeeEngine
 * @} IronBee
 */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_H_ */
