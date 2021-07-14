with import <nixpkgs> {};
let 
  kernel = linuxPackages_4_14.kernel;
in
gcc9Stdenv.mkDerivation {
  name = "env";
  hardeningDisable = [ "fortify" ];
  NIX_CFLAGS_COMPILE = "-march=native";
  NIXOS_KERNELDIR = "${kernel.dev}/lib/modules/${kernel.modDirVersion}/build";
  RTE_KERNELDIR = "${kernel.dev}/lib/modules/${kernel.modDirVersion}/build";
  buildInputs = [
    cmake
    fmt
    glog
    folly
    openssl
    boost175
    jemalloc
    double-conversion
    numactl
    libevent
    gflags
    docopt_cpp
  ];
}
