# KTableFS

KVell + TableFS

## trick

### nop()

```C
static inline void nop() {
  // volatile variable won't be optimized
  for (volatile int i = 0; i < 1000; ++i) ;
}
```
