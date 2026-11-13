// Isolated Z80 interpreter diagnostic. No PCM/FM voices or audio output.
#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>
#include <iostream>
#include "sound_ports.h"
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
namespace sz {
#include "emu.h"
#include "logmacro.h"
#include "z80.cpp"
#include "z80dasm.cpp"
}
