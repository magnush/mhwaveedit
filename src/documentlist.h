/*
 * Copyright (C) 2002 2003 2004 2005, Magnus Hjorth
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


/* Document selection widget for the effect browser */

#ifndef DOCUMENTLIST_H_INCLUDED
#define DOCUMENTLIST_H_INCLUDED

#include "combo.h"
#include "document.h"
#include "listobject.h"

#define DOCUMENT_LIST(obj) GTK_CHECK_CAST(obj,document_list_get_type(),DocumentList)
#define DOCUMENT_LIST_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,document_list_get_type(),DocumentListClass)
#define IS_DOCUMENT_LIST(obj) GTK_CHECK_TYPE(obj,document_list_get_type())
typedef struct {
     Combo combo;
     Document *selected;
     /* Format of the selected document's chunk. The reason to keep track of 
      * this separately is that it makes it easier to handle the special case
      * where there are no windows (selected==NULL) */
     Dataformat format; 
} DocumentList;

typedef struct {
     ComboClass comboclass;
     /* The current mainwindow has changed, either by the user or the 
      * selected mainwindow was closed, or the chunk in the current 
      * mainwindow has changed. . */
     void (*document_changed)(DocumentList *dl);     
} DocumentListClass;

GtkType document_list_get_type(void);
GtkWidget *document_list_new(Document *chosen);
void document_list_setup(DocumentList *mwl, Document *chosen);

#endif
