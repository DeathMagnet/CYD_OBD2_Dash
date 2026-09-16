#include "system/dtc_lookup.h"
#include "system/dtc_table.h"
#include <string.h>

const char* lookupDtcDescription(const char* code) {
    size_t lo = 0;
    size_t hi = kDtcTableCount;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = strcmp(code, kDtcTable[mid].code);
        if (cmp == 0) {
            return kDtcTable[mid].description;
        }
        if (cmp < 0) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return nullptr;
}
