/*
 * Copyright 2017 The Android Open Source Project
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

#ifndef SIMPLESYNTH_TRACE_H
#define SIMPLESYNTH_TRACE_H

class Trace {

public:
  static void beginSection(const char *format, ...);
  static void endSection();
  static bool isEnabled(){ return is_enabled_; };
  static void initialize();

private:
  static bool is_enabled_;
  static bool has_error_been_shown_;
};

#endif //SIMPLESYNTH_TRACE_H
