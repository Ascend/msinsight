#ifndef PROFILER_SERVER_RANKMETADATAMERGEPOLICY_H
#define PROFILER_SERVER_RANKMETADATAMERGEPOLICY_H

#include <string>

namespace Dic::Module::Timeline {
class RankMetadataMergePolicy {
  public:
    static bool ShouldInclude(const std::string &metaType, bool representative);
};
}

#endif
