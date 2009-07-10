/*
 * Copyright (C) 2002 2003 2004, Magnus Hjorth
 *
 * This file is part of mhWaveEdit.
 *
 * mhWaveEdit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by        
 * the Free Software Foundation; either version 2 of the License, or  
 * (at your option) any later version.
 *
 * mhWaveEdit is distributed in the hope that it will be useful,   
 * but WITHOUT ANY WARRANTY; without even the implied warranty of  
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mhWaveEdit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */


/* --------------------------------------------------------------------------
 * This is the base class for most of the dialogs inside the effects
 * browser. The class should be considered "abstract" and the functions in this
 * file should only be used from subclasses. 
 *
 * Each EffectDialog is associated with an effect browser  and when
 * the associated browser is closed, the dialog will automatically
 * close as well. 
 *
 * The EffectDialog has an input area where subclasses can place widgets for
 * setting effect parameters etc. 
 * -------------------------------------------------------------------------- */


#ifndef EFFECT_DIALOG_H_INCLUDED
#define EFFECT_DIALOG_H_INCLUDED

#include <gtk/gtk.h>
#include "mainwindow.h"

#define EFFECT_DIALOG(obj) GTK_CHECK_CAST(obj,effect_dialog_get_type(),EffectDialog)
#define EFFECT_DIALOG_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,effect_dialog_get_type(),EffectDialogClass)
#define IS_EFFECT_DIALOG(obj) GTK_CHECK_TYPE(obj,effect_dialog_get_type())

typedef struct _EffectDialog {

     GtkVBox vbox;

     /* Input area - subclasses can add their own widgets here. */
     GtkContainer *input_area;

     /* Associated effect browser. Read-only, set this once with the
      * effect_dialog_setup function. */
     gpointer eb;

     /* Effect name. Read-only, do not modify or free. */
     gchar *effect_name;

} EffectDialog;



typedef struct _EffectDialogClass {
     GtkVBoxClass vbox_class;

     /* This signal is emitted when the OK or Apply button is pressed. Default
      * action: do nothing. Return TRUE if something failed. */
     gboolean (*apply)(EffectDialog *ed);

     /* This signal is called when the associated EffectBrowser and
      * effect name has been set for the dialog. */
     void (*setup)(EffectDialog *ed);

     /* Called when eb->mwl->selected->chunk has changed */
     void (*target_changed)(EffectDialog *ed);

} EffectDialogClass;

GtkType effect_dialog_get_type(void);

/* This sets the associated Mainwindow for the dialog. This must be called once
 * and only once for a dialog.
 *
 * ed - An EffectDialog
 * eb - The EffectBrowser to associate with the EffectDialog.
 */
void effect_dialog_setup(EffectDialog *ed, gchar *effect_name, gpointer eb);



/* This "runs" the effect with the current settings. Returns TRUE on failure. 
 */
gboolean effect_dialog_apply(EffectDialog *ed);


#endif
