// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ANONYMOUS_TOKENS_CPP_CRYPTO_VERIFIER_H_
#define ANONYMOUS_TOKENS_CPP_CRYPTO_VERIFIER_H_

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

class Verifier {
 public:
  virtual absl::Status Verify(absl::string_view signature,
                              absl::string_view message) = 0;

  virtual ~Verifier() = default;
};

#endif  // ANONYMOUS_TOKENS_CPP_CRYPTO_VERIFIER_H_
