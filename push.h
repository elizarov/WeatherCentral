#ifndef PUSH_H
#define PUSH_H

#include <WProgram.h>

#define MAX_PUSH_ID 30

class PushItem {
public:
  int id;    // feed id
  int val;   // value
  int prec;  // precision

  inline PushItem() {}

  inline PushItem(int _id, int _val, int _prec) :
    id(_id), val(_val), prec(_prec) {}
};

extern void initPush();
extern void checkPush();
extern void push(const PushItem& item);

#endif

