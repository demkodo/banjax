/*
 * RegexManager unit test set
 *
 * Copyright (c) eQualit.ie 2013 under GNU AGPL v3.0 or later
 *
 *  Vmon: Sept 2013, Initial version
 */

// Copyright 2005, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <zmq.hpp>

#include <re2/re2.h> //google re2

#include <yaml-cpp/yaml.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtest/gtest.h> //google test

#include <ts/ts.h>

#include "util.h"
#include "banjax.h"
#include "unittest_common.h"
#include "regex_manager.h"

#include "swabber_interface.h"
#include "ip_database.h"

using namespace std;

/**
   Mainly fill an string stream buffer with a predefined configuration
   and to check if the regex manager has picked them correctly and
   match them correctly.
 */
class RegexManagerTest : public testing::Test {
 protected:
  string TEMP_DIR = "/tmp";

  std::unique_ptr<BanjaxFilter> test_regex_manager;

  IPDatabase test_ip_database;
  SwabberInterface test_swabber_interface;

  RegexManagerTest()
    :  test_swabber_interface(&test_ip_database)
  {}

  virtual void SetUp() { }
  virtual void TearDown() { }

  static std::string default_config() {
    return
      "regex_banner:\n"
      "  - rule: simple to ban\n"
      "    regex: '.*simple_to_ban.*'\n"
      "    interval: 1\n"
      "    hits_per_interval: 0\n"
      "  - rule: hard to ban\n"
      "    regex: '.*not%20so%20simple%20to%20ban[\\s\\S]*'\n"
      "    interval: 1\n"
      "    hits_per_interval: 0\n"
      "  - rule: 'flooding ban' \n"
      "    regex: '.*flooding_ban.*'\n"
      "    interval: 30 \n"
      "    hits_per_interval: 10\n"
      "  - rule: 'flooding ban 2' \n"
      "    regex: '.*flooding_diff_ban.*'\n"
      "    interval: 30 \n"
      "    hits_per_interval: 10\n";
  }

  void open_config(std::string config = default_config())
  {
    YAML::Node cfg = YAML::Load(config);

    FilterConfig regex_filter_config;

    try {
      for(auto i = cfg.begin(); i != cfg.end(); ++i) {
        std::string node_name = i->first.as<std::string>();
        if (node_name == "regex_banner")
          regex_filter_config.config_node_list.push_back(i);
      }
    }
    catch(YAML::RepresentationException& e)
    {
      ASSERT_TRUE(false);
    }

    test_regex_manager.reset(
        new RegexManager(TEMP_DIR,
                         regex_filter_config,
                         &test_ip_database,
                         &test_swabber_interface));
  }
};

/**
   read a pre determined config file and check if the values are
   as expected for the regex manager
 */
TEST_F(RegexManagerTest, load_config) {
  open_config();
}

/**
   make up a fake GET request and check that the manager is banning
 */
TEST_F(RegexManagerTest, match)
{
  open_config();

  //first we make a mock up request
  TransactionParts mock_transaction;
  mock_transaction[TransactionMuncher::METHOD] = "GET";
  mock_transaction[TransactionMuncher::IP] = "123.456.789.123";
  mock_transaction[TransactionMuncher::URL] = "http://simple_to_ban_me/";
  mock_transaction[TransactionMuncher::HOST] = "neverhood.com";
  mock_transaction[TransactionMuncher::UA] = "neverhood browsing and co";

  FilterResponse cur_filter_result = test_regex_manager->execute(mock_transaction);

  EXPECT_EQ(cur_filter_result.response_type, FilterResponse::I_RESPOND);
}

/**
 * A test to fix a github issue:
 * https://github.com/equalitie/banjax/issues/35
 */
TEST_F(RegexManagerTest, match_blank)
{
  auto config =
    "regex_banner:\n"
    "  - rule: simple to ban\n"
    "    regex: '^GET\\ .*mywebsite\\.org\\ $'\n"
    "    interval: 1\n"
    "    hits_per_interval: 0\n";

  open_config(config);

  //first we make a mock up request
  TransactionParts mock_transaction;
  mock_transaction[TransactionMuncher::METHOD] = "GET";
  mock_transaction[TransactionMuncher::IP] = "123.456.789.123";
  mock_transaction[TransactionMuncher::URL] = "http:///";
  mock_transaction[TransactionMuncher::HOST] = "mywebsite.org";
  mock_transaction[TransactionMuncher::UA] = "";

  FilterResponse cur_filter_result = test_regex_manager->execute(mock_transaction);

  EXPECT_EQ(cur_filter_result.response_type, FilterResponse::I_RESPOND);
}

/**
   make up a fake GET request and check that the manager is not banning
 */
TEST_F(RegexManagerTest, miss)
{
  open_config();

  //first we make a mock up request
  TransactionParts mock_transaction;
  mock_transaction[TransactionMuncher::METHOD] = "GET";
  mock_transaction[TransactionMuncher::IP] = "123.456.789.123";
  mock_transaction[TransactionMuncher::URL] = "http://dont_ban_me/";
  mock_transaction[TransactionMuncher::HOST] = "neverhood.com";
  mock_transaction[TransactionMuncher::UA] = "neverhood browsing and co";

  FilterResponse cur_filter_result = test_regex_manager->execute(mock_transaction);

  EXPECT_EQ(cur_filter_result.response_type, FilterResponse::GO_AHEAD_NO_COMMENT);

}


/**
   make up a fake GET request and check that the manager is not banning when
   the request method changes to POST despite passing the allowed rate
 */
TEST_F(RegexManagerTest, post_get_counter)
{

  open_config();

  //first we make a mock up request
  TransactionParts mock_transaction;
  mock_transaction[TransactionMuncher::METHOD] = "GET";
  mock_transaction[TransactionMuncher::IP] = "123.456.789.123";
  mock_transaction[TransactionMuncher::URL] = "http://flooding_ban/";
  mock_transaction[TransactionMuncher::HOST] = "neverhood.com";
  mock_transaction[TransactionMuncher::UA] = "neverhood browsing and co";
  FilterResponse cur_filter_result = FilterResponse::GO_AHEAD_NO_COMMENT;

  for ( int i=0; i<2; i++ ) {
    cur_filter_result = test_regex_manager->execute(mock_transaction);
  }

  mock_transaction[TransactionMuncher::URL] = "http://flooding_diff_ban/";
  cur_filter_result = test_regex_manager->execute(mock_transaction);

  EXPECT_EQ(cur_filter_result.response_type, FilterResponse::GO_AHEAD_NO_COMMENT);

}

/**
   make up a fake GET request and check that the manager is banning a request
   with special chars that . doesn't match
 */
TEST_F(RegexManagerTest, match_special_chars)
{

  open_config();

  //first we make a mock up request
  TransactionParts mock_transaction;
  mock_transaction[TransactionMuncher::METHOD] = "GET";
  mock_transaction[TransactionMuncher::IP] = "123.456.789.123";
  mock_transaction[TransactionMuncher::URL] = "http://not%20so%20simple%20to%20ban//";
  mock_transaction[TransactionMuncher::HOST] = "neverhood.com";
  mock_transaction[TransactionMuncher::UA] = "\"[this is no simple]\" () * ... :; neverhood browsing and co";

  FilterResponse cur_filter_result = test_regex_manager->execute(mock_transaction);

  EXPECT_EQ(cur_filter_result.response_type, FilterResponse::I_RESPOND);

}

/**
   Check the response
 */
TEST_F(RegexManagerTest, forbidden_response)
{

  open_config();

  //first we make a mock up request
  TransactionParts mock_transaction;
  mock_transaction[TransactionMuncher::METHOD] = "GET";
  mock_transaction[TransactionMuncher::IP] = "123.456.789.123";
  mock_transaction[TransactionMuncher::URL] = "http://simple_to_ban_me/";
  mock_transaction[TransactionMuncher::HOST] = "neverhood.com";
  mock_transaction[TransactionMuncher::UA] = "neverhood browsing and co";

  FilterResponse cur_filter_result = test_regex_manager->execute(mock_transaction);

  EXPECT_EQ(cur_filter_result.response_type, FilterResponse::I_RESPOND);

  EXPECT_EQ("<html><header></header><body>Forbidden</body></html>", test_regex_manager->generate_response(mock_transaction, cur_filter_result));

}

//TOOD: We need a test that listen on the publication and see if the ip really being send on the port
