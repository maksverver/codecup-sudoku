#ifndef CHECK_H_INCLUDED
#define CHECK_H_INCLUDED

[[noreturn]] void CheckFail(const char *file, int line, const char *expr);

#ifdef NDEBUG
# define CHECK(x) (x)
#else
# define CHECK(x) if (!(x)) [[unlikely]] CheckFail(__FILE__, __LINE__, #x);
#endif

#endif  //ndef CHECK_H_INCLUDED
