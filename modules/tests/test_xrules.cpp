//////////////////////////////////////////////////////////////////////////////
// Licensed to Qualys, Inc. (QUALYS) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// QUALYS licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
/// @file
/// @brief IronBee --- PCRE module tests
///
/// @author Brian Rectanus <brectanus@qualys.com>
//////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "base_fixture.h"
#include <ironbee/operator.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>
#include <ironbee/field.h>
#include <ironbee/bytestr.h>

#include <boost/filesystem.hpp>

class XRulesTest :
    public BaseTransactionFixture
{
};

TEST_F(XRulesTest, Load) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
}

TEST_F(XRulesTest, IPv4) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleIpv4 \"1.0.0.2/32\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, IPv6) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleIpv6 \"::1/128\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_FALSE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, Path) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRulePath \"/\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, Time1) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleTime \"00:00-23:59+0000\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, Time2) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleTime \"!00:00-23:59+0000\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_FALSE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, Time3) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleTime \"0,1,2,3,4,5,6,7@00:00-23:59+0000\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, Time4) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleTime \"!0,1,2,3,4,5,6,7@00:00-23:59+0000\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_FALSE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, ReqContentType1) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleRequestContentType \"all\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, ReqContentType2) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleRequestContentType \"text/html\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, ReqContentType3) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleRequestContentType \"text/bob|all\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, ReqContentType4) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleRequestContentType \"text/bob\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_FALSE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}

TEST_F(XRulesTest, RespContentType) {
    std::string config =
        std::string(
            "LogLevel DEBUG\n"
            "LoadModule \"ibmod_xrules.so\"\n"
            "SensorId B9C1B52B-C24A-4309-B9F9-0EF4CD577A3E\n"
            "SensorName UnitTesting\n"
            "SensorHostname unit-testing.sensor.tld\n"
            "XRuleResponseContentType \"text/html\" block priority=1\n"
            "<Site test-site>\n"
            "   SiteId AAAABBBB-1111-2222-3333-000000000000\n"
            "   Hostname somesite.com\n"
            "</Site>\n"
        );

    configureIronBeeByString(config.c_str());
    performTx();
    ASSERT_TRUE(ib_tx);
    ASSERT_TRUE(ib_tx->flags & IB_TX_BLOCK_IMMEDIATE);
}
