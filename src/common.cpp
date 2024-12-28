#include <cstdarg>
#include <cstdio>
#include <cstdlib>

void panic(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  fflush(stdout);

  fprintf(stderr, "Panic: ");
  vfprintf(stderr, msg, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  std::abort();
}
