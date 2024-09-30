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

  Array() = default;
  explicit Array(size_t size) : size(size), content(new int[size]) { }

  // RULE OF SIX
  // copy constructor
  Array(const Array &rhs) : size(rhs.size), content(new int[size])
  {
    for(size_t i = 0; i < size; ++i) {
      content[i] = rhs.content[i];
    }
  }
  // copy assignment operator
  Array operator=(const Array &rhs)
  {
    size = rhs.size;
    content = new int[size];
    for(size_t i = 0; i < size; ++i) {
      content[i] = rhs.content[i];
    }
    return *this;
  };
  // move constructor (rvalue reference)
  Array(Array &&) = delete;
  // move assignment operator (rvalue reference)
  void operator=(Array &&) = delete;

  ~Array()
  {
    delete[] content;
  }
};

Array a;

int main()
{
  printf("hello\n");
  a.print_infos();

  Array b(10);
  b.print_infos();

  Array c = b;
  c.print_infos();

  Array d;
  d = c;
  d.print_infos();

  return 0;
}
