#include <cstdio>

class Foo
{
  // the single phantom byte is omitted for non-empty classes
  char c;
};

// o's implicit constructor/destructor is called before/after main
Foo o;

int main()
{
  printf("hello world\n");
  printf("sizeof(o) == %zd\n", sizeof(o));
  return 0;
}
