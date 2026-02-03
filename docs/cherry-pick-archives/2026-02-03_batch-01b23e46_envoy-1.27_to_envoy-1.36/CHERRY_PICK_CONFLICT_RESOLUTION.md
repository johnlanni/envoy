# Cherry-pick Conflict Resolution Document

## Overview

**Commit**: cc9ebb23d12878f114d64d9f83a502facc073f80  
**Author**: zty98751 <zty98751@alibaba-inc.com>  
**Date**: Tue Jan 20 17:20:05 2026 +0800  
**Title**: feat: add global request limit per I/O cycle with Wasm foreign function support  
**Source Branch**: envoy-1.27  
**Target Branch**: envoy-1.36  

**Number of Conflicted Files**: 2

## Summary

This commit introduces a global request rate limiting mechanism that allows dynamic control of the maximum number of requests across all connections in a single I/O cycle through runtime configuration or Wasm modules. The conflicts arise from architectural differences between envoy-1.27 and envoy-1.36 in the `ConnectionManagerImpl` class.

**Key User Requirement**: Replace all `ALIMESH` macros with `HIGRESS` macros.

## Conflicted Files

### 1. source/common/http/conn_manager_impl.cc

**Conflict Locations**:
- Line 72-88: Runtime key declarations and thread_local variables
- Line 155-184: Constructor initialization list
- Line 2569-2579: `onDeferredRequestProcessing` method

**Risk Level**: ⚠️ Medium-High

### 2. source/common/http/conn_manager_impl.h

**Conflict Locations**:
- Line 151-159: Static member declaration
- Line 715-739: Thread-local member variables

**Risk Level**: ⚠️ Medium

## Detailed Conflict Analysis

### Conflict 1: Runtime keys and thread_local variables (conn_manager_impl.cc:72-88)

**Location**: After `MaxRequestsPerIoCycle` declaration

**HEAD (envoy-1.36)**:
```cpp
const absl::string_view ConnectionManagerImpl::MaxRequestsPerIoCycle =
    "http.max_requests_per_io_cycle";
// Don't attempt to intelligently delay close: https://github.com/envoyproxy/envoy/issues/30010
const absl::string_view ConnectionManagerImpl::OptionallyDelayClose =
    "http1.optionally_delay_close";
```

**Incoming (patch)**:
```cpp
const absl::string_view ConnectionManagerImpl::MaxRequestsPerIoCycle =
    "http.max_requests_per_io_cycle";
#if defined(ALIMESH)
// Runtime key for global maximum number of requests that can be processed from all connections
// per I/O cycle on this thread. Requests over this limit are deferred until the next I/O cycle.
const absl::string_view ConnectionManagerImpl::MaxTotalRequestsPerIoCycle =
    "http.max_total_requests_per_io_cycle";

// Initialize thread_local variables for global request limiting.
thread_local uint64_t ConnectionManagerImpl::global_requests_during_current_event_loop_ = 0;
thread_local uint64_t ConnectionManagerImpl::global_max_requests_per_io_cycle_ = UINT64_MAX;
thread_local bool ConnectionManagerImpl::global_reset_watchers_registered_ = false;
#endif
```

**Conflict Type**: Insertion conflict - envoy-1.36 added `OptionallyDelayClose`, patch adds global request limiting code

**Analysis**:
- envoy-1.36 introduced a new runtime key `OptionallyDelayClose` that doesn't exist in envoy-1.27
- The patch adds new global request limiting functionality with `ALIMESH` macro guards
- Both additions are independent and can coexist

**Resolution**:
✅ Keep both additions:
1. Keep the `OptionallyDelayClose` from envoy-1.36
2. Add the global request limiting code from the patch (with `ALIMESH` → `HIGRESS`)
3. Place the conditional code after `OptionallyDelayClose`

**Action**: Manual merge required

---

### Conflict 2: Constructor initialization (conn_manager_impl.cc:155-184)

**Location**: End of `ConnectionManagerImpl` constructor initialization list

**HEAD (envoy-1.36)**:
```cpp
      max_requests_during_dispatch_(
          runtime_.snapshot().getInteger(ConnectionManagerImpl::MaxRequestsPerIoCycle, UINT32_MAX)),
      direction_(direction),
      allow_upstream_half_close_(Runtime::runtimeFeatureEnabled(
          "envoy.reloadable_features.allow_multiplexed_upstream_half_close")) {
  ENVOY_LOG_ONCE_IF(
      trace, accept_new_http_stream_ == nullptr,
      "LoadShedPoint envoy.load_shed_points.http_connection_manager_decode_headers is not "
      "found. Is it configured?");
  ENVOY_LOG_ONCE_IF(trace, hcm_ondata_creating_codec_ == nullptr,
                    "LoadShedPoint envoy.load_shed_points.hcm_ondata_creating_codec is not found. "
                    "Is it configured?");
}
```

**Incoming (patch)**:
```cpp
      max_requests_during_dispatch_(
          runtime_.snapshot().getInteger(ConnectionManagerImpl::MaxRequestsPerIoCycle, UINT32_MAX)),
      refresh_rtt_after_request_(
#if defined(ALIMESH)
          Runtime::runtimeFeatureEnabled("envoy.reloadable_features.refresh_rtt_after_request")) {
  // Initialize global request limit from runtime configuration.
  // Runtime value always takes priority if configured (not UINT64_MAX).
  // This allows runtime to override values set via setGlobalMaxRequestsPerIoCycle().
  uint64_t runtime_value =
      runtime_.snapshot().getInteger(ConnectionManagerImpl::MaxTotalRequestsPerIoCycle, UINT64_MAX);
  if (runtime_value != UINT64_MAX) {
    global_max_requests_per_io_cycle_ = runtime_value;
  }
}
#else
          Runtime::runtimeFeatureEnabled("envoy.reloadable_features.refresh_rtt_after_request")) {
}
#endif
```

**Conflict Type**: Structural difference - envoy-1.36 has different member variables at the end

**Analysis**:
- envoy-1.36 has `direction_` and `allow_upstream_half_close_` members that don't exist in envoy-1.27
- envoy-1.36 has ENVOY_LOG_ONCE_IF statements in constructor body
- ⚠️ **IMPORTANT**: `refresh_rtt_after_request_` is an envoy-1.27-specific feature that doesn't exist in envoy-1.36
- The patch wraps `refresh_rtt_after_request_` initialization with `#if defined(ALIMESH)` and adds global limit initialization in the ALIMESH branch
- **For envoy-1.36**: We don't need to migrate `refresh_rtt_after_request_` logic, only extract the global limit initialization code

**Resolution**:
✅ Simple adaptation:
1. Keep envoy-1.36's constructor initialization list unchanged (ending with `allow_upstream_half_close_`)
2. Keep envoy-1.36's ENVOY_LOG_ONCE_IF statements
3. After the ENVOY_LOG_ONCE_IF statements, add **only the global limit initialization code** from the patch:
   ```cpp
   #if defined(HIGRESS)
     // Initialize global request limit from runtime configuration.
     // Runtime value always takes priority if configured (not UINT64_MAX).
     // This allows runtime to override values set via setGlobalMaxRequestsPerIoCycle().
     uint64_t runtime_value =
         runtime_.snapshot().getInteger(ConnectionManagerImpl::MaxTotalRequestsPerIoCycle, UINT64_MAX);
     if (runtime_value != UINT64_MAX) {
       global_max_requests_per_io_cycle_ = runtime_value;
     }
   #endif
   ```
4. Do NOT add `refresh_rtt_after_request_` member or initialization

**Action**: Extract and add only global limit initialization code

---

### Conflict 3: onDeferredRequestProcessing method (conn_manager_impl.cc:2569-2579)

**Location**: `ActiveStream::onDeferredRequestProcessing` method

**HEAD (envoy-1.36)**:
```cpp
  state_.deferred_to_next_io_iteration_ = false;
  bool end_stream = state_.deferred_end_stream_ && deferred_data_ == nullptr &&
                    deferred_request_trailers_ == nullptr && deferred_metadata_.empty();
```

**Incoming (patch)**:
```cpp
  state_.deferred_to_next_io_iteration_ = false;
#if defined(ALIMESH)
  // Decrement deferred gauge as this stream is now being processed
  connection_manager_.stats_.named_.downstream_rq_deferred_.dec();
#endif
  bool end_stream =
      state_.deferred_end_stream_ && deferred_data_ == nullptr && request_trailers_ == nullptr;
```

**Conflict Type**: Code difference - different variable name

**Analysis**:
- envoy-1.36 uses `deferred_request_trailers_`
- envoy-1.27 (patch) uses `request_trailers_`
- The patch adds gauge decrement logic
- This indicates a variable rename between versions

**Resolution**:
✅ Adapt to envoy-1.36:
1. Keep the gauge decrement from the patch (with `ALIMESH` → `HIGRESS`)
2. Use envoy-1.36's `deferred_request_trailers_` instead of `request_trailers_`
3. Keep envoy-1.36's `deferred_metadata_.empty()` check

**Action**: Manual merge with variable name adaptation

---

### Conflict 4: Header file static member (conn_manager_impl.h:151-159)

**Location**: After `MaxRequestsPerIoCycle` declaration

**HEAD (envoy-1.36)**:
```cpp
  static const absl::string_view MaxRequestsPerIoCycle;
  static const absl::string_view OptionallyDelayClose;
```

**Incoming (patch)**:
```cpp
  static const absl::string_view MaxRequestsPerIoCycle;
#if defined(ALIMESH)
  // Runtime key for global maximum number of requests that can be processed from all connections
  // per I/O cycle on this thread. Requests over this limit are deferred until the next I/O cycle.
  static const absl::string_view MaxTotalRequestsPerIoCycle;
#endif
```

**Conflict Type**: Insertion conflict

**Analysis**:
- envoy-1.36 added `OptionallyDelayClose` declaration
- Patch adds `MaxTotalRequestsPerIoCycle` declaration
- Both are independent additions

**Resolution**:
✅ Keep both:
1. Keep `OptionallyDelayClose` from envoy-1.36
2. Add `MaxTotalRequestsPerIoCycle` from the patch (with `ALIMESH` → `HIGRESS`)

**Action**: Manual merge required

---

### Conflict 5: Thread-local members (conn_manager_impl.h:715-739)

**Location**: End of private member variables section

**HEAD (envoy-1.36)**:
```cpp
  uint32_t requests_during_dispatch_count_{0};
  const uint32_t max_requests_during_dispatch_{UINT32_MAX};
  Event::SchedulableCallbackPtr deferred_request_processing_callback_;
  const envoy::config::core::v3::TrafficDirection direction_;

  // If independent half-close is enabled...
  // [Long comment about half-close]

  const bool allow_upstream_half_close_{};

  // Whether the connection manager is drained due to premature resets.
  bool drained_due_to_premature_resets_{false};
```

**Incoming (patch)**:
```cpp
  uint32_t requests_during_dispatch_count_{0};
  const uint32_t max_requests_during_dispatch_{UINT32_MAX};
  Event::SchedulableCallbackPtr deferred_request_processing_callback_;

#if defined(ALIMESH)
  // Thread-local global request limiting variables.
  // These are shared across all ConnectionManagerImpl instances on this thread.
  static thread_local uint64_t global_requests_during_current_event_loop_;
  static thread_local uint64_t global_max_requests_per_io_cycle_;
  static thread_local bool global_reset_watchers_registered_; // Tracks if watchers were registered
#endif
  const bool refresh_rtt_after_request_{};
```

**Conflict Type**: Structural difference

**Analysis**:
- envoy-1.36 has `direction_`, `allow_upstream_half_close_`, and `drained_due_to_premature_resets_` members
- envoy-1.27 (patch) has `refresh_rtt_after_request_` and thread_local static members
- ⚠️ **Key**: `refresh_rtt_after_request_` is an envoy-1.27-specific feature, not needed in envoy-1.36
- We only need to add the thread_local static members for global request limiting

**Resolution**:
✅ Add only the global request limiting members:
1. Keep envoy-1.36's `direction_` member
2. Add the patch's thread_local static members (with `ALIMESH` → `HIGRESS`)
3. Do NOT add `refresh_rtt_after_request_` member (envoy-1.27 specific)
4. Keep envoy-1.36's `allow_upstream_half_close_` and `drained_due_to_premature_resets_` members

**Member Order**:
```cpp
const envoy::config::core::v3::TrafficDirection direction_;

#if defined(HIGRESS)
  // Thread-local global request limiting variables.
  // These are shared across all ConnectionManagerImpl instances on this thread.
  static thread_local uint64_t global_requests_during_current_event_loop_;
  static thread_local uint64_t global_max_requests_per_io_cycle_;
  static thread_local bool global_reset_watchers_registered_;
#endif

const bool allow_upstream_half_close_{};

// Whether the connection manager is drained due to premature resets.
bool drained_due_to_premature_resets_{false};
```

**Action**: Add only thread_local members

---

## Additional Changes from Patch (No Conflicts)

The following files were modified by the patch but have no conflicts:

1. **source/common/http/conn_manager_config.h** - These changes should apply cleanly
2. **source/extensions/common/wasm/foreign.cc** - These changes should apply cleanly

Note: Need to verify that `ALIMESH` macros in these files are also changed to `HIGRESS`.

---

## Resolution Strategy Summary

| File | Conflict | Strategy | Macro Change |
|------|----------|----------|--------------|
| conn_manager_impl.cc:72-88 | Runtime keys | Keep both additions | ALIMESH → HIGRESS |
| conn_manager_impl.cc:155-184 | Constructor | Keep 1.36 structure + add global limit init | ALIMESH → HIGRESS |
| conn_manager_impl.cc:2569-2579 | Method body | Keep gauge + use 1.36 vars | ALIMESH → HIGRESS |
| conn_manager_impl.h:151-159 | Static member | Keep both | ALIMESH → HIGRESS |
| conn_manager_impl.h:715-739 | Member vars | Add only thread_local members | ALIMESH → HIGRESS |

---

## Risk Assessment

### Medium-High Risks
1. **Constructor initialization order**: Must ensure correct initialization order for new members
2. **Variable name changes**: `request_trailers_` vs `deferred_request_trailers_` must be handled correctly
3. **Architectural differences**: envoy-1.36 has additional members that envoy-1.27 doesn't have

### Verification Required
1. ✅ Check that all `ALIMESH` occurrences are changed to `HIGRESS`
2. ✅ Verify member variable order matches class design
3. ✅ Ensure gauge metrics are properly incremented/decremented
4. ✅ Validate that non-conflicted files also apply cleanly

---

## Post-Resolution Verification Checklist

- [ ] All conflicts resolved
- [ ] All `ALIMESH` macros changed to `HIGRESS`
- [ ] Code compiles successfully
- [ ] Member variable initialization order is correct
- [ ] Gauge metrics logic is complete (increment/decrement pairs)
- [ ] Non-conflicted files applied correctly
- [ ] `refresh_rtt_after_request_` logic NOT migrated (envoy-1.27 specific feature)

---

## Document Updates

| Date | Update | Reason |
|------|--------|--------|
| 2026-02-03 | Clarified Conflict 2 & 5 analysis | User identified that `refresh_rtt_after_request_` is envoy-1.27 specific and should NOT be migrated to envoy-1.36. Only extract the global limit initialization code (lines 116-123) and thread_local members. |

---

**请审核以上方案，确认无误后回复"确认执行"开始解决冲突。**
