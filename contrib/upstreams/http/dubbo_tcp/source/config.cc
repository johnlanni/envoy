#include "config.h"

#include "upstream_request.h"

namespace Envoy {
namespace Extensions {
namespace Upstreams {
namespace Http {
namespace DubboTcp {

Router::GenericConnPoolPtr DubboTcpGenericConnPoolFactory::createGenericConnPool(
    Upstream::HostConstSharedPtr host, Upstream::ThreadLocalCluster& thread_local_cluster,
    UpstreamProtocol, Upstream::ResourcePriority priority, absl::optional<Envoy::Http::Protocol>,
    Upstream::LoadBalancerContext* ctx, const Protobuf::Message&) const {
  auto ret = std::make_unique<TcpConnPool>(host, thread_local_cluster, priority, ctx);
  return (ret->valid() ? std::move(ret) : nullptr);
}

REGISTER_FACTORY(DubboTcpGenericConnPoolFactory, Router::GenericConnPoolFactory);

} // namespace DubboTcp
} // namespace Http
} // namespace Upstreams
} // namespace Extensions
} // namespace Envoy
