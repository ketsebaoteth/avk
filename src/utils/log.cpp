#include "utils/log.h"
#include <cstdarg>
#include <cstdio>

namespace atomic {
void log_error(const char *message) {
  if (!message)
    return;
  std::fprintf(stderr, "avk error: %s\n", message);
  std::fflush(stderr);
}

void log_error_fmt(const char *fmt, ...) {
  if (!fmt)
    return;
  std::fprintf(stderr, "avk error: ");

  std::va_list args;
  va_start(args, fmt);
  std::vfprintf(stderr, fmt, args);
  va_end(args);

  std::fprintf(stderr, "\n");
  std::fflush(stderr);
}

} // namespace atomic
