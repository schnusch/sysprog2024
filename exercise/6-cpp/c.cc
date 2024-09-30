#include <cstdio>

class Foo
{
public:
  // the single phantom byte is omitted for non-empty classes
  char c;

  Foo() = default;

  // intialized this->m to argument m
  Foo(unsigned char c) : c(c) {}
};

// o's implicit constructor/destructor is called before/after main
Foo o;

int main()
{
  printf("hello world\n");
  printf("sizeof(o) == %zd\n", sizeof(o));
  // global variables are in the bss segment, which is zero
  printf("o.c == %hhd\n", o.c);
  // objects on stack may be uninitialized
  Foo l(2);
  printf("l.c == %hhd\n", l.c);
  return 0;
}
