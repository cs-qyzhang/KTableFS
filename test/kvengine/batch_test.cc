#include <iostream>
#include "kvengine/batch.h"
#include "kvengine/slice.h"

using namespace std;
using kvengine::Slice;
using kvengine::Batch;
using kvengine::Respond;

void callback(Respond* respond, std::string name, int a, int b) {
  cout << respond->Result() << " " << name << " " << a << " + " << b << " = " << a + b << endl;
}

void scan(const Slice* s, int a, int b) {
  cout << s->data() << " " << a << " " << b << endl;
}

int main(void) {
  Slice key("this is key");
  Slice value("this is value");

  {
    Batch batch;
    batch.Put(Slice("put key"), Slice("put value"));
    batch.Delete(Slice("delete key"));
    batch.Get(Slice("get key"));
    batch.Scan(Slice("min"), Slice("max"), scan, 1, 2);
    batch.AddCallback(callback, string("name"), 10, 21);
  }

  cout << "finish" << endl;

  return 0;
}