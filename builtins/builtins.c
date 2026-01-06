#include "../mshX.h"

struct builtin_s builtins[] =
{   
    { "cd"      , cd         },
    { "dump"    , dump       },
};

int builtins_count = sizeof(builtins)/sizeof(struct builtin_s);