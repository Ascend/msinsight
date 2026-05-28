#include "TritonPlugin.h"
#include "TritonProtocol.h"
namespace Dic::Module::Triton {
std::unique_ptr<Module::ProtocolUtil> TritonPlugin::GetProtocolUtil() {
    return std::make_unique<Dic::Protocol::TritonProtocolUtil>();
}
} // namespace Dic::Module::Triton
