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
 * @brief IronBee -- Test Main
 *
 * This file defines main() for tests and understands boost exception.
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include <gtest/gtest.h>
#include <boost/exception/all.hpp>

using namespace std;

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int result = 0;
  if (testing::GTEST_FLAG(catch_exceptions)) {
      testing::GTEST_FLAG(catch_exceptions) = 0;
      try {
          result = RUN_ALL_TESTS();
      }
      catch (const boost::exception& e) {
          cerr << "Boost Exception: " << endl;
          cerr << diagnostic_information(e) << endl;
          return 1;
      }
      catch (const std::exception& e) {
          cerr << "Standard Exception: " << endl;
          cerr << e.what() << endl;
          return 1;
      }
      catch (...) {
          cerr << "Other Exception." << endl;
          return 1;
      }
  } 
  else {
      result = RUN_ALL_TESTS();
  }
  return result;
}