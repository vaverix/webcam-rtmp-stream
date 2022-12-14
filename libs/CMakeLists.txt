CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

INCLUDE(ExternalProject)
INCLUDE_DIRECTORIES(BEFORE ${CMAKE_BINARY_DIR}/WebcamRtmpStream)
LINK_DIRECTORIES(${CMAKE_BINARY_DIR}/lib)
SET(ENV{PATH} ${CMAKE_BINARY_DIR}/bin:$ENV{PATH})
SET(ENV{PKG_CONFIG_PATH} ${CMAKE_BINARY_DIR}/lib/pkgconfig)

EXTERNALPROJECT_ADD(
  yasm
  URL ${CMAKE_SOURCE_DIR}/vendor/yasm-1.3.0.tar.gz
  PATCH_COMMAND ${CMAKE_SOURCE_DIR}/patches/patch-manager.sh yasm
  CONFIGURE_COMMAND ./configure --prefix=${CMAKE_BINARY_DIR}
  BUILD_COMMAND make -j12
  BUILD_IN_SOURCE 1
)

EXTERNALPROJECT_ADD(
  nasm
  URL ${CMAKE_SOURCE_DIR}/vendor/nasm-2.15.05.tar.bz2
  PATCH_COMMAND ${CMAKE_SOURCE_DIR}/patches/patch-manager.sh nasm
  CONFIGURE_COMMAND ./configure --prefix=${CMAKE_BINARY_DIR}
  BUILD_COMMAND make -j12
  BUILD_IN_SOURCE 1
)

EXTERNALPROJECT_ADD(
  zlib
  URL ${CMAKE_SOURCE_DIR}/vendor/zlib-v1.2.12.tar.gz
  PATCH_COMMAND ${CMAKE_SOURCE_DIR}/patches/patch-manager.sh zlib
  BUILD_COMMAND make -j12
  CONFIGURE_COMMAND ./configure --prefix=${CMAKE_BINARY_DIR} --static
  BUILD_IN_SOURCE 1
)

if (APPLE)
  EXTERNALPROJECT_ADD(
    openssl
    URL ${CMAKE_SOURCE_DIR}/vendor/openssl-1.0.2o.tar.gz
    PATCH_COMMAND ${CMAKE_SOURCE_DIR}/patches/patch-manager.sh openssl
    CONFIGURE_COMMAND ./Configure darwin64-x86_64-cc no-shared --prefix=${CMAKE_BINARY_DIR}
    INSTALL_COMMAND make install_sw -j12
    BUILD_IN_SOURCE 1
  )
elseif (UNIX AND NOT APPLE)
  EXTERNALPROJECT_ADD(
    openssl
    URL ${CMAKE_SOURCE_DIR}/vendor/openssl-1.0.2o.tar.gz
    PATCH_COMMAND ${CMAKE_SOURCE_DIR}/patches/patch-manager.sh openssl
    CONFIGURE_COMMAND ./config --prefix=${CMAKE_BINARY_DIR}
    INSTALL_COMMAND make install_sw -j12
    BUILD_IN_SOURCE 1
  )
endif()

EXTERNALPROJECT_ADD(
  xvidcore
  DEPENDS yasm
  URL ${CMAKE_SOURCE_DIR}/vendor/xvidcore-1.3.7.tar.gz
  PATCH_COMMAND ${CMAKE_SOURCE_DIR}/patches/patch-manager.sh xvidcore
  CONFIGURE_COMMAND cd build/generic && ./configure --prefix=${CMAKE_BINARY_DIR}
  BUILD_COMMAND cd build/generic && make -j12
  INSTALL_COMMAND cd build/generic && make install
  BUILD_IN_SOURCE 1
)

EXTERNALPROJECT_ADD(
  x264
  DEPENDS nasm
  URL ${CMAKE_SOURCE_DIR}/vendor/x264-master.tar.bz2
  PATCH_COMMAND ${CMAKE_SOURCE_DIR}/patches/patch-manager.sh x264
  CONFIGURE_COMMAND ./configure --prefix=${CMAKE_BINARY_DIR} --enable-static --enable-pic --enable-lto --disable-lavf
  BUILD_COMMAND make -j12
  BUILD_IN_SOURCE 1
)

EXTERNALPROJECT_ADD(
  rtmpdump
  DEPENDS zlib openssl
  URL ${CMAKE_SOURCE_DIR}/vendor/rtmpdump-2.4.tar.gz
  PATCH_COMMAND ${CMAKE_SOURCE_DIR}/patches/patch-manager.sh rtmpdump
  CONFIGURE_COMMAND ""
  BUILD_COMMAND make -j12 PREFIX=${CMAKE_BINARY_DIR} SHARED= XCFLAGS=-I${CMAKE_BINARY_DIR}/include XLDFLAGS=-L${CMAKE_BINARY_DIR}/lib XLIBS=-ldl
  INSTALL_COMMAND make PREFIX=${CMAKE_BINARY_DIR} SHARED= install
  BUILD_IN_SOURCE 1
)

EXTERNALPROJECT_ADD(
  iconv
  URL ${CMAKE_SOURCE_DIR}/vendor/libiconv-1.17.tar.gz
  PATCH_COMMAND ${CMAKE_SOURCE_DIR}/patches/patch-manager.sh iconv
  CONFIGURE_COMMAND PKG_CONFIG_PATH=$ENV{PKG_CONFIG_PATH} ./configure --prefix=${CMAKE_BINARY_DIR} --disable-shared --enable-static
  BUILD_COMMAND make -j12
  BUILD_IN_SOURCE 1
)

EXTERNALPROJECT_ADD(
  libpng
  DEPENDS zlib
  URL ${CMAKE_SOURCE_DIR}/vendor/libpng-1.6.37.tar.gz
  PATCH_COMMAND ${CMAKE_SOURCE_DIR}/patches/patch-manager.sh libpng
  CONFIGURE_COMMAND CFLAGS=-I${CMAKE_BINARY_DIR}/include CPPFLAGS=-I${CMAKE_BINARY_DIR}/include LDFLAGS=-L${CMAKE_BINARY_DIR}/lib ./configure --prefix=${CMAKE_BINARY_DIR} --disable-shared --enable-static
  BUILD_COMMAND make -j12
  BUILD_IN_SOURCE 1
)

EXTERNALPROJECT_ADD(
  ffmpeg
  DEPENDS yasm xvidcore x264 rtmpdump openssl iconv
  URL ${CMAKE_SOURCE_DIR}/vendor/ffmpeg-4.4.2.tar.bz2
  CONFIGURE_COMMAND PKG_CONFIG_PATH=$ENV{PKG_CONFIG_PATH} ./configure --prefix=${CMAKE_BINARY_DIR} --datadir=${CMAKE_BINARY_DIR}/etc --disable-shared --enable-static --enable-pic --enable-gpl --enable-nonfree --pkg-config-flags=--static --disable-doc --disable-debug --disable-ffplay --disable-outdevs --enable-runtime-cpudetect --extra-cflags=-I${CMAKE_BINARY_DIR}/include\ -static --extra-ldflags=-L${CMAKE_BINARY_DIR}/lib --extra-libs=-lstdc++\ -lexpat\ -pthread\ -lm\ -ldl --enable-libx264 --enable-libxvid --enable-librtmp --enable-openssl
  BUILD_COMMAND make -j12
  BUILD_IN_SOURCE 1
)
