/*
 * Copyright (C) 2004 2009, Magnus Hjorth
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


/* This is a GtkObject wrapper around GList with notification signals 
 * and optional automatic refcounting for GtkObjects. 
 */

#ifndef LISTOBJECT_H_INCLUDED
#define LISTOBJECT_H_INCLUDED

#define LIST_OBJECT(obj) GTK_CHECK_CAST(obj,list_object_get_type(),ListObject)
#define LIST_OBJECT_CLASS(klass) GTK_CHECK_CLASS_CAST(klass,list_object_get_type(),ListObjectClass)
#define IS_LIST_OBJECT(obj) GTK_CHECK_TYPE(obj,list_object_get_type())

typedef struct {
     GtkObject obj;
     /* <private> */
     GList *list;
     gboolean do_ref;
} ListObject;

typedef struct {
     GtkObjectClass obj_class;
     /* Signal sent directly after one item has been removed
      * The signal is sent before dereferencing the object (if do_ref)  */
     void (*item_removed)(ListObject *,gpointer);
     /* Signal sent directly after one item has been added */
     void (*item_added)(ListObject *,gpointer);
     /* General "notification" signal */
     void (*item_notify)(ListObject *,gpointer);
} ListObjectClass;

GtkType list_object_get_type(void);


/* Creates a new ListObject 
 * do_ref: Enable automatic referencing/derefencing - should only be used
 *         if the list just contains GtkObjects  */
ListObject *list_object_new(gboolean do_ref);

/* Creates a new ListObject from an existing list. The list is "taken over"
 * by the new object so it should not be changed or freed afterwards. */
ListObject *list_object_new_from_list(GList *l, gboolean do_ref);

/* Adds ptr to the list. */
void list_object_add(ListObject *lo, gpointer ptr);

/* Removes ptr from the list. Returns TRUE if something was removed. */
gboolean list_object_remove(ListObject *lo, gpointer ptr);

/* Sends the notify signal */
void list_object_notify(ListObject *lo, gpointer ptr);

/* Returns the number of items in the list */
guint list_object_get_size(ListObject *lo);

/* Gets an item from the list */
gpointer list_object_get(ListObject *lo, guint index);

void list_object_foreach(ListObject *lo, GFunc func, gpointer user_data);

/* Clears the list. 
   do_signal: Whether the item-removed signal should be sent for each object. 
*/
void list_object_clear(ListObject *lo, gboolean do_signal);

#endif
