/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef AAUDIO_FIXED_BLOCK_WRITER_H
#define AAUDIO_FIXED_BLOCK_WRITER_H

#include <stdint.h>

#include "FixedBlockAdapter.h"

/**
 * This can be used to convert a push data flow from variable sized buffers to fixed sized buffers.
 * An example would be an audio input callback.
 */
class FixedBlockWriter : public FixedBlockAdapter
{
public:
    FixedBlockWriter(FixedBlockProcessor &fixedBlockProcessor);

    virtual ~FixedBlockWriter() = default;

    int32_t writeToStorage(uint8_t *buffer, int32_t numBytes);

    /**
     * Write from a variable sized block.
     */
    int32_t processVariableBlock(uint8_t *buffer, int32_t numBytes) override;
};

#endif /* AAUDIO_FIXED_BLOCK_WRITER_H */
