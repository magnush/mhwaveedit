bin_PROGRAMS = mhwaveedit

mhwaveedit_SOURCES = chunk.c int_box.c sound.c speeddialog.c              \
                     chunkview.c main.c um.c                              \
                     effectdialog.c mainwindow.c ringbuf.c                \
                     volumedialog.c float_box.c player.c                  \
                     sampleratedialog.c gtkfiles.c samplesizedialog.c     \
                     formatselector.c recorddialog.c vu_meter.c inifile.c \
                     configdialog.c combinechannelsdialog.c rawdialog.c   \
		     gotodialog.c pipedialog.c historybox.c viewcache.c   \
		     filetypes.c datasource.c statusbar.c effectbrowser.c \
		     soxdialog.c help.c ladspacore.c ladspadialog.c       \
		     tempfile.c listobject.c documentlist.c combo.c       \
		     rateconv.c dataformat.c rateest.c document.c         \
		     session.c mapchannelsdialog.c mainloop.c             \
		     recordformatcombo.c sandwichdialog.c bitmap.c

INCLUDES = @GTK_CFLAGS@ @SDL_CFLAGS@ @SNDFILE_CFLAGS@ @JACK_CFLAGS@ \
	   @ESOUND_CFLAGS@ @LIBSAMPLERATE_CFLAGS@ @ARTSC_CFLAGS@    \
           @GTHREAD_CFLAGS@ @PULSE_CFLAGS@
LDADD = @GTK_LIBS@ @SDL_LIBS@ @PORTAUDIO_LIBS@ @SNDFILE_LIBS@ @ALSALIB_LIBS@ \
        @JACK_LIBS@ @ESOUND_LIBS@ @LIBSAMPLERATE_LIBS@ @ARTSC_LIBS@ \
        @GTHREAD_LIBS@ @PULSE_LIBS@

localedir = $(datadir)/locale
AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\"

CLEANFILES = *~ core core.* #*#
DISTCLEANFILES = .deps/*.P
