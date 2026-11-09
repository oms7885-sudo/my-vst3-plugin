// Checks the parameter table (Source/Parameters.h): unique ids, sane ranges and defaults,
// choice counts matching their option lists, known sections, a tooltip for every control.
#include "../Source/Parameters.h"
#include <cstdio>
#include <cstring>

int main()
{
    using namespace bsyn::spec;
    int bad = 0;
    for (int i = 0; i < numParams; ++i)
    {
        const Param& p = params[i];
        for (int j = i + 1; j < numParams; ++j)
            if (std::strcmp (p.id, params[j].id) == 0) { std::printf ("duplicate id %s\n", p.id); ++bad; }
        if (! (p.min < p.max) || p.def < p.min || p.def > p.max) { std::printf ("bad range/default %s\n", p.id); ++bad; }
        if (p.kind == Kind::Choice)
        {
            int n = p.choices ? 1 : 0;
            for (const char* c = p.choices; c && *c; ++c) if (*c == '|') ++n;
            if (n != (int) p.max + 1 || p.min != 0.0f) { std::printf ("choice count %s\n", p.id); ++bad; }
        }
        if (p.kind == Kind::Bool && (p.min != 0.0f || p.max != 1.0f)) { std::printf ("bool range %s\n", p.id); ++bad; }
        bool known = false;
        for (const char* s : sections) known = known || std::strcmp (s, p.section) == 0;
        if (! known) { std::printf ("unknown section %s\n", p.id); ++bad; }
        if (p.tip == nullptr || std::strchr (p.tip, '\n') == nullptr) { std::printf ("missing tooltip %s\n", p.id); ++bad; }
        if (find (p.id) != &p) { std::printf ("find() fails %s\n", p.id); ++bad; }
    }
    std::printf ("%d parameters, %d problems\n", numParams, bad);
    if (bad == 0) std::printf ("OK\n");
    return bad == 0 ? 0 : 1;
}
