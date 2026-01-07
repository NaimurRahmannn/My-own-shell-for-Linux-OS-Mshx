#include "../mshX.h"

struct builtin_s builtins[] =
{   
    { "cd"      , cd         },
    { "dump"    , dump       },
    { "dry"     , dry        },
};

int builtins_count = sizeof(builtins)/sizeof(struct builtin_s);