#include <cstdio>

class Foo
{
public:
  // the single phantom byte is omitted for non-empty classes
  char c;

  Foo() = default;

  Foo &operator = (Foo const &rhs) noexcept
  {
    printf("%s: this=%p, rhs=%p\n", __PRETTY_FUNCTION__, (void *)this, (void *)&rhs);
    c = rhs.c;
    return *this;
  }

  // intialized this->m to argument m
  Foo(unsigned char c) : c(c)
  {
    printf("Foo(c=%hhd): this=%p\n", c, (void *)this);
  }
};

// o's implicit constructor/destructor is called before/after main
Foo o(2);

/*
Beware of this code:
Foo o();

This is actually a function declaration.
*/

int main()
{
  printf("hello world\n");
  printf("sizeof(o) == %zd\n", sizeof(o));
  // global variables are in the bss segment, which is zero
  printf("o.c == %hhd\n", o.c);
  // `=` operator
  o = 3;
  printf("o.c == %hhd\n", o.c);
  // objects on stack may be uninitialized
  { size_t uninitialized = -1; }
  Foo l;
  printf("l.c == %hhd\n", l.c);
  return 0;
}
