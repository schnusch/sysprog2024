#include <cstdio>

class Foo
{
public:
  // the single phantom byte is omitted for non-empty classes
  size_t size = 0;

  Foo() = default;

  Foo &operator = (Foo const &rhs) noexcept
  {
    printf("%s: this=%p, rhs=%p\n", __PRETTY_FUNCTION__, (void *)this, (void *)&rhs);
    size = rhs.size;
    return *this;
  }

  // intialized this->m to argument m
  explicit Foo(unsigned char size) : size(size)
  {
    printf("Foo(size=%hhd): this=%p\n", size, (void *)this);
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
  printf("o.size == %zu\n", o.size);
  // `=` operator
  o = Foo(3);
  printf("o.size == %zu\n", o.size);
  // does not first create a Foo with default constructor, deletes it, and
  // then uses `=` operator. (copy elision)
  Foo l = Foo(4);
  printf("l.size == %zu\n", l.size);
  return 0;
}
