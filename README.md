# cppVpp

C++17 client library for the VPP binary API (unix socket / `socksvr`), modelled on GoVPP.

- `vpp::Connection` – handshake (`sockclnt_create`), request/reply and dump calls.
- `tools/gen_api` (Go) – generates C++ structs/enums with `Encode`/`Decode` from VPP's
  `*.api.json` (default `/usr/share/vpp/api`) at build time; see `vpp/rpc.h` for
  typed `Invoke` / `InvokeDump`.

## Build and test (in the devcontainer image)

    docker build -t cppvpp-dev .devcontainer
    docker run -d --name vpp -v $PWD:/src -v vppsock:/run/vpp cppvpp-dev vpp -c /src/docker/startup.conf
    docker run --rm -v $PWD:/src -v vppsock:/run/vpp -e VPP_API_SOCKET=/run/vpp/api.sock cppvpp-dev \
      bash -c 'cd /src && cmake -S . -B build -G Ninja && cmake --build build && cd build && ctest --output-on-failure && ./show_version'

Integration tests are skipped unless `VPP_API_SOCKET` is set.
