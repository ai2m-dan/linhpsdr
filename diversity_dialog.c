/* Copyright (C)
* 2018 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr

#include "receiver.h"
#include "transmitter.h"
#include "wideband.h"
#include "discovered.h"
#include "adc.h"
#include "dac.h"
#include "radio.h"
#include "main.h"
#include "mode.h"
#include "filter.h"
#include "diversity_mixer.h"
#include "property.h"

#include "math.h"

static GtkWidget *name_text;


static GtkListStore *store;
static GtkWidget *view;
static GtkCellRenderer *renderer;
static GtkTreeIter iter;
static GtkTreeSortable *sortable;



static GtkWidget *dialog=NULL;
static GtkWidget *gain_coarse_scale=NULL;
static GtkWidget *gain_fine_scale=NULL;
static GtkWidget *phase_fine_scale=NULL;
static GtkWidget *phase_coarse_scale=NULL;


static double gain_coarse, gain_fine;
static double phase_coarse, phase_fine;

double div_gain;
double div_phase;
double div_cos, div_sin;

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
  return FALSE;
}



static void gain_coarse_changed_cb(GtkWidget *widget, gpointer data) {
  DIVMIXER *dmix=(DIVMIXER *)data;
  dmix->gain = gtk_range_get_value(GTK_RANGE(widget));
  set_gain_phase(dmix);
}

static void phase_coarse_changed_cb(GtkWidget *widget, gpointer data) {
  DIVMIXER *dmix=(DIVMIXER *)data;  
  dmix->phase = gtk_range_get_value(GTK_RANGE(widget));
  set_gain_phase(dmix);
}

GtkWidget *create_diversity_dialog(DIVMIXER *dmix) {
  int x;
  int y;
  gchar temp[128];

  GtkWidget *dialog=gtk_dialog_new();
  gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(main_window));
  //g_signal_connect (dialog,"delete_event",G_CALLBACK(delete_event),(gpointer)rx);
  GtkWidget *content=gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  GtkWidget *grid=gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(content),grid);

  gtk_grid_set_column_spacing (GTK_GRID(grid),10);
  gtk_grid_set_row_spacing (GTK_GRID(grid),10);

  GtkWidget *gain_coarse_label=gtk_label_new("Gain (dB, coarse):");
  gtk_misc_set_alignment (GTK_MISC(gain_coarse_label), 0, 0);
  gtk_widget_show(gain_coarse_label);
  gtk_grid_attach(GTK_GRID(grid),gain_coarse_label,0,1,1,1);
  
  gain_coarse_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-25.0,+25.0,0.5);
  gtk_widget_set_size_request (gain_coarse_scale, 300, 25);
  gtk_range_set_value(GTK_RANGE(gain_coarse_scale),gain_coarse);
  gtk_widget_show(gain_coarse_scale);
  gtk_grid_attach(GTK_GRID(grid),gain_coarse_scale,1,1,1,1);
  g_signal_connect(G_OBJECT(gain_coarse_scale),"value_changed",G_CALLBACK(gain_coarse_changed_cb),dmix);

  GtkWidget *phase_coarse_label=gtk_label_new("Phase (coarse):");
  gtk_misc_set_alignment (GTK_MISC(phase_coarse_label), 0, 0);
  gtk_widget_show(phase_coarse_label);
  gtk_grid_attach(GTK_GRID(grid),phase_coarse_label,0,3,1,1);

  phase_coarse_scale=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,-180.0,180.0,1.0);
  gtk_widget_set_size_request (phase_coarse_scale, 300, 25);
  gtk_range_set_value(GTK_RANGE(phase_coarse_scale),phase_coarse);
  gtk_widget_show(phase_coarse_scale);
  gtk_grid_attach(GTK_GRID(grid),phase_coarse_scale,1,3,1,1);
  g_signal_connect(G_OBJECT(phase_coarse_scale),"value_changed",G_CALLBACK(phase_coarse_changed_cb),dmix);

  gtk_container_add(GTK_CONTAINER(content),grid);


  gtk_widget_show_all(dialog);
  return dialog;
}
