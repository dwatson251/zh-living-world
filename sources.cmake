include(${CMAKE_SOURCE_DIR}/dsp_sources.cmake)
set(GE_ALL ${GE_DSP})
# Dropped from the headless build: GameSpy/WOL online services, menus/GUI callbacks, browser/speech/WMF glue.
list(FILTER GE_ALL EXCLUDE REGEX "GameNetwork/GameSpy|GameNetwork/GameSpy[A-Za-z]*\\.cpp|Audio/simpleplayer|Audio/urllaunch|Audio/GameSpeech|Displayers|GameNetwork/NAT\\.cpp|Menus/WOL[A-Za-z]*\\.cpp")
add_library(ge OBJECT ${GE_ALL})

# WWVegas support libraries (WW3D2/WWAudio/WWDownload/Miles6/wwshade are graphics/audio/online: not built)
set(WW_ALL ${WW_DSP})
add_library(ww OBJECT ${WW_ALL})

# zlib 1.3.1 (the 1.1.4 sources are not in the EA drop); built with Z_PREFIX like the original.
file(GLOB ZLIB_C ${CMAKE_SOURCE_DIR}/deps/zlib-1.3.1/*.c)
list(FILTER ZLIB_C EXCLUDE REGEX "gz|infback|minigzip|example")
add_library(zlibobj OBJECT ${ZLIB_C})
set_source_files_properties(${ZLIB_C} PROPERTIES COMPILE_FLAGS "-DZ_PREFIX -DHAVE_UNISTD_H=0")

# Compression: CompressionManager + EA btree/huff decoders. RefPack and NOX are replaced in src/.
set(CMP ${CODE}/Libraries/Source/Compression)
file(GLOB EAC_CPP ${CMP}/EAC/*.cpp)
add_library(comp OBJECT ${CMP}/CompressionManager.cpp ${EAC_CPP})

file(GLOB_RECURSE UM_CPP ${CMAKE_SOURCE_DIR}/src/*.cpp)
include_directories(${CMAKE_SOURCE_DIR}/src)
list(FILTER UM_CPP EXCLUDE REGEX "headless_main|src/client/|src/launcher/|src/host/")
add_library(um OBJECT ${UM_CPP})

# Device-layer files the headless sim needs (map loading, terrain logic, ghost objects).
set(DEV ${CODE}/GameEngineDevice/Source)
add_library(dev OBJECT ${DEV}/W3DDevice/GameClient/WorldHeightMap.cpp ${DEV}/W3DDevice/GameClient/TileData.cpp
  ${DEV}/W3DDevice/GameLogic/W3DGameLogic.cpp ${DEV}/W3DDevice/GameLogic/W3DTerrainLogic.cpp ${DEV}/W3DDevice/GameLogic/W3DGhostObject.cpp
  ${DEV}/W3DDevice/Common/Thing/W3DThingFactory.cpp
  ${DEV}/Win32Device/Common/Win32LocalFileSystem.cpp ${DEV}/Win32Device/Common/Win32LocalFile.cpp ${DEV}/Win32Device/Common/Win32BIGFileSystem.cpp ${DEV}/Win32Device/Common/Win32BIGFile.cpp ${DEV}/Win32Device/Common/Win32CDManager.cpp)

add_executable(zh_headless src/headless_main.cpp $<TARGET_OBJECTS:ge> $<TARGET_OBJECTS:ww> $<TARGET_OBJECTS:dev> $<TARGET_OBJECTS:zlibobj> $<TARGET_OBJECTS:comp> $<TARGET_OBJECTS:um>)
target_link_libraries(zh_headless ws2_32 winmm imagehlp ole32 oleaut32 uuid shlwapi advapi32 user32 gdi32 shell32 version wininet comdlg32 vfw32 psapi imm32 ddraw dxguid)
target_link_directories(zh_headless PRIVATE ${CMAKE_SOURCE_DIR}/build/shim/lib)
target_link_options(zh_headless PRIVATE -static -Wl,--wrap=__cxa_throw -Wl,--wrap=exit -Wl,--wrap=abort -Wl,--error-limit=0 -Wl,-Map=${CMAKE_BINARY_DIR}/zh.map)

set_source_files_properties(${CODE}/Libraries/Source/WWVegas/WWDownload/Download.cpp ${CODE}/Libraries/Source/WWVegas/WWDownload/urlBuilder.cpp ${CODE}/Libraries/Source/WWVegas/WWDownload/FTP.CPP ${CODE}/Libraries/Source/WWVegas/WWDownload/registry.cpp
  PROPERTIES COMPILE_FLAGS "-I/vroot/Libraries/Source/WWVegas/WWDownload")
