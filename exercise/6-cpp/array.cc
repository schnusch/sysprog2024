#include <cstdio>

class Array
{
public:
  // the single phantom byte is omitted for non-empty classes
  size_t size = 0;
  int *content = nullptr;

  void print_infos() const
  {
    printf("const: array: this=%p: size=%zu content=%p\n", (void *)this, size, (void *)content);
  }

  void print_infos()
  {
    printf("non-const: array: this=%p: size=%zu content=%p\n", (void *)this, size, (void *)content);
  }
};

Array a;

int main()
{
  printf("hello\n");
  a.print_infos();
  static_cast<Array const &>(a).print_infos();
  return 0;
}
