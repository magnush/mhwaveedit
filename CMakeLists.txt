cmake_minimum_required(VERSION 3.22)

set(MWE_TARGET        "mhWaveEdit")
set(MWE_VERSION       "1.4.24")
set(MWE_MAC_COPYRIGHT "© 2002-2018 Magnus Hjorth.")
set(MWE_MAC_BUNDLE_ID "mh.wave.edit")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")

project(${MWE_TARGET}
	VERSION ${MWE_VERSION}
  DESCRIPTION "Sound file editor (written in C using the GTK+ toolkit)"
  HOMEPAGE_URL "https://github.com/magnush/mhwaveedit"
	LANGUAGES C
)

# GLOBAL OPTIONS
find_package(ALSA REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(SDL REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GTHREAD REQUIRED gthread-2.0)
pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
pkg_check_modules(JACK REQUIRED jack)
pkg_check_modules(PULSE REQUIRED libpulse)
pkg_check_modules(SAMPLERATE REQUIRED samplerate)
pkg_check_modules(SNDFILE REQUIRED sndfile)

# Setting the include directory for the application to find config.h
include_directories(${CMAKE_BINARY_DIR})
include_directories(${GTK2_INCLUDE_DIRS})
include_directories(${SDL_INCLUDE_DIRS})

link_directories(${GTK2_LIBRARY_DIRS})

# Since we have created a config.h add a global define for it
add_definitions(${GTK2_CFLAGS_OTHER})
add_definitions("-DHAVE_CONFIG_H" )
add_definitions("-DLOCALEDIR=\"${CMAKE_CURRENT_SOURCE_DIR}/locale\"")

add_subdirectory( src )

set(MWE_LIBRARIES
	m
  ${ALSA_LIBRARIES}
  ${GLIB_LIBRARIES}
  ${GTHREAD_LIBRARIES}
  ${GTK2_LIBRARIES}
  ${JACK_LIBRARIES}
  ${PULSE_LIBRARIES}
  ${SAMPLERATE_LIBRARIES}
  ${SDL_LIBRARIES}
  ${SNDFILE_LIBRARIES}
  )

target_link_libraries(mhwaveedit ${MWE_LIBRARIES})
