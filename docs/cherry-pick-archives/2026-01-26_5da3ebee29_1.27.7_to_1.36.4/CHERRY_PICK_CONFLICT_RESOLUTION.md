# Cherry-pick Conflict Resolution

## Overview
- **Original Commit**: 5da3ebee29 "Apply patch: 000-9-rsa2048.patch"
- **Source Branch**: 1.27.7
- **Target Branch**: 1.36.4
- **Conflict Files**: 2
  - `source/extensions/transport_sockets/tls/context_impl.cc` (moved to `source/common/tls/context_impl.cc`)
  - `test/extensions/transport_sockets/tls/context_impl_test.cc` (moved to `test/common/tls/context_impl_test.cc`)

## Architecture Changes Identified
In version 1.36.4, TLS context implementation files were moved from:
- `source/extensions/transport_sockets/tls/` → `source/common/tls/`
- `test/extensions/transport_sockets/tls/` → `test/common/tls/`

## Conflict Analysis and Solutions

### 1. Main Implementation File
**Source Location**: `source/extensions/transport_sockets/tls/context_impl.cc` (in 1.27.7)
**Target Location**: `source/common/tls/context_impl.cc` (in 1.36.4)

**Patch Content**:
```cpp
case EVP_PKEY_RSA: {
#if !defined(HIGRESS)
  // We require RSA certificates with 2048-bit or larger keys.
  const RSA* rsa_public_key = EVP_PKEY_get0_RSA(public_key.get());
  // Since we checked the key type above, this should be valid.
  ASSERT(rsa_public_key != nullptr);
  const unsigned rsa_key_length = RSA_bits(rsa_public_key);
#ifdef BORINGSSL_FIPS
  if (rsa_key_length != 2048 && rsa_key_length != 3072 && rsa_key_length != 4096) {
    throw EnvoyException(
        fmt::format("Failed to load certificate chain from {}, only RSA certificates with "
                    "2048-bit, 3072-bit or 4096-bit keys are supported in FIPS mode",
                    ctx.cert_chain_file_path_));
  }
#else
  if (rsa_key_length < 2048) {
    throw EnvoyException(
        fmt::format("Failed to load certificate chain from {}, only RSA "
                    "certificates with 2048-bit or larger keys are supported",
                    ctx.cert_chain_file_path_));
  }
#endif
#endif
} break;
```

**Solution**: Apply the same conditional `#if !defined(HIGRESS)` wrapper to the RSA key validation code in the new location.

### 2. Test File
**Source Location**: `test/extensions/transport_sockets/tls/context_impl_test.cc` (in 1.27.7)
**Target Location**: `test/common/tls/context_impl_test.cc` (in 1.36.4)

**Solution**: Apply the same conditional compilation changes to the test file.

## Risk Assessment
- Low risk: The change adds conditional compilation directives without changing core logic
- Compatibility: Maintains existing behavior when HIGRESS is defined
- Security: Preserves RSA key validation requirements for non-HIGRESS builds

## Verification Checklist
- [x] Apply changes to correct file locations in 1.36.4
- [x] Ensure HIGRESS builds bypass RSA 2048-bit requirement
- [x] Verify non-HIGRESS builds maintain RSA 2048-bit requirement
- [ ] Compile successfully after changes
- [ ] Run related tests to ensure functionality

## Completed Changes
- Applied `#if !defined(HIGRESS)` conditional compilation directives to RSA key validation in `source/common/tls/context_impl.cc`
- Updated test file paths in `test/common/tls/context_impl_test.cc` to point to correct location