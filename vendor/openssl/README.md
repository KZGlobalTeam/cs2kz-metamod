# OpenSSL prebuilt (Win x64, static, /MT-compat)

OpenSSL 3.6.3 built with vcpkg triplet `x64-windows-static`.
Used by ixwebsocket.lib and the plugin on Windows.
Linux links the system OpenSSL instead.

The libs are large because vcpkg embeds /Z7 debug info. Do NOT strip them.
llvm-objcopy --strip-debug corrupts the nasm-built EC objects and every TLS handshake then fails with "invalid curve".

To rebuild:

```sh
vcpkg install openssl:x64-windows-static
cp /path/to/vcpkg/installed/x64-windows-static/lib/libcrypto.lib vendor/openssl/lib/
cp /path/to/vcpkg/installed/x64-windows-static/lib/libssl.lib vendor/openssl/lib/
cp -r /path/to/vcpkg/installed/x64-windows-static/include/openssl vendor/openssl/include/openssl

# The headers are pruned to the ~69 that ixwebsocket actually compiles against.
# header-whitelist.txt lists them.  Extra headers are harmless if you skip this, just larger.
cd vendor/openssl/include/openssl && ls *.h | grep -vxF -f ../../header-whitelist.txt | xargs -r rm && cd -
```

Then rebuild ixwebsocket.lib (`configure.py --build-vendor-deps` + `ambuild`) and commit everything together.

If a version bump makes ixwebsocket need a header not in the whitelist, the build fails with a clear `Cannot open include file`.
Add that header's name to header-whitelist.txt and re-copy it.
