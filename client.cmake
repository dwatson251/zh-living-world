# Graphical client: the retail engine (WW3D2 + W3DDevice + Win32 devices) with the Living World network layer. Build with -DUM_CLIENT_BUILD=ON.
include(${CMAKE_SOURCE_DIR}/dsp_sources.cmake)
set(GE_ALL ${GE_DSP})
list(FILTER GE_ALL EXCLUDE REGEX "GameNetwork/GameSpy|GameNetwork/GameSpy[A-Za-z]*\\.cpp|Audio/simpleplayer|Audio/urllaunch|Audio/GameSpeech|Displayers|GameNetwork/NAT\\.cpp|Menus/WOL[A-Za-z]*\\.cpp")
add_library(ge OBJECT ${GE_ALL})

set(WW_ALL ${WW_DSP})
add_library(ww OBJECT ${WW_ALL})

set(WW3D_ALL ${WW3D_DSP})
list(FILTER WW3D_ALL EXCLUDE REGEX "dx8webbrowser|animatedsoundmgr|sr_util|ww3d2/FramGrab")
add_library(ww3d OBJECT ${WW3D_ALL})

file(GLOB ZLIB_C ${CMAKE_SOURCE_DIR}/deps/zlib-1.3.1/*.c)
list(FILTER ZLIB_C EXCLUDE REGEX "gz|infback|minigzip|example")
add_library(zlibobj OBJECT ${ZLIB_C})
set_source_files_properties(${ZLIB_C} PROPERTIES COMPILE_FLAGS "-DZ_PREFIX -DHAVE_UNISTD_H=0")

set(CMP ${CODE}/Libraries/Source/Compression)
file(GLOB EAC_CPP ${CMP}/EAC/*.cpp)
add_library(comp OBJECT ${CMP}/CompressionManager.cpp ${EAC_CPP})

set(DEV_ALL ${DEV_DSP})
list(FILTER DEV_ALL EXCLUDE REGEX "MilesAudioDevice|Bink|W3DWebBrowser")
add_library(dev OBJECT ${DEV_ALL})

file(GLOB UM_CPP ${CMAKE_SOURCE_DIR}/src/null/NullAudioManager.cpp ${CMAKE_SOURCE_DIR}/src/null/NullNetworkBase.cpp ${CMAKE_SOURCE_DIR}/src/null/NullGameResultsInterface.cpp ${CMAKE_SOURCE_DIR}/src/null/NullVideoPlayerInterface.cpp ${CMAKE_SOURCE_DIR}/src/net/*.cpp ${CMAKE_SOURCE_DIR}/src/client/*.cpp ${CMAKE_SOURCE_DIR}/src/client/d3dnull/*.cpp ${CMAKE_SOURCE_DIR}/src/um_refpack.cpp ${CMAKE_SOURCE_DIR}/src/d3dx8_impl.cpp ${CMAKE_SOURCE_DIR}/src/um_nox_stub.cpp ${CMAKE_SOURCE_DIR}/src/stubs_gen.cpp ${CMAKE_SOURCE_DIR}/src/stubs_manual.cpp)
include_directories(${CMAKE_SOURCE_DIR}/src ${V}/Main ${V}/Libraries/Source/Benchmark)
add_library(um OBJECT ${UM_CPP})

add_executable(zh_client $<TARGET_OBJECTS:ge> $<TARGET_OBJECTS:ww> $<TARGET_OBJECTS:ww3d> $<TARGET_OBJECTS:dev> $<TARGET_OBJECTS:zlibobj> $<TARGET_OBJECTS:comp> $<TARGET_OBJECTS:um>)
target_link_libraries(zh_client ws2_32 winmm imagehlp ole32 oleaut32 uuid shlwapi advapi32 user32 gdi32 shell32 version wininet comdlg32 vfw32 psapi imm32 d3d8 ddraw dinput8 dxguid)
target_link_directories(zh_client PRIVATE ${CMAKE_SOURCE_DIR}/build/shim/lib)
target_link_options(zh_client PRIVATE -static -mwindows -Wl,--disable-dynamicbase -Wl,--image-base=0x400000 -Wl,--wrap=exit -Wl,--wrap=abort -Wl,--error-limit=0 -Wl,-Map=${CMAKE_BINARY_DIR}/zhc.map)

set_source_files_properties(${CODE}/Libraries/Source/WWVegas/WWDownload/Download.cpp ${CODE}/Libraries/Source/WWVegas/WWDownload/urlBuilder.cpp ${CODE}/Libraries/Source/WWVegas/WWDownload/FTP.CPP ${CODE}/Libraries/Source/WWVegas/WWDownload/registry.cpp
  PROPERTIES COMPILE_FLAGS "-I/vroot/Libraries/Source/WWVegas/WWDownload")

# Standalone launcher (plain Win32, no engine code).
add_executable(zh_launcher src/launcher/launcher.cpp)
set_target_properties(zh_launcher PROPERTIES COMPILE_FLAGS "")
target_link_libraries(zh_launcher user32 gdi32 comdlg32 comctl32 shell32 ole32 advapi32)
target_link_options(zh_launcher PRIVATE -static -mwindows)

# Server host: window or --cli supervisor for the dedicated server (zh_client.exe in server mode).
add_executable(zh_host src/host/host.cpp)
target_link_libraries(zh_host user32 gdi32 comctl32 shell32 ole32 advapi32 ws2_32)
target_link_options(zh_host PRIVATE -static -mwindows)
