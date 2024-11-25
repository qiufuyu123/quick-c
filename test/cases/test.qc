// #include "../../libqc/io.qh"

typedef struct{
  i32 a;
  u64 b;
}t1_t;

typedef struct{
  i32 a;
  i32 b;
  t1_t tt[1];
}t2_t;

i8 main(u32 bus,u32 slot,u32 func){

  t2_t aaa;
  t2_t *ptr = &aaa;
  ptr.a=1;
  ptr.b=2;
  ptr->tt[0].a=3;
  ptr->tt[0].b=4;
  &ptr->tt[1];
  // printf("%x %x\n",ptr.a,ptr.b);
  // printf("%x %x\n",ptr->tt[0].a,ptr->tt[0].b  );
  return 1;
}