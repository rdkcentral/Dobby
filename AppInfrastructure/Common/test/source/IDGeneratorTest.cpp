/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include <IDGenerator.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <bitset>
#include <random>
#include <algorithm>

using namespace testing;



template< std::size_t N >
static void testAllPossibleIdValuesN()
{
    AICommon::IDGenerator<N> generator;
    std::bitset<(1u << N)> bits;

    // allocate the max number of id's and check they are unique
    for (size_t i = 1; i < bits.size(); i++)
    {
        int id = generator.get();

        ASSERT_GT(id, 0);
        ASSERT_LT(id, (int)bits.size());
        ASSERT_FALSE(bits.test(id));

        bits.set(id);
    }

    // check all id's are allocated (except the zero'ed id)
    ASSERT_EQ(bits.count(), (bits.size() - 1));


    // free all the ids
    for (int i = 1; i < (int)bits.size(); i++)
    {
        ASSERT_TRUE(generator.put(i));
    }
    bits.reset();


    // try and allocate them all again
    for (size_t i = 1; i < bits.size(); i++)
    {
        int id = generator.get();

        ASSERT_GT(id, 0);
        ASSERT_LT(id, (int)bits.size());
        ASSERT_FALSE(bits.test(id));
        
        bits.set(id);
    }

    // check all id's are allocated (except the zero'ed id)
    ASSERT_EQ(bits.count(), (bits.size() - 1));


    // test that clear works
    generator.clear();



    // try and allocate them all again
    std::vector<int> ids;
    std::vector<int>::iterator it;
    ids.reserve(1u << N);
    for (size_t i = 1; i < bits.size(); i++)
    {
        int id = generator.get();

        ASSERT_GT(id, 0);
        ASSERT_LT(id, (int)bits.size());
        ASSERT_EQ(std::find(ids.begin(), ids.end(), id), ids.end());

        ids.push_back(id);
    }

    // setup the random shizzle
    std::random_device rd;
    // std::mt19937 seq(rd());

    // check put / get operations
    for (size_t n = 0; n < 100; n++)
    {
        const unsigned toRemove = rand() % (bits.size() - 1);

        // shuffle the ids
        std::shuffle(ids.begin(), ids.end(), rd);

        // free some id's
        unsigned putted = 0;
        it = ids.begin();
        while (putted <= toRemove)
        {
            ASSERT_TRUE(generator.put(*it));

            it = ids.erase(it);
            putted++;
        }

        // get some more ids
        unsigned getted = 0;
        while (getted <= toRemove)
        {
            int id = generator.get();

            ASSERT_GT(id, 0);
            ASSERT_LT(id, (int)bits.size());
            ASSERT_EQ(std::find(ids.begin(), ids.end(), id), ids.end());

            ids.push_back(id);
            getted++;
        }
    }
}

TEST(IDGeneratorTest, testAllPossibleIdValues)
{
    testAllPossibleIdValuesN<4>();
    testAllPossibleIdValuesN<5>();
    testAllPossibleIdValuesN<6>();
    testAllPossibleIdValuesN<7>();
    testAllPossibleIdValuesN<8>();
    testAllPossibleIdValuesN<9>();
    testAllPossibleIdValuesN<10>();
    testAllPossibleIdValuesN<11>();
    testAllPossibleIdValuesN<12>();
}


TEST(IDGeneratorTest, testWithOffsetIdValues)
{
    const unsigned offset = 123 + (rand() % 10000);
    const size_t _N = 8;

    AICommon::IDGenerator<_N> generator(offset);

    std::vector<int> ids;
    ids.reserve(1 << _N);

    // allocate the max number of id's and check they are unique
    for (size_t i = 1; i < (1 << _N); i++)
    {
        int id = generator.get();

        ASSERT_GT(id, (int)offset);
        ASSERT_LT(id, (int)(offset + (1 << _N)));
        ASSERT_EQ(std::find(ids.begin(), ids.end(), id), ids.end());

        ids.push_back(id);
    }

    // check all id's are allocated (except the zero'ed id)
    ASSERT_EQ(ids.size(), (size_t)((1 << _N) - 1));


    // free all the ids
    for (int id : ids)
    {
        ASSERT_TRUE(generator.put(id));
    }

}
