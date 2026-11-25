#include <assert.h>
#include "../format-origin.h"

int main(void)
{
    assert(nrio_sd_launch_path("sd:/nrioTool.nds"));
    assert(nrio_sd_launch_path("sdmc:/nrioTool.nds"));
    assert(!nrio_sd_launch_path("fat:/nrioTool.nds"));
    assert(!nrio_sd_launch_path("slot1:/nrioTool.nds"));
    assert(!nrio_sd_launch_path("sd:/"));
    assert(!nrio_sd_launch_path(""));
    assert(!nrio_sd_launch_path(0));
    for (unsigned mask = 0; mask < 16; ++mask) {
        const bool expected = (mask & 3) == 3 || (mask & 12) == 12;
        assert(nrio_can_format(mask & 1, mask & 2, mask & 4, mask & 8) == expected);
    }
}
