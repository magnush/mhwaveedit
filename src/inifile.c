/*
 * Copyright (C) 2002 2003 2004 2005 2008, Magnus Hjorth
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


#include <config.h>

#include <string.h>
#include <sys/stat.h>
#include "main.h"
#include "gtkfiles.h"
#include "inifile.h"
#include "gettext.h"

static GHashTable *settings;
static gchar *ininame;
static gboolean settings_changed = FALSE;

struct inifile_setting {
     gchar *name;
     gchar *value;
};

static gboolean xisspace(gchar c)
{
     return (c==' ' || c=='\t');
}

static gchar *skipspace(gchar *c)
{
     while (xisspace(*c)) c++;
     return c;
}

void inifile_init(void)
{
     EFILE *f;
     gchar *c,*d,*namestart,*valuestart;
     size_t s = 0;
     int x;
     settings = g_hash_table_new(g_str_hash,g_str_equal);
     c = g_strjoin(NULL,get_home_directory(),"/.mhwaveedit",NULL);
     mkdir(c,CONFDIR_PERMISSION);
     g_free(c);
     ininame = g_strjoin(NULL,get_home_directory(),"/.mhwaveedit/config",NULL);
     if (!file_exists(ininame)) return;
     f = e_fopen(ininame,EFILE_READ);
     if (!f) return;
     c = NULL;
     while (1) {
	  x = e_readline(&c,&s,f);
	  if (x <= 0) break;

	  /* Ta bort kommentar och avslutande space*/
	  d = strchr(c,'#');
	  if (!d) d=strchr(c,0);
	  while (d>c && xisspace(*(d-1))) d--;
	  *d = 0;

	  /* Skippa inledande space */
	  d = skipspace(c);

	  if (*d == 0) continue;

	  namestart = d;
	  while (*d!=0 && *d!=' ' && *d!='\t' && *d!='=') d++;
	  if (*d == 0) {
	       g_printerr(_("%s: Expected '=': %s\n"),ininame,c);
	       break;
	  }
	  *d = 0;
	  d++;
	  d = skipspace(d);
	  if (*d!='=') {
	       g_printerr(_("%s: Expected '=': %s\n"),ininame,c);
	       break;
	  }
	  d++;
	  d = skipspace(d);
	  if (*d==0) {
	       g_printerr(_("%s: Expected value: %s\n"),ininame,c);
	       break;
	  }
	  valuestart = d;
	  g_hash_table_insert(settings,g_strdup(namestart),
			      g_strdup(valuestart));
     }
     e_fclose(f);
     g_free(c);
}

gchar *inifile_get(gchar *setting, gchar *defaultValue)
{
     gchar *c;
     c = g_hash_table_lookup(settings, setting);
     if (c == NULL) return defaultValue;
     return c;
}

guint32 inifile_get_guint32(gchar *setting, guint32 defaultValue)
{
     gchar *c,*d;
     guint32 ui;
     c = inifile_get(setting, NULL);
     if (c) {
	  ui = strtoul(c,&d,10);
	  if (*d==0) return ui;
	  inifile_set_guint32(setting,defaultValue);
     }
     return defaultValue;
}

gint32 inifile_get_gint32(gchar *setting, gint32 defaultValue)
{
     gchar *c,*d;
     gint32 ui;
     c = inifile_get(setting, NULL);
     if (c) {
	  ui = strtol(c,&d,10);
	  if (*d==0) return ui;
	  inifile_set_gint32(setting,defaultValue);
     }
     return defaultValue;
}

gfloat inifile_get_gfloat(gchar *setting, gfloat defaultValue)
{
     gchar *c,*d;
     double x;
     c = inifile_get(setting,NULL);
     if (c) {
	  x = strtod(c,&d);
	  if (*d==0) return (gfloat)x;
	  inifile_set_gfloat(setting,defaultValue);
     }
     return defaultValue;
}

gboolean inifile_get_gboolean(gchar *setting, gboolean defaultValue)
{
     gchar *c;
     c = inifile_get(setting, NULL);
     // printf("inifile_get_gboolean: c == %s\n",c);
     if (c) {
	  if ( !g_strcasecmp(c,"y") || !strcmp(c,"1") ||
	       !g_strcasecmp(c,"yes") || !g_strcasecmp(c,"on") || 
	       !g_strcasecmp(c,"true") || !g_strcasecmp(c,"enabled")) 
	       return TRUE;
	  if (!g_strcasecmp(c,"n") || !strcmp(c,"0") ||
	      !g_strcasecmp(c,"no") || !g_strcasecmp(c,"off") ||
	      !g_strcasecmp(c,"false") || !g_strcasecmp(c,"disabled"))
	       return FALSE;
	  inifile_set_gboolean(setting,defaultValue);
     }
     return defaultValue;
}

gboolean inifile_set(gchar *setting, gchar *value)
{
     gboolean b;
     gpointer c,d;

     b = g_hash_table_lookup_extended(settings,setting,&c,&d);
     if (!b && value == NULL) return FALSE;
     if (b && value != NULL && !strcmp(d,value)) return FALSE;     

     /* printf("key: '%s', from: '%s', to: '%s'\n",setting,b?d:"(none)",value);
      */

     if (b) {
	  g_hash_table_remove(settings,c);
	  g_free(c);
	  g_free(d);
     }

     if (value != NULL)
	  g_hash_table_insert(settings,g_strdup(setting),g_strdup(value));
     settings_changed = TRUE;
     return TRUE;
}

gboolean inifile_set_guint32(gchar *setting, guint32 value)
{
     gchar c[32];
     g_snprintf(c,sizeof(c),"%lu",(unsigned long int)value);
     return inifile_set(setting,c);
}

gboolean inifile_set_gint32(gchar *setting, gint32 value)
{
     gchar c[32];
     g_snprintf(c,sizeof(c),"%ld",(signed long int)value);
     return inifile_set(setting,c);
}

gboolean inifile_set_gboolean(gchar *setting, gboolean value)
{
     return inifile_set(setting,value?"true":"false");
}

gboolean inifile_set_gfloat(gchar *setting, gfloat value)
{
     gchar c[128];
     g_snprintf(c,sizeof(c),"%f",(float)value);
     return inifile_set(setting,c);
}

static void do_save_setting(gchar *setting, gchar *value, EFILE *f)
{
     e_fwrite(setting,strlen(setting),f);
     e_fwrite(" = ",3,f);
     e_fwrite(value,strlen(value),f);
     e_fwrite("\n",1,f);
}

void inifile_save(void)
{
     EFILE *f = e_fopen(ininame,EFILE_WRITE);
     char *c = 
	  "# mhWaveEdit configuration file.\n"
	  "# Automatically generated by " PROGRAM_VERSION_STRING "\n"
	  "# May be hand edited but extra comments will be removed when the \n"
	  "# settings are saved.\n"
	  "# Remove this file to restore default settings.\n"
	  "\n";
     if (f == NULL) return;
     if (e_fwrite(c,strlen(c),f)) return;
     g_hash_table_foreach(settings,(GHFunc)do_save_setting,f);
     e_fclose(f);
     settings_changed = FALSE;
}

static void do_quit(gchar *setting, gchar *value)
{
     g_free(setting);
     g_free(value);
}

void inifile_quit(void)
{
     if (settings_changed) inifile_save();
     g_hash_table_foreach(settings,(GHFunc)do_quit,NULL);
     g_hash_table_destroy(settings);
}
