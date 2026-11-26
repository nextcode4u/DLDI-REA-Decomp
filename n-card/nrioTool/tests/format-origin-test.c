#include <assert.h>
#include "../format-origin.h"

int main(void)
{
    /* DSi System Menu install has no argv, unlike hbmenu. */
    assert(nrio_can_format(NULL, true, true, false, false));
    assert(nrio_can_format("", true, true, false, false));
    assert(nrio_can_format("sd:/nrioTool.nds", true, true, false, false));
    assert(!nrio_can_format("sd:/nrioTool.nds", true, false, false, false));
    assert(!nrio_can_format(NULL, true, false, true, true));
    assert(nrio_can_format("cart:/nrioTool.nds", true, true, false, false));
    assert(nrio_can_format("slot1:/nrioTool.nds", true, true, false, false));
    assert(nrio_can_format("fat:/nrioTool.nds", false, false, true, true));
    assert(!nrio_can_format("fat:/nrioTool.nds", false, false, false, true));
    assert(!nrio_can_format("fat:/nrioTool.nds", false, false, true, false));
    assert(!nrio_can_format("slot1:/nrioTool.nds", false, false, true, true));
    assert(!nrio_can_format(NULL, false, true, false, false));
}
