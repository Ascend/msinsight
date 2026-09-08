/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
#include "NumaPlugin.h"

namespace Dic::Module::Numa {
std::unique_ptr<Module::ProtocolUtil> NumaPlugin::GetProtocolUtil() {
    return std::make_unique<Protocol::NumaProtocolUtil>();
}
} // namespace Dic::Module::Numa
