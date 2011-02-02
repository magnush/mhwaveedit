/*
 * Copyright (C) 2002 2003 2005, Magnus Hjorth
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


#ifndef INTBOX_H_INCLUDED
#define INTBOX_H_INCLUDED

#define INTBOX(obj) GTK_CHECK_CAST (obj, intbox_get_type(), Intbox)
#define INTBOX_CLASS(class) GTK_CHECK_CLASS_CAST(class,intbox_get_type(),IntboxClass)
#define IS_INTBOX(obj) GTK_CHECK_TYPE (obj,intbox_get_type())

typedef struct _Intbox Intbox;
typedef struct _IntboxClass IntboxClass;

struct _Intbox {
     GtkEntry parent;
     long int val;
     GtkAdjustment *adj;
};

struct _IntboxClass {
	GtkEntryClass parent;
	void (*numchange)(Intbox *, long int);
	};

GtkType intbox_get_type(void);
GtkWidget *intbox_new(long value);
void intbox_set(Intbox *box, long value);

/* TRUE = Contains invalid value */
gboolean intbox_check(Intbox *box);
gboolean intbox_check_limit(Intbox *box, long int lowest, long int highest,
			    gchar *valuename);
GtkWidget *intbox_create_scale(Intbox *box, long minval, long maxval);


#endif



