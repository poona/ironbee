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

#ifndef _IB_MODDEVEL_PRIVATE_H_
#define _IB_MODDEVEL_PRIVATE_H_

/**
 * @file
 * @brief IronBee --- Private development module definitions
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/config.h>
#include <ironbee/engine_types.h>
#include <ironbee/list.h>
#include <ironbee/types.h>

/**
 * Development module configuration structures
 */
typedef struct ib_moddevel_rules_config_t  ib_moddevel_rules_config_t;
typedef struct ib_moddevel_txdata_config_t ib_moddevel_txdata_config_t;
typedef struct ib_moddevel_txdump_config_t ib_moddevel_txdump_config_t;
typedef struct ib_moddevel_txresp_config_t ib_moddevel_txresp_config_t;

/**
 * TxData sub-module definitions
 */

/**
 * Initialize the TxData sub-module
 *
 * @param[in] ib IronBee engine
 * @param[in] mod Module
 * @param[in] mp Memory pool to use for allocations
 * @param[out] pconfig TxData configuration
 *
 * @returns Return code
 */
ib_status_t ib_moddevel_txdata_init(
    ib_engine_t                  *ib,
    ib_module_t                  *mod,
    ib_mpool_t                   *mp,
    ib_moddevel_txdata_config_t **pconfig);

/**
 * Cleanup the TxData sub-module
 *
 * @param[in] ib IronBee engine
 * @param[in] mod Module
 * @param[in] config TxData configuration
 *
 * @returns Return code
 */
ib_status_t ib_moddevel_txdata_cleanup(
    ib_engine_t                  *ib,
    ib_module_t                  *mod,
    ib_moddevel_txdata_config_t  *config);

/**
 * Un-Initialize the TxData sub-module
 *
 * @param[in] ib IronBee engine
 * @param[in] mod Module
 *
 * @returns Return code
 */
ib_status_t ib_moddevel_txdata_fini(
    ib_engine_t                  *ib,
    ib_module_t                  *mod);


/**
 * Rules sub-module definitions
 */

/**
 * Initialize the rules development sub-module
 *
 * Registers rule development operators and actions.
 *
 * @param[in] ib IronBee object
 * @param[in] mod Module object
 * @param[in] mp Memory pool to use for allocations
 * @param[out] pconfig Rules configuration
 *
 * @returns Status code
 */
ib_status_t ib_moddevel_rules_init(
    ib_engine_t                  *ib,
    ib_module_t                  *mod,
    ib_mpool_t                   *mp,
    ib_moddevel_rules_config_t  **pconfig);

/**
 * Cleanup the Rules sub-module
 *
 * @param[in] ib IronBee engine
 * @param[in] mod Module object
 * @param[in] config Rules configuration
 *
 * @returns Return code
 */
ib_status_t ib_moddevel_rules_cleanup(
    ib_engine_t                  *ib,
    ib_module_t                  *mod,
    ib_moddevel_rules_config_t   *config);

/**
 * Un-Initialize the rules sub-module
 *
 * @param[in] ib IronBee engine
 * @param[in] mod Module
 *
 * @returns Return code
 */
ib_status_t ib_moddevel_rules_fini(
    ib_engine_t                  *ib,
    ib_module_t                  *mod);


/**
 * TxDump sub-module definitions
 */

/**
 * Initialize the TxDump development sub-module
 *
 * @param[in] ib IronBee object
 * @param[in] mod Module object
 * @param[in] mp Memory pool to use for allocations
 * @param[out] pconfig TxDump configuration
 *
 * @returns Status code
 */
ib_status_t ib_moddevel_txdump_init(
    ib_engine_t                  *ib,
    ib_module_t                  *mod,
    ib_mpool_t                   *mp,
    ib_moddevel_txdump_config_t **pconfig);

/**
 * Cleanup the TxDump sub-module
 *
 * @param[in] ib IronBee engine
 * @param[in] mod Module
 * @param[in] config TxDump configuration
 *
 * @returns Return code
 */
ib_status_t ib_moddevel_txdump_cleanup(
    ib_engine_t                  *ib,
    ib_module_t                  *mod,
    ib_moddevel_txdump_config_t  *config);

/**
 * Un-Initialize the TxDump sub-module
 *
 * @param[in] ib IronBee engine
 * @param[in] mod Module
 *
 * @returns Return code
 */
ib_status_t ib_moddevel_txdump_fini(
    ib_engine_t                  *ib,
    ib_module_t                  *mod);


/**
 * TxResp sub-module definitions
 */

/**
 * Initialize the TxResp development sub-module
 *
 * @param[in] ib IronBee object
 * @param[in] mod Module object
 * @param[in] mp Memory pool to use for allocations
 * @param[out] pconfig TxResp configuration
 *
 * @returns Status code
 */
ib_status_t ib_moddevel_txresp_init(
    ib_engine_t                  *ib,
    ib_module_t                  *mod,
    ib_mpool_t                   *mp,
    ib_moddevel_txresp_config_t **pconfig);

/**
 * Cleanup the TxResp sub-module
 *
 * @param[in] ib IronBee engine
 * @param[in] mod Module
 * @param[in] config TxResp configuration
 *
 * @returns Return code
 */
ib_status_t ib_moddevel_txresp_cleanup(
    ib_engine_t                  *ib,
    ib_module_t                  *mod,
    ib_moddevel_txresp_config_t  *config);

/**
 * Un-Initialize the TxResp sub-module
 *
 * @param[in] ib IronBee engine
 * @param[in] mod Module
 *
 * @returns Return code
 */
ib_status_t ib_moddevel_txresp_fini(
    ib_engine_t                  *ib,
    ib_module_t                  *mod);


#endif /* _IB_MODDEVEL_PRIVATE_H_ */
