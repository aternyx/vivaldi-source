Use of original work by Vivaldi Technologies contained in this source code
package is governed by a BSD-style license that can be found in the LICENSE
file. Other works are governed by their original licensing terms.

Tool installation instructions for your platform can be found at https://www.chromium.org/developers/how-tos/get-the-code

# Build instructions (bash shell assumed):

### for Linux
```./chromium/build/install-build-deps.sh --no-syms --no-arm --no-chromeos-fonts --no-nacl --no-prompt```

### for Windows
```export DEPOT_TOOLS_WIN_TOOLCHAIN=0
export PATH="$PATH":$PWD/chromium/third_party/depot_tools/```

some chromium scripts require a git repo to work
```git init
python3 scripts/runhooks.py
ninja -C out/Release vivaldi```