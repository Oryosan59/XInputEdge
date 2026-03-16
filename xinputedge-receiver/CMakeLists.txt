cmake_minimum_required(VERSION 3.16)
project(xinputedge_receiver C)

set(CMAKE_C_STANDARD 99)

# ---------------------------------------------------------------
# Static library
# ---------------------------------------------------------------
add_library(xinputedge STATIC
    src/server.c
    src/packet.c
    src/ring_buffer.c
    src/udp.c
    src/time_util.c
)

target_include_directories(xinputedge
    PUBLIC
        # ライブラリ利用者が必要なヘッダ（API 境界）
        # 利用側: #include <xinputedge/xinputedge.h> または #include <xie_server.h>
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol
    PRIVATE
        # ライブラリ内部だけで使う実装ヘッダ
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# コンパイル警告ポリシー: target 単位で管理することで
# CI・ローカルビルド・他プロジェクトからのリンク全てに一貫して適用される
target_compile_options(xinputedge PRIVATE
    -Wall
    -Wextra
    -Werror
    -pedantic
)

# ---------------------------------------------------------------
# Install ターゲット
# `cmake --install build/` または `sudo make install` で以下に配置される
#   /usr/local/lib/libxinputedge.a
#   /usr/local/include/xinputedge/xinputedge.h (統一エントリ)
#   /usr/local/include/xie_server.h            (後方互換)
#   /usr/local/include/protocol/               (XIE Protocol)
# ---------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS xinputedge
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 公開ヘッダ群（include/ ディレクトリ以下すべて）
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# プロトコル定義ヘッダ
install(DIRECTORY protocol/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/protocol
)

# ---------------------------------------------------------------
# Examples（オプション）
# ---------------------------------------------------------------
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples/basic_receiver)
endif()

# ---------------------------------------------------------------
# Unit Tests（オプション）
# ---------------------------------------------------------------
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
