#include <cstdio>

class Foo
{
};

// o's implicit constructor/destructor is called before/after main
Foo o;

int main()
{
  printf("hello world\n");
  return 0;
}
