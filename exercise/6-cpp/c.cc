#include <cstdio>

class Foo
{
};

// o's implicit constructor/destructor is called before/after main
Foo o;

int main()
{
  printf("hello world\n");
  // Because C++ guarantees the addresses of objects to differ a single byte
  // is allocated for empty classes.
  printf("sizeof(o) == %zd\n", sizeof(o));
  return 0;
}
