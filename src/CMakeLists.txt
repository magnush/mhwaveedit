# Generating the library mhwaveedit
set(project "mhwaveedit")

set(${project}, "")
list(APPEND ${project}_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/bitmap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/chunk.c
    ${CMAKE_CURRENT_SOURCE_DIR}/chunkview.c
    ${CMAKE_CURRENT_SOURCE_DIR}/combinechannelsdialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/combo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/configdialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dataformat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/datasource.c
    ${CMAKE_CURRENT_SOURCE_DIR}/document.c
    ${CMAKE_CURRENT_SOURCE_DIR}/documentlist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/effectbrowser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/effectdialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/filetypes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/float_box.c
    ${CMAKE_CURRENT_SOURCE_DIR}/formatselector.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gotodialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gtkfiles.c
    ${CMAKE_CURRENT_SOURCE_DIR}/help.c
    ${CMAKE_CURRENT_SOURCE_DIR}/historybox.c
    ${CMAKE_CURRENT_SOURCE_DIR}/inifile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/int_box.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ladspacore.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ladspadialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/listobject.c
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mainloop.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mapchannelsdialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pipedialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/player.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rateconv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rateest.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rawdialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/recorddialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/recordformatcombo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ringbuf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sampleratedialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/samplesizedialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sandwichdialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/session.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sound.c
    ${CMAKE_CURRENT_SOURCE_DIR}/soxdialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/speeddialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/statusbar.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tempfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/um.c
    ${CMAKE_CURRENT_SOURCE_DIR}/viewcache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/volumedialog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vu_meter.c
)
add_executable(mhwaveedit ${${project}_SOURCES} )
