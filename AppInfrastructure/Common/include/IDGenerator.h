/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
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
/*
 * File:   IDGenerator.h
 *
 */
#ifndef IDGENERATOR_H
#define IDGENERATOR_H

#include <bitset>
#include <mutex>
#include <random>

#include <math.h>
#include <stdlib.h>

namespace AICommon
{

// -----------------------------------------------------------------------------
/**
 *  @class IDGenerator
 *  @brief Class used to generate unique numbers.
 *
 *  Why? there a few places in the code were we create some resource and then
 *  return a unique 'id' for it, this is typically done for resources created
 *  over dbus.
 *
 *  This class is guaranteed to return unique id numbers in a non-linear
 *  sequence.
 *
 *  The N template parameters refers to the number of bits in the generator
 *  range.  Avoid large values for N, as for each entry we allocate a bit to
 *  tell if it's in use or not, in addition in the worst case we have to
 *  iterate through all 2^N possible values to find a free one.
 *
 *  The api has a get() and a put() operation, obviously get() returns a new
 *  id and put() releases the id back to the pool.  The id's returned are
 *  not sequential, instead they are created using a pseudo random repeating
 *  sequence (fibonacci LFSR).
 *
 *  When the pool is exhausted get() will return -1.
 *
 */
template< std::size_t N >
class IDGenerator
{
    static_assert(((N >= 4) && (N <= 20)), "N template value is invalid (3 < N < 21)");

private:
    // The total number of possible values to generate
    static const unsigned mSize = (1u << N);

    // Polynomial values from https://users.ece.cmu.edu/~koopman/lfsr/index.html
    static const unsigned mPolynomial = (N == 4)  ? 0x9     :
                                        (N == 5)  ? 0x1B    :
                                        (N == 6)  ? 0x36    :
                                        (N == 7)  ? 0x5F    :
                                        (N == 8)  ? 0xE1    :
                                        (N == 9)  ? 0x1B0   :
                                        (N == 10) ? 0x3A6   :
                                        (N == 11) ? 0x574   :
                                        (N == 12) ? 0xC48   :
                                        (N == 13) ? 0x11D4  :
                                        (N == 14) ? 0x214E  :
                                        (N == 15) ? 0x41A6  :
                                        (N == 16) ? 0x84BE  :
                                        (N == 17) ? 0x1022E :
                                        (N == 18) ? 0x20196 :
                                        (N == 19) ? 0x4032F :
                                        (N == 20) ? 0x80534 : 0;

private:
    static unsigned getRandomSeed()
    {
        std::random_device rd;
        return rd();
    }

public:
    IDGenerator(unsigned offset = 0)
        : mOffset(offset)
        , mLfsr(1 + (getRandomSeed() % (mSize - 2)))
    { }

public:
    int get()
    {
        std::lock_guard<std::mutex> locker(mLock);

        // sanity check we haven't used up all the container ids
        if (mUsed.count() >= (mSize - 1))
            return -1;

        // use a fibonacci LFSR to cycle through the possible numbers rather
        // than just a random number generator or a sequential search
        do
        {
            unsigned lsb = (mLfsr & 0x1u);
            mLfsr >>= 1;

            if (lsb)
                mLfsr ^= mPolynomial;

        } while (mUsed.test(mLfsr) == true);


        // reserve the id and return it
        mUsed.set(mLfsr);
        return static_cast<int>(mOffset + mLfsr);
    }

    bool put(int id)
    {
        std::lock_guard<std::mutex> locker(mLock);

        // sanity check the id is within the valid range
        if ((id < 0) || (id <= static_cast<int>(mOffset)) ||
            ((id - mOffset) >= static_cast<int>(mSize)))
            return false;

        // sanity check the bit is set in the 'used' set
        if (!mUsed.test(id - mOffset))
            return false;

        mUsed.reset(id - mOffset);
        return true;
    }

    void clear()
    {
        std::lock_guard<std::mutex> locker(mLock);

        // clear all the 'used' bits which effectively makes all id's available
        // again
        mUsed.reset();
    }

private:
    const unsigned mOffset;
    unsigned mLfsr;

    mutable std::mutex mLock;
    std::bitset<mSize> mUsed;
};


} // namespace AICommon


#endif // !defined(IDGENERATOR_H)
