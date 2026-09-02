#include "RankMetadataMergePolicy.h"

#include "DomainObject.h"
#include "FullDbEnumUtil.h"

namespace Dic::Module::Timeline {
bool RankMetadataMergePolicy::ShouldInclude(const std::string &metaType, bool representative) {
    if (representative) {
        return true;
    }
    const auto overlap = Dic::Protocol::ENUM_TO_STR(PROCESS_TYPE::OVERLAP_ANALYSIS).value_or("OVERLAP_ANALYSIS");
    return metaType != overlap;
}
}
