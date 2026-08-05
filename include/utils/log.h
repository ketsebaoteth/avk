#pragma once

namespace atomic {

void log_error(const char *message);

// Rich string formatting variant (printf style)
void log_error_fmt(const char *fmt, ...);

} // namespace atomic
