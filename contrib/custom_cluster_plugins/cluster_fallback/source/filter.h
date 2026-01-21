#pragma once

#include <string>
#include <vector>

#include "contrib/envoy/extensions/custom_cluster_plugins/cluster_fallback/v3/cluster_fallback.pb.h"
#include "contrib/envoy/extensions/custom_cluster_plugins/cluster_fallback/v3/cluster_fallback.pb.validate.h"

#include "envoy/router/cluster_specifier_plugin.h"
#include "envoy/upstream/cluster_manager.h"

#include "source/common/common/logger_impl.h"
#include "source/common/common/logger.h"

namespace Envoy {
namespace Extensions {
namespace CustomClusterPlugins {
namespace ClusterFallback {

class ClusterFallbackPlugin : public Envoy::Router::ClusterSpecifierPlugin,
                              public Logger::Loggable<Logger::Id::router> {
public:
  ClusterFallbackPlugin(
      const envoy::extensions::custom_cluster_plugins::cluster_fallback::v3::ClusterFallbackConfig&
          config,
      Server::Configuration::CommonFactoryContext& context);

  // ClusterSpecifierPlugin interface - required pure virtual method
  // This is used when ClusterFallbackPlugin is the primary cluster specifier (non-weighted cluster routes)
  Envoy::Router::RouteConstSharedPtr route(Envoy::Router::RouteEntryAndRouteConstSharedPtr parent,
                                           const Http::RequestHeaderMap& headers,
                                           const StreamInfo::StreamInfo& /*stream_info*/,
                                           uint64_t /*random*/) const override;

  // Fallback route method for weighted cluster scenarios
  // This is used when ClusterFallbackPlugin is a fallback plugin on WeightedClusterSpecifierPlugin
  Envoy::Router::RouteConstSharedPtr route(Envoy::Router::RouteConstSharedPtr route,
                                           const Http::RequestHeaderMap& headers) const override;

private:
  bool hasHealthHost(absl::string_view cluster_name) const;
  Envoy::Router::RouteConstSharedPtr
  calculateWeightedClusterFallback(Envoy::Router::RouteConstSharedPtr route,
                                   const std::string& cluster_name) const;
  Envoy::Router::RouteConstSharedPtr
  calculateNormalClusterFallback(Envoy::Router::RouteConstSharedPtr route,
                                 const std::string& original_cluster) const;

  Upstream::ClusterManager& cluster_manager_;
  std::unordered_map<std::string/*routing cluster*/, std::vector<std::string>/*fallback clusters*/> clusters_config_;
};

} // namespace ClusterFallback
} // namespace CustomClusterPlugins
} // namespace Extensions
} // namespace Envoy
