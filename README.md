# cppVpp

C++17 client library for the VPP binary API (unix socket / `socksvr`), modelled on GoVPP.

- `vpp::Connection` – handshake (`sockclnt_create`), request/reply and dump calls.
- `tools/gen_api` (Go) – generates C++ structs/enums with `Encode`/`Decode` from VPP's
  `*.api.json` (default `/usr/share/vpp/api`) at build time; see `vpp/rpc.h` for
  typed `Invoke` / `InvokeDump`.

## Examples (`examples/`, built into `build/`)

- `show_version` – connect, `control_ping`, `show_version`.
- `list_interfaces` – `sw_interface_dump` plus `ip_address_dump` per interface.
- `bridge_demo` – creates a bridge domain and two loopbacks (one BVI with an
  IPv4 address), brings them up, dumps bridge/addresses, then cleans up.

Each takes an optional API socket path (default `/run/vpp/api.sock`).

## Build and test (in the devcontainer image)

    docker build -t cppvpp-dev .devcontainer
    docker run -d --name vpp -v $PWD:/src -v vppsock:/run/vpp cppvpp-dev vpp -c /src/docker/startup.conf
    docker run --rm -v $PWD:/src -v vppsock:/run/vpp -e VPP_API_SOCKET=/run/vpp/api.sock cppvpp-dev \
      bash -c 'cd /src && cmake -S . -B build -G Ninja && cmake --build build && cd build && ctest --output-on-failure && ./show_version'

Integration tests are skipped unless `VPP_API_SOCKET` is set.
