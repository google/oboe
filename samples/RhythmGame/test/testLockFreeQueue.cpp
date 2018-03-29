/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include "utils/LockFreeQueue.h"

/**
 * Tests
 * =====
 *
 * READ/POP:
 *  - if queue is empty return false
 *  - if queue has elements return true
 *  - returns correct value once
 *  - returns correct value more than once
 *
 * WRITE/PUSH:
 *  - if queue is full return false
 *  - if queue has space return true
 *
 * WRAP:
 *  - read from full queue, then write one more element, then correct read
 *
 * PEEK:
 *  - peek once
 *  - peek twice
 *
 * SIZE:
 *  - correct zero
 *  - correct once
 *  - correct many
 *
 * CANNOT TEST:
 *  - read/writes correct after wraparound (takes too long to run)
 *  - Constructing queue with non power of 2 capacity results in assert failure (needs death test)
 *  - Constructing queue with signed type results in assert failure (needs death test)
 *  - Multi-threaded read/writes (complicated to test)
 *
 */

constexpr int kQueueCapacity = 2;

class TestLockFreeQueue : public ::testing::Test {

public:

    int result1;
    int result2;
    int result3;
    LockFreeQueue<int, kQueueCapacity> q;
};

TEST_F(TestLockFreeQueue, PopReturnsFalseWhenEmpty){
    bool result = q.pop(result1);
    ASSERT_EQ(result, false);
}

TEST_F(TestLockFreeQueue, PopReturnsTrueWhenNotEmpty){
    q.push(123);
    bool result = q.pop(result1);
    ASSERT_EQ(result, true);
}

TEST_F(TestLockFreeQueue, PopReturnsOneCorrectValue){
    q.push(123);
    q.pop(result1);
    ASSERT_EQ(result1, 123);
}

TEST_F(TestLockFreeQueue, PopReturnsManyCorrectValues){
    q.push(123);
    q.push(456);
    q.pop(result1);
    q.pop(result2);
    ASSERT_EQ(result1, 123);
    ASSERT_EQ(result2, 456);
}

TEST_F(TestLockFreeQueue, PushWhenFullReturnsFalse){
    q.push(123);
    q.push(123);
    ASSERT_EQ(q.push(123), false);
}

TEST_F(TestLockFreeQueue, PushWhenSpaceAvailableReturnsTrue){
    ASSERT_EQ(q.push(123), true);
}

TEST_F(TestLockFreeQueue, PushHandlesWrapCorrectly){
    q.push(123);
    q.push(234);
    q.pop(result1);
    q.push(999);
    q.pop(result2);
    ASSERT_EQ(result2, 234);
    q.pop(result3);
    ASSERT_EQ(result3, 999);
}

TEST_F(TestLockFreeQueue, PeekWhenEmptyReturnsFalse){
    ASSERT_EQ(q.peek(result1), false);
}

TEST_F(TestLockFreeQueue, PeekWhenNotEmptyReturnsTrue){
    q.push(123);
    ASSERT_EQ(q.peek(result1), true);
}

TEST_F(TestLockFreeQueue, PeekReturnsCorrectValueOnce){
    q.push(456);
    q.peek(result1);
    ASSERT_EQ(result1, 456);
}

TEST_F(TestLockFreeQueue, PeekReturnsCorrectValueTwice){
    q.push(456);
    q.peek(result1);
    q.peek(result2);
    ASSERT_EQ(result2, 456);
}

TEST_F(TestLockFreeQueue, SizeReturnsZeroAfterInit){
    ASSERT_EQ(q.size(), 0);
}

TEST_F(TestLockFreeQueue, SizeIsOneAfterPushOnce){
    q.push(321);
    ASSERT_EQ(q.size(), 1);
}

TEST_F(TestLockFreeQueue, SizeIsCorrectAfterPushingMaxItems){
    for (int i = 0; i < kQueueCapacity; ++i) { q.push(i); }
    ASSERT_EQ(q.size(), kQueueCapacity);
}

TEST_F(TestLockFreeQueue, SizeCorrectAfterWriteCounterWraps){

    const uint32_t kCapacity = (uint32_t) 1 << 7;
    LockFreeQueue<int, kCapacity, uint8_t> myQ;

    for (int i = 0; i < UINT8_MAX; ++i) {
        myQ.push(i);
        myQ.pop(result1);
    }

    myQ.push(1);
    ASSERT_EQ(myQ.size(), 1);
}