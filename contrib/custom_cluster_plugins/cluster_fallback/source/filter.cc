#include "contrib/custom_cluster_plugins/cluster_fallback/source/filter.h"

#include "source/common/common/assert.h"
#include "source/common/router/delegating_route_impl.h"

namespace Envoy {
namespace Extensions {
namespace CustomClusterPlugins {
namespace ClusterFallback {

ClusterFallbackPlugin::ClusterFallbackPlugin(
    const envoy::extensions::custom_cluster_plugins::cluster_fallback::v3::ClusterFallbackConfig&
        config,
    Server::Configuration::CommonFactoryContext& context)
    : cluster_manager_(context.clusterManager()) {
  if (config.config_specifier_case() ==
      envoy::extensions::custom_cluster_plugins::cluster_fallback::v3::ClusterFallbackConfig::
          kWeightedClusterConfig) {
    for (auto& item : config.weighted_cluster_config().config()) {
      clusters_config_.emplace(item.routing_cluster(),
                               std::vector<std::string>(item.fallback_clusters().begin(),
                                                        item.fallback_clusters().end()));
    }
  } else {
    clusters_config_.emplace(
        config.cluster_config().routing_cluster(),
        std::vector<std::string>(config.cluster_config().fallback_clusters().begin(),
                                 config.cluster_config().fallback_clusters().end()));
  }

  if (clusters_config_.empty()) {
    ENVOY_LOG(info, "there is no fallback cluster");
  }
}

// 4-parameter route method - used when ClusterFallbackPlugin is the primary cluster specifier
Envoy::Router::RouteConstSharedPtr
ClusterFallbackPlugin::route(Envoy::Router::RouteEntryAndRouteConstSharedPtr parent,
                             const Http::RequestHeaderMap& /*headers*/,
                             const StreamInfo::StreamInfo& /*stream_info*/,
                             uint64_t /*random*/) const {
  if (parent == nullptr || parent->routeEntry() == nullptr) {
    ENVOY_LOG(warn, "parent route or route entry is null, returning original route");
    return parent;
  }

  // For non-weighted cluster routes, clusters_config_ has exactly 1 entry
  // Use the same fallback logic as the 2-parameter version
  if (clusters_config_.size() > 1) {
    return calculateWeightedClusterFallback(parent, parent->routeEntry()->clusterName());
  }
  return calculateNormalClusterFallback(parent, parent->routeEntry()->clusterName());
}

// 2-parameter route method - used as fallback plugin on WeightedClusterSpecifierPlugin
Envoy::Router::RouteConstSharedPtr
ClusterFallbackPlugin::route(Envoy::Router::RouteConstSharedPtr route,
                             const Http::RequestHeaderMap& /*headers*/) const {
  if (route == nullptr || route->routeEntry() == nullptr) {
    ENVOY_LOG(warn, "route or route entry is null, returning original route");
    return route;
  }

  const auto* route_entry = route->routeEntry();
  const std::string& cluster_name = route_entry->clusterName();

  // This 2-parameter route() is ONLY called from WeightedClusterSpecifierPlugin,
  // so we always use weighted cluster fallback logic. This ensures that if a cluster
  // is not in the fallback config, we return the original route unchanged.
  return calculateWeightedClusterFallback(route, cluster_name);
}

Envoy::Router::RouteConstSharedPtr ClusterFallbackPlugin::calculateNormalClusterFallback(
    Envoy::Router::RouteConstSharedPtr route, const std::string& original_cluster) const {
  ASSERT(clusters_config_.size() == 1);

  auto first_item = clusters_config_.begin();
  if (hasHealthHost(first_item->first)) {
    ENVOY_LOG(info, "The target cluster {} has healthy nodes and does not require fallback",
              first_item->first);
    // Return route with original/configured cluster if healthy
    if (original_cluster == first_item->first) {
      return route;
    }
    return std::make_shared<Envoy::Router::DynamicRouteEntry>(std::move(route), 
                                                               std::string(first_item->first));
  }

  for (const auto& cluster_name : first_item->second) {
    if (hasHealthHost(cluster_name)) {
      ENVOY_LOG(info, "Falling back to cluster {}", cluster_name);
      return std::make_shared<Envoy::Router::DynamicRouteEntry>(std::move(route),
                                                                 std::string(cluster_name));
    }
  }

  ENVOY_LOG(info, "All clusters have no healthy nodes, the original routing cluster is returned");
  return std::make_shared<Envoy::Router::DynamicRouteEntry>(std::move(route),
                                                             std::string(first_item->first));
}

Envoy::Router::RouteConstSharedPtr ClusterFallbackPlugin::calculateWeightedClusterFallback(
    Envoy::Router::RouteConstSharedPtr route, const std::string& cluster_name) const {
  auto search = clusters_config_.find(cluster_name);
  if (search == clusters_config_.end()) {
    ENVOY_LOG(warn, "there is no fallback cluster config for {}, returning original route",
              cluster_name);
    return route;
  }

  if (hasHealthHost(search->first)) {
    ENVOY_LOG(info, "The target cluster {} has healthy nodes and does not require fallback",
              search->first);
    return route;
  }

  for (const auto& fallback_cluster : search->second) {
    if (hasHealthHost(fallback_cluster)) {
      ENVOY_LOG(info, "Falling back from {} to cluster {}", cluster_name, fallback_cluster);
      return std::make_shared<Envoy::Router::DynamicRouteEntry>(std::move(route),
                                                                 std::string(fallback_cluster));
    }
  }

  ENVOY_LOG(info, "All clusters have no healthy nodes, the original routing cluster is returned");
  return route;
}

bool ClusterFallbackPlugin::hasHealthHost(absl::string_view cluster_name) const {
  bool has_health_host{false};
  Upstream::ThreadLocalCluster* cluster = cluster_manager_.getThreadLocalCluster(cluster_name);
  if (!cluster) {
    return has_health_host;
  }

  for (auto& i : cluster->prioritySet().hostSetsPerPriority()) {
    if (i->healthyHosts().size() > 0) {
      has_health_host = true;
      break;
    }
  }

  return has_health_host;
}

} // namespace ClusterFallback
} // namespace CustomClusterPlugins
} // namespace Extensions
} // namespace Envoy
