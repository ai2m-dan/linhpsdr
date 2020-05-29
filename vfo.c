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
#include <string.h>
#include <wdsp.h>

#include "agc.h"
#include "mode.h"
#include "filter.h"
#include "discovered.h"
#include "receiver.h"
#include "receiver_dialog.h"
#include "transmitter.h"
#include "wideband.h"
#include "adc.h"
#include "dac.h"
#include "radio.h"
#include "band.h"
#include "main.h"
#include "vfo.h"
#include "bookmark_dialog.h"
#include "rigctl.h"
#include "bpsk.h"

enum {
  BUTTON_NONE=-1,
  BUTTON_LOCK=0,
  BUTTON_MODE,
  BUTTON_FILTER,
  BUTTON_NB,
  BUTTON_NR,
  BUTTON_SNB,
  BUTTON_ANF,
  BUTTON_AGC1,
  BUTTON_AGC2,
  BUTTON_BMK,
  BUTTON_CTUN1,
  BUTTON_CTUN2,
  BUTTON_DUP,
  BUTTON_RIT,
  VALUE_RIT1,
  VALUE_RIT2,
  BUTTON_XIT,
  VALUE_XIT1,
  VALUE_XIT2,
  BUTTON_UP1,
  BUTTON_UP2,
  BUTTON_BPSK,
  BUTTON_ATOB,
  BUTTON_BTOA,
  BUTTON_ASWAPB,
  BUTTON_SPLIT,
  BUTTON_STEP,
  BUTTON_ZOOM,
  BUTTON_VFO,
  SLIDER_AF,
  SLIDER_AGC
};

typedef struct _choice {
  RECEIVER *rx;
  int selection;
} CHOICE;

typedef struct _step {
  gint64 step;
  char *label;
} STEP;

const long long ll_step[13]= {
   10000000000LL,
   1000000000LL,
   100000000LL,
   10000000LL,
   1000000LL,
   0LL,
   100000LL,
   10000LL,
   1000LL,
   0LL,
   100LL,
   10LL,
   1LL
};

#define STEPS 25
const int steps[STEPS] = {1,10,25,50,100,250,500,1000,2000,2500,5000,6250,9000,10000,12500,15000,20000,25000,30000,50000,100000,250000,500000,1000000,10000000};

   
static gint slider=BUTTON_NONE;
static gboolean slider_moved=FALSE;
static gint slider_lastx;

static gboolean vfo_configure_event_cb(GtkWidget *widget,GdkEventConfigure *event,gpointer data) {
  RECEIVER *rx=(RECEIVER *)data;
  gint width=gtk_widget_get_allocated_width(widget);
  gint height=gtk_widget_get_allocated_height(widget);
//g_print("vfo_configure_event_cb: width=%d height=%d\n",width,height);
  if(rx->vfo_surface) {
    cairo_surface_destroy(rx->vfo_surface);
  }
  rx->vfo_surface=gdk_window_create_similar_surface(gtk_widget_get_window(widget),CAIRO_CONTENT_COLOR,width,height);

  /* Initialize the surface to black */
  cairo_t *cr;
  cr = cairo_create (rx->vfo_surface);
  cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
  cairo_paint (cr);
  cairo_destroy(cr);
  return TRUE;
}


static gboolean vfo_draw_cb(GtkWidget *widget,cairo_t *cr,gpointer data) {
  RECEIVER *rx=(RECEIVER *)data;
  if(rx->vfo_surface!=NULL) {
    cairo_set_source_surface (cr, rx->vfo_surface, 0.0, 0.0);
    cairo_paint (cr);
  }
  return FALSE;
}

static int which_button(RECEIVER *rx,int x,int y) {
  int button=BUTTON_NONE;
  if(y<=12) {
    if(x>=355 && x<430) {
      button=BUTTON_STEP;
    } else if(x>285 && x<355) {
      button=BUTTON_ZOOM;
    } else if (x>195 && x<230) {
      button=BUTTON_SPLIT;
    } else {
      if(x>=70 && x<175) {
        button=(x/35)-2+BUTTON_ATOB;
      }
    }
  } else if(y>=48) {; 
    int click_loc = (x-5)/35;
    if (click_loc < BUTTON_ATOB) button = click_loc;
  } else if(x>=560) {
    if(y<=35) {
      button=SLIDER_AF;
    } else {
      button=SLIDER_AGC;
    }
  }
  if(y>12 && y<48){
    if(x>=rx->vfo_a_x && x<rx->vfo_b_x+rx->vfo_b_width){
      button=BUTTON_VFO;
    }
  }

  return button;
}

void mode_cb(GtkWidget *menu_item,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  receiver_mode_changed(choice->rx,choice->selection);
  if(choice->rx->split!=SPLIT_OFF) {
    choice->rx->mode_b=choice->selection;
  }
  if(radio->transmitter->rx==choice->rx) {
    if(choice->rx->split!=SPLIT_OFF) {
      transmitter_set_mode(radio->transmitter,choice->rx->mode_b);
    } else {
      transmitter_set_mode(radio->transmitter,choice->rx->mode_a);
    }
  }
  update_vfo(choice->rx);
  g_free(choice);
}

void filter_cb(GtkWidget *menu_item,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  receiver_filter_changed(choice->rx,choice->selection);
  update_vfo(choice->rx);
  g_free(choice);
}

void nb_cb(GtkWidget *menu_item,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  switch(choice->selection) {
    case 0:
      choice->rx->nb=FALSE;
      choice->rx->nb2=FALSE;
      break;
    case 1:
      choice->rx->nb=TRUE;
      choice->rx->nb2=FALSE;
      break;
    case 2:
      choice->rx->nb=FALSE;
      choice->rx->nb2=TRUE;
      break;
  }
  update_noise(choice->rx);
  g_free(choice);
}

void nr_cb(GtkWidget *menu_item,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  switch(choice->selection) {
    case 0:
      choice->rx->nr=FALSE;
      choice->rx->nr2=FALSE;
      break;
    case 1:
      choice->rx->nr=TRUE;
      choice->rx->nr2=FALSE;
      break;
    case 2:
      choice->rx->nr=FALSE;
      choice->rx->nr2=TRUE;
      break;
  }
  update_noise(choice->rx);
  g_free(choice);
}

void agc_cb(GtkWidget *menu_item,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  choice->rx->agc=choice->selection;
  set_agc(choice->rx);
  update_vfo(choice->rx);
  g_free(choice);
}

void zoom_cb(GtkWidget *menu_item,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  receiver_change_zoom(choice->rx,choice->selection);
  update_vfo(choice->rx);
  g_free(choice);
}

void band_cb(GtkWidget *menu_item,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  BAND *b;
  int mode_a;
  long long frequency_a;
  long long lo_a=0LL;
  long long error_a=0LL;

  switch(choice->selection) {
    case band2200:
      switch(choice->rx->mode_a) {
        case LSB:
        case USB:
        case DSB:
          mode_a=LSB;
          frequency_a=136000LL;
          break;
        case CWL:
        case CWU:
          mode_a=CWL;
          frequency_a=136000LL;
          break;
        case FMN:
        case AM:
        case DIGU:
        case SPEC:
        case DIGL:
        case SAM:
        case DRM:
          mode_a=LSB;
          frequency_a=136000LL;
          break;
      }
      break;
    case band630:
      switch(choice->rx->mode_a) {
        case LSB:
        case USB:
        case DSB:
        case CWL:
        case CWU:
        case FMN:
        case AM:
        case DIGU:
        case SPEC:
        case DIGL:
        case SAM:
        case DRM:
          mode_a=CWL;
          frequency_a=472100LL;
          break;
      }
      break;
    case band160:
      switch(choice->rx->mode_a) {
        case LSB:
        case USB:
        case DSB:
          mode_a=LSB;
          frequency_a=1900000LL;
          break;
        case CWL:
        case CWU:
          mode_a=CWL;
          frequency_a=1830000LL;
          break;
        case FMN:
        case AM:
        case DIGU:
        case SPEC:
        case DIGL:
        case SAM:
        case DRM:
          mode_a=LSB;
          frequency_a=1900000LL;
          break;
      }
      break;
    case band80:
      switch(choice->rx->mode_a) {
        case LSB:
        case USB:
        case DSB:
          mode_a=LSB;
          frequency_a=3700000LL;
          break;
        case CWL:
        case CWU:
          mode_a=CWL;
          frequency_a=3520000LL;
          break;
        case FMN:
        case AM:
        case DIGU:
        case SPEC:
        case DIGL:
        case SAM:
        case DRM:
          mode_a=LSB;
          frequency_a=3700000LL;
          break;
      }
      break;
    case band60:
      switch(choice->rx->mode_a) {
        case LSB:
        case USB:
        case DSB:
          mode_a=LSB;
          switch(radio->region) {
            case REGION_OTHER:
              frequency_a=5330000LL;
              break;
            case REGION_UK:
              frequency_a=5280000LL;
              break;
          }
          break;
        case CWL:
        case CWU:
          mode_a=CWL;
          switch(radio->region) {
            case REGION_OTHER:
              frequency_a=5330000LL;
              break;
            case REGION_UK:
              frequency_a=5280000LL;
              break;
          }
          break;
        case FMN:
        case AM:
        case DIGU:
        case SPEC:
        case DIGL:
        case SAM:
        case DRM:
          mode_a=LSB;
          switch(radio->region) {
            case REGION_OTHER:
              frequency_a=5330000LL;
              break;
            case REGION_UK:
              frequency_a=5280000LL;
              break;
          }
          break;
      }
      break;
    case band40:
      switch(choice->rx->mode_a) {
        case LSB:
        case USB:
        case DSB:
          mode_a=LSB;
          frequency_a=7100000LL;
          break;
        case CWL:
        case CWU:
          mode_a=CWL;
          frequency_a=7020000LL;
          break;
        case DIGU:
        case SPEC:
        case DIGL:
          mode_a=LSB;
          frequency_a=7070000LL;
          break;
        case FMN:
        case AM:
        case SAM:
        case DRM:
          mode_a=LSB;
          frequency_a=7100000LL;
          break;
      }
      break;
    case band30:
      switch(choice->rx->mode_a) {
        case LSB:
        case USB:
        case DSB:
          mode_a=USB;
          frequency_a=10145000LL;
          break;
        case CWL:
        case CWU:
          mode_a=CWU;
          frequency_a=10120000LL;
          break;
        case FMN:
        case AM:
        case DIGU:
        case SPEC:
        case DIGL:
        case SAM:
        case DRM:
          mode_a=USB;
          frequency_a=10145000LL;
          break;
      }
      break;
    case band20:
      switch(choice->rx->mode_a) {
        case LSB:
        case USB:
        case DSB:
          mode_a=USB;
          frequency_a=14150000LL;
          break;
        case CWL:
        case CWU:
          mode_a=CWU;
          frequency_a=14020000LL;
          break;
        case DIGU:
        case SPEC:
        case DIGL:
          mode_a=USB;
          frequency_a=14070000LL;
          break;
        case FMN:
        case AM:
        case SAM:
        case DRM:
          mode_a=USB;
          frequency_a=14020000LL;
          break;
      }
      break;
    case band17:
      switch(choice->rx->mode_a) {
        case LSB:
        case USB:
        case DSB:
          mode_a=USB;
          frequency_a=18140000LL;
          break;
        case CWL:
        case CWU:
          mode_a=CWU;
          frequency_a=18080000LL;
          break;
        case FMN:
        case AM:
        case DIGU:
        case SPEC:
        case DIGL:
        case SAM:
        case DRM:
          mode_a=USB;
          frequency_a=18140000LL;
          break;
      }
      break;
    case band15:
      switch(choice->rx->mode_a) {
        case LSB:
        case USB:
        case DSB:
          mode_a=USB;
          frequency_a=21200000LL;
          break;
        case CWL:
        case CWU:
          mode_a=CWU;
          frequency_a=21080000LL;
          break;
        case FMN:
        case AM:
        case DIGU:
        case SPEC:
        case DIGL:
        case SAM:
        case DRM:
          mode_a=USB;
          frequency_a=21200000LL;
          break;
      }
      break;
    case band12:
      switch(choice->rx->mode_a) {
        case LSB:
        case USB:
        case DSB:
          mode_a=USB;
          frequency_a=24960000LL;
          break;
        case CWL:
        case CWU:
          mode_a=CWU;
          frequency_a=24900000LL;
          break;
        case FMN:
        case AM:
        case DIGU:
        case SPEC:
        case DIGL:
        case SAM:
        case DRM:
          mode_a=USB;
          frequency_a=24960000LL;
          break;
      }
      break;
    case band10:
      switch(choice->rx->mode_a) {
        case LSB:
        case USB:
        case DSB:
          mode_a=USB;
          frequency_a=28300000LL;
          break;
        case CWL:
        case CWU:
          mode_a=CWU;
          frequency_a=28020000LL;
          break;
        case FMN:
        case AM:
        case DIGU:
        case SPEC:
        case DIGL:
        case SAM:
        case DRM:
          mode_a=USB;
          frequency_a=28300000LL;
          break;
      }
      break;
    case band6:
      switch(choice->rx->mode_a) {
        case LSB:
        case USB:
        case DSB:
          mode_a=USB;
          frequency_a=51000000LL;
          break;
        case CWL:
        case CWU:
          mode_a=CWU;
          frequency_a=50090000LL;
          break;
        case FMN:
        case AM:
        case DIGU:
        case SPEC:
        case DIGL:
        case SAM:
        case DRM:
          mode_a=USB;
          frequency_a=51000000LL;
          break;
      }
      break;
#ifdef SOAPYSDR
    case band70:
      mode_a=USB;
      frequency_a=70300000LL;
      break;
    case band220:
      mode_a=USB;
      frequency_a=430300000LL;
      break;
    case band430:
      mode_a=USB;
      frequency_a=430300000LL;
      break;
    case band902:
      mode_a=USB;
      frequency_a=430300000LL;
      break;
    case band1240:
      mode_a=USB;
      frequency_a=1240300000LL;
      break;
    case band2300:
      mode_a=USB;
      frequency_a=2300300000LL;
      break;
    case band3400:
      mode_a=USB;
      frequency_a=3400300000LL;
      break;
    case bandAIR:
      mode_a=AM;
      frequency_a=126825000LL;
      break;
#endif
    case bandGen:
      mode_a=AM;
      frequency_a=5975000LL;
      break;
    case bandWWV:
      mode_a=SAM;
      frequency_a=10000000LL;
      break;
    default:
      b=band_get_band(choice->selection);
      mode_a=USB;
      frequency_a=b->frequencyMin;
      lo_a=b->frequencyLO;
      error_a=b->errorLO;
      break;
  }
  choice->rx->band_a=choice->selection;
  choice->rx->mode_a=mode_a;
  choice->rx->frequency_a=frequency_a;
  choice->rx->lo_a=lo_a;
  choice->rx->error_a=error_a;
  choice->rx->ctun=FALSE;
  choice->rx->ctun_offset=0;
  receiver_band_changed(choice->rx,band);
  if(radio->transmitter) {
    if(radio->transmitter->rx==choice->rx) {
      if(choice->rx->split!=SPLIT_OFF) {
        transmitter_set_mode(radio->transmitter,choice->rx->mode_b);
      } else {
        transmitter_set_mode(radio->transmitter,choice->rx->mode_a);
      }
    }
  }
  g_free(choice);
}

void split_cb(GtkWidget *menu_item,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  RECEIVER *rx=choice->rx;
  rx->split=choice->selection;
  frequency_changed(rx);
  if(radio->transmitter->rx==rx) {
    switch(rx->split) {
      case SPLIT_OFF:
        transmitter_set_mode(radio->transmitter,rx->mode_a);
        break;
      case SPLIT_ON:
        // Split mode in CW, RX on VFO A, TX on VFO B.
        // When mode turned on, default to VFO A +1 kHz
        if (rx->mode_a == CWL || rx->mode_a ==CWU) {
          // Most pile-ups start with UP 1
          rx->frequency_b = rx->frequency_a + 1000; 
          rx->mode_b=rx->mode_a;
        }
        transmitter_set_mode(radio->transmitter,rx->mode_b);
        break;
      case SPLIT_SAT:
      case SPLIT_RSAT:
        transmitter_set_mode(radio->transmitter,rx->mode_b);
        break;
    }
  }
  update_vfo(rx);
  g_free(choice);
}

void step_cb(GtkWidget *menu_item,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  choice->rx->step=choice->selection;
  update_vfo(choice->rx);
  g_free(choice);
}

void rit_cb(GtkWidget *menu_item,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  choice->rx->rit_step=choice->selection;
  update_vfo(choice->rx);
  g_free(choice);
}

void xit_cb(GtkWidget *menu_item,gpointer data) {
  CHOICE *choice=(CHOICE *)data;
  radio->transmitter->xit_step=choice->selection;
  update_vfo(choice->rx);
  g_free(choice);
}


static gboolean vfo_press_event_cb(GtkWidget *widget,GdkEventButton *event,gpointer data) {
  GtkWidget *menu;
  GtkWidget *menu_item;
  RECEIVER *rx=(RECEIVER *)data;
  gint temp_band;
  gint64 temp_frequency;
  gint temp_mode;
  gint temp_filter;
  gint64 temp_lo;
  gint64 temp_error;
  int i;
  CHOICE *choice;
  FILTER *mode_filters;
  BAND *band;
  int x=(int)event->x;
  int y=(int)event->y;
  char text[32];

//fprintf(stderr,"vfo_press_event_cb: x=%d y=%d\n",x,y);

  switch(which_button(rx,x,y)) {
    case BUTTON_LOCK:
      switch(event->button) {
        case 1:  // LEFT
            rx->locked=!rx->locked;
            update_vfo(rx);
          break;
        case 3:  // RIGHT
          break;
      }
      break;
    case BUTTON_MODE:
      switch(event->button) {
        case 1:  // LEFT
          menu=gtk_menu_new();
          for(i=0;i<MODES;i++) {
            menu_item=gtk_menu_item_new_with_label(mode_string[i]);
            choice=g_new0(CHOICE,1);
            choice->rx=rx;
            choice->selection=i;
            g_signal_connect(menu_item,"activate",G_CALLBACK(mode_cb),choice);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          }
          gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
          gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
#else
          gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,event->button,event->time);
#endif
          break;
        case 3:  // RIGHT
          break;
      }
      break;
    case BUTTON_FILTER:
      switch(event->button) {
        case 1:  // LEFT
          if(rx->mode_a==FMN) {
            menu=gtk_menu_new();
            menu_item=gtk_menu_item_new_with_label("2.5k Dev");
            choice=g_new0(CHOICE,1);
            choice->rx=rx;
            choice->selection=0;
            g_signal_connect(menu_item,"activate",G_CALLBACK(filter_cb),choice);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
            menu_item=gtk_menu_item_new_with_label("5.0k Dev");
            choice=g_new0(CHOICE,1);
            choice->rx=rx;
            choice->selection=1;
            g_signal_connect(menu_item,"activate",G_CALLBACK(filter_cb),choice);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          } else {
            mode_filters=filters[rx->mode_a];
            menu=gtk_menu_new();
            for(i=0;i<FILTERS;i++) {
              if(i>=FVar1) {
                sprintf(text,"%s (%d..%d)",mode_filters[i].title,mode_filters[i].low,mode_filters[i].high);
                menu_item=gtk_menu_item_new_with_label(text);
              } else {
                menu_item=gtk_menu_item_new_with_label(mode_filters[i].title);
              }
              choice=g_new0(CHOICE,1);
              choice->rx=rx;
              choice->selection=i;
              g_signal_connect(menu_item,"activate",G_CALLBACK(filter_cb),choice);
              gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
            }
          }
          gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
          gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
#else
          gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,event->button,event->time);
#endif
          break;
        case 3:  // RIGHT
          break;
      }
      break;
    case BUTTON_NB:
      switch(event->button) {
        case 1:  // LEFT
          menu=gtk_menu_new();
          menu_item=gtk_menu_item_new_with_label("OFF");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=0;
          g_signal_connect(menu_item,"activate",G_CALLBACK(nb_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("NB");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=1;
          g_signal_connect(menu_item,"activate",G_CALLBACK(nb_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("NB2");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=2;
          g_signal_connect(menu_item,"activate",G_CALLBACK(nb_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
          gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
#else
          gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,event->button,event->time);
#endif
          break;
        case 3:  // RIGHT
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=0;
          nb_cb(0, choice);
          break;
      }
      break;
    case BUTTON_NR:
      switch(event->button) {
        case 1:  // LEFT
          menu=gtk_menu_new();
          menu_item=gtk_menu_item_new_with_label("OFF");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=0;
          g_signal_connect(menu_item,"activate",G_CALLBACK(nr_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("NR");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=1;
          g_signal_connect(menu_item,"activate",G_CALLBACK(nr_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("NR2");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=2;
          g_signal_connect(menu_item,"activate",G_CALLBACK(nr_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
          gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
#else
          gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,event->button,event->time);
#endif
          break;
        case 3:  // RIGHT
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=0;
          nr_cb(0, choice);
          break;
      }
      break;
    case BUTTON_SNB:
      switch(event->button) {
        case 1:  // LEFT
          rx->snb=!rx->snb;
          SetRXASNBARun(rx->channel, rx->snb);
          break;
        case 3:  // RIGHT
          break;
      }
      break;
    case BUTTON_ANF:
      switch(event->button) {
        case 1:  // LEFT
          rx->anf=!rx->anf;
          SetRXAANFRun(rx->channel, rx->anf);
          break;
        case 3:  // RIGHT
          break;
      }
      break;
    case BUTTON_AGC1:
    case BUTTON_AGC2:
      switch(event->button) {
        case 1:  // LEFT
          menu=gtk_menu_new();
          menu_item=gtk_menu_item_new_with_label("OFF");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=AGC_OFF;
          g_signal_connect(menu_item,"activate",G_CALLBACK(agc_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("LONG");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=AGC_LONG;
          g_signal_connect(menu_item,"activate",G_CALLBACK(agc_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("SLOW");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=AGC_SLOW;
          g_signal_connect(menu_item,"activate",G_CALLBACK(agc_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("MEDIUM");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=AGC_MEDIUM;
          g_signal_connect(menu_item,"activate",G_CALLBACK(agc_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("FAST");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=AGC_FAST;
          g_signal_connect(menu_item,"activate",G_CALLBACK(agc_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
          gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
#else
          gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,event->button,event->time);
#endif
          break;
        case 3:  // RIGHT
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=AGC_OFF;
          agc_cb(0, choice);
          break;
      }
      break;
    case BUTTON_BMK:
      switch(event->button) {
        case 1:  // LEFT
          g_print("Diversity rx %d\n", rx->diversity);
          if(rx->diversity) {
            //Delete receiver
            int this_mixer = rx->dmix_id;
            //int hidden_rx_channel = radio->divmixer[this_mixer]->rx_hidden->channel;
            delete_diversity_mixer(radio->divmixer[this_mixer]);
                     
            //delete_receiver(radio->receiver[hidden_rx_channel]);
            rx->diversity = FALSE;
          } else {
            //RADIO *r=(RADIO *)data;
            // Full capacity for mixers
            // if (radio->diversity_mixers < 1) 
            g_print("Add new rx\n");
            int new_hidden_rx  = add_receiver(radio, FALSE);            
            if (new_hidden_rx > 0) {
              g_print("-----------Hidden RX added %d\n", new_hidden_rx);
              int dmix_num = add_diversity_mixer(radio, rx, radio->receiver[new_hidden_rx]);
              if (dmix_num > -1) {
                g_print("Vis mix chan %d\n", rx->dmix_id);
                g_print("Hid mix chan %d\n", radio->receiver[new_hidden_rx]->dmix_id); 
                g_print("channel %d\n", radio->divmixer[dmix_num]->rx_hidden->channel);            
                create_diversity_dialog(radio->divmixer[dmix_num]);
                rx->diversity = TRUE;
              }
              
            } else {
              g_print("^^^^^^^^Failed to add new rx\n");
            }
            
          }        
          break;
        case 3:  // RIGHT
          if(rx->bookmark_dialog==NULL) {
            rx->bookmark_dialog=create_bookmark_dialog(rx,ADD_BOOKMARK,NULL);
          }
          break;
      }
      break;
    case BUTTON_RIT:
      switch(event->button) {
        case 1:  // LEFT
          rx->rit_enabled=!rx->rit_enabled;
          frequency_changed(rx);
          update_frequency(rx);
          break;
        case 3:  // RIGHT
          menu=gtk_menu_new();

          menu_item=gtk_menu_item_new_with_label("1Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=1;
          g_signal_connect(menu_item,"activate",G_CALLBACK(rit_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);

          menu_item=gtk_menu_item_new_with_label("5Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=5;
          g_signal_connect(menu_item,"activate",G_CALLBACK(rit_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);

          menu_item=gtk_menu_item_new_with_label("10Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=10;
          g_signal_connect(menu_item,"activate",G_CALLBACK(rit_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);

          menu_item=gtk_menu_item_new_with_label("100Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=100;
          g_signal_connect(menu_item,"activate",G_CALLBACK(rit_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);

          menu_item=gtk_menu_item_new_with_label("1000Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=1000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(rit_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);

          gtk_widget_show_all(menu);
          gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
          gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
#else
          gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,event->button,event->time);
#endif
          break;
      }
      break;
    case VALUE_RIT1:
    case VALUE_RIT2:
      switch(event->button) {
        case 1:  // LEFT
          break;
        case 3:  // RIGHT
          rx->rit=0;
          frequency_changed(rx);
          update_frequency(rx);
          break;
      }
      break;
    case BUTTON_XIT:
      switch(event->button) {
        case 1:  // LEFT
          if(radio->transmitter!=NULL) {
            radio->transmitter->xit_enabled=!radio->transmitter->xit_enabled;
          }
          break;
        case 3:  // RIGHT
          menu=gtk_menu_new();

          menu_item=gtk_menu_item_new_with_label("1Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=1;
          g_signal_connect(menu_item,"activate",G_CALLBACK(xit_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);

          menu_item=gtk_menu_item_new_with_label("5Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=5;
          g_signal_connect(menu_item,"activate",G_CALLBACK(xit_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);

          menu_item=gtk_menu_item_new_with_label("10Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=10;
          g_signal_connect(menu_item,"activate",G_CALLBACK(xit_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);

          menu_item=gtk_menu_item_new_with_label("100Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=100;
          g_signal_connect(menu_item,"activate",G_CALLBACK(xit_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);

          menu_item=gtk_menu_item_new_with_label("1000Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=1000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(xit_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);

          gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
          gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
#else
          gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,event->button,event->time);
#endif
          break;
      }
      break;


    case VALUE_XIT1:
    case VALUE_XIT2:
      switch(event->button) {
        case 1:  // LEFT
          break;
        case 3:  // RIGHT
          radio->transmitter->xit=0;
          break;
      }
      break;

    case BUTTON_UP1:
      radio->transmitter->xit_enabled=TRUE;
      if(rx->mode_b==CWL || rx->mode_b==CWU) {
        radio->transmitter->xit=1000LL;
      } else {
        radio->transmitter->xit=5000LL;
      }
      break;
    case BUTTON_UP2:
      radio->transmitter->xit_enabled=TRUE;
      if(rx->mode_b==CWL || rx->mode_b==CWU) {
        radio->transmitter->xit=2000LL;
      } else {
        radio->transmitter->xit=10000LL;
      }
      break;

    case BUTTON_CTUN1:
    case BUTTON_CTUN2:
      rx->ctun=!rx->ctun;
      rx->ctun_offset=0;
      rx->ctun_frequency=rx->frequency_a;
      rx->ctun_min=rx->frequency_a-(rx->sample_rate/2);
      rx->ctun_max=rx->frequency_a+(rx->sample_rate/2);
      if(!rx->ctun) {
        SetRXAShiftRun(rx->channel, 0);
      } else {
        SetRXAShiftRun(rx->channel, 1);
      }
      frequency_changed(rx);
      update_frequency(rx);
      break;
    case BUTTON_DUP:
      rx->duplex=!rx->duplex;
      break;
    case BUTTON_BPSK:
      if(rx->bpsk) {
        destroy_bpsk(rx);
      } else {
        create_bpsk(rx);
      }
      break;
    case BUTTON_ATOB:
      switch(event->button) {
        case 1:  // LEFT
          rx->band_b=rx->band_a;
          rx->frequency_b=rx->frequency_a;
          rx->mode_b=rx->mode_a;
          rx->filter_b=rx->filter_a;
          rx->lo_b=rx->lo_a;
          rx->error_b=rx->error_a;
          frequency_changed(rx);
          break;
        case 3:  // RIGHT
          break;
      }
      break;
    case BUTTON_BTOA:
      switch(event->button) {
        case 1:  // LEFT
          rx->band_a=rx->band_b;
          rx->frequency_a=rx->frequency_b;
          rx->mode_a=rx->mode_b;
          rx->filter_a=rx->filter_b;
          rx->lo_a=rx->lo_b;
          rx->error_a=rx->error_b;
          frequency_changed(rx);
          break;
        case 3:  // RIGHT
          break;
      }
      break;
    case BUTTON_ASWAPB:
      switch(event->button) {
        case 1:  // LEFT
          temp_band=rx->band_a;
          temp_frequency=rx->frequency_a;
          temp_mode=rx->mode_a;
          temp_filter=rx->filter_a;
          temp_lo=rx->lo_a;
          temp_error=rx->error_a;

          rx->band_a=rx->band_b;
          rx->frequency_a=rx->frequency_b;
          rx->mode_a=rx->mode_b;
          rx->filter_a=rx->filter_b;
          rx->lo_a=rx->lo_b;
          rx->error_a=rx->error_b;

          rx->band_b=temp_band;
          rx->frequency_b=temp_frequency;
          rx->mode_b=temp_mode;
          rx->filter_b=temp_filter;
          rx->lo_b=temp_lo;
          rx->error_b=temp_error;

          frequency_changed(rx);
          receiver_mode_changed(rx,rx->mode_a);
          if(radio->transmitter->rx==rx) {
            if(rx->split!=SPLIT_OFF) {
              transmitter_set_mode(radio->transmitter,rx->mode_b);
            } else {
              transmitter_set_mode(radio->transmitter,rx->mode_a);
            }
          }
          break;
        case 3:  // RIGHT
          break;
      }
      break;
    case BUTTON_SPLIT:
      switch(event->button) {
        case 1:  // RIGHT
          if(rx->split==SPLIT_OFF) {
            // turn on standard SPLIT
            CHOICE *choice=g_new0(CHOICE,1);
            choice->rx=rx;
            choice->selection=SPLIT_ON;
            split_cb(NULL,(void *)choice);
          } else {
            // turns off regardless of split mode
            CHOICE *choice=g_new0(CHOICE,1);
            choice->rx=rx;
            choice->selection=SPLIT_OFF;
            split_cb(NULL,(void *)choice);
          }
          break;
        case 3:  // RIGHT
          menu=gtk_menu_new();
          menu_item=gtk_menu_item_new_with_label("Off");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=SPLIT_OFF;
          g_signal_connect(menu_item,"activate",G_CALLBACK(split_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("SPLIT");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=SPLIT_ON;
          g_signal_connect(menu_item,"activate",G_CALLBACK(split_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("SAT");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=SPLIT_SAT;
          g_signal_connect(menu_item,"activate",G_CALLBACK(split_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("RSAT");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=SPLIT_RSAT;
          g_signal_connect(menu_item,"activate",G_CALLBACK(split_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
          gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
#else
          gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,event->button,event->time);
#endif
          break;
      }
      break;
    case BUTTON_STEP:
      switch(event->button) {
        case 1:  // LEFT
          menu=gtk_menu_new();
          menu_item=gtk_menu_item_new_with_label("1Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=1;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("10Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=10;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("25Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=25;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("50Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=50;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("100Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=100;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("250Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=250;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("500Hz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=500;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("1kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=1000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("2kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=2000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("2.5kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=2500;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("5kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=5000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("6.25kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=6250;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("9kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=9000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("10kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=10000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("12.5kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=12500;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("15kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=15000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("20kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=20000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("25kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=25000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("30kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=30000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("50kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=50000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("100kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=100000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("250kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=250000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("500kHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=500000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("1MHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=1000000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("10MHz");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=10000000;
          g_signal_connect(menu_item,"activate",G_CALLBACK(step_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
          gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
#else
          gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,event->button,event->time);
#endif
          break;
        case 3:  // RIGHT
          break;
      }
      break;
    case BUTTON_ZOOM:
      switch(event->button) {
        case 1:  // LEFT
          menu=gtk_menu_new();
          menu_item=gtk_menu_item_new_with_label("x1");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=1;
          g_signal_connect(menu_item,"activate",G_CALLBACK(zoom_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("x2");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=2;
          g_signal_connect(menu_item,"activate",G_CALLBACK(zoom_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("x3");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=3;
          g_signal_connect(menu_item,"activate",G_CALLBACK(zoom_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("x4");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=4;
          g_signal_connect(menu_item,"activate",G_CALLBACK(zoom_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("x5");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=5;
          g_signal_connect(menu_item,"activate",G_CALLBACK(zoom_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("x6");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=6;
          g_signal_connect(menu_item,"activate",G_CALLBACK(zoom_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("x7");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=7;
          g_signal_connect(menu_item,"activate",G_CALLBACK(zoom_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          menu_item=gtk_menu_item_new_with_label("x8");
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=8;
          g_signal_connect(menu_item,"activate",G_CALLBACK(zoom_cb),choice);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
          gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
          gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
#else
          gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,event->button,event->time);
#endif
          break;
        case 3:  // RIGHT
          break;
      }
      break;
      
    case BUTTON_VFO:
      if(x>4 && x<230) {
        switch(event->button) {
          case 1: //LEFT
            if(!rx->locked) {
              menu=gtk_menu_new();
              for(i=0;i<BANDS+XVTRS;i++) {
#ifdef SOAPYSDR
                if(radio->discovered->protocol!=PROTOCOL_SOAPYSDR) {
                  if(i>=band70 && i<=band3400) {
                    continue;
                  }
                }
#endif
                band=(BAND*)band_get_band(i);
                if(strlen(band->title)>0) {
                menu_item=gtk_menu_item_new_with_label(band->title);
                choice=g_new0(CHOICE,1);
                choice->rx=rx;
                choice->selection=i;
                g_signal_connect(menu_item,"activate",G_CALLBACK(band_cb),choice);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
                }
              }
              gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
              gtk_menu_popup_at_pointer(GTK_MENU(menu),(GdkEvent *)event);
#else
              gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,event->button,event->time);
#endif
            }
            break;
          case 3: //RIGHT
            break;
        }
      }
      break;
    case SLIDER_AF:
      slider=SLIDER_AF;
      slider_moved=FALSE;
      slider_lastx=x;
      break;
    case SLIDER_AGC:
      slider=SLIDER_AGC;
      slider_moved=FALSE;
      slider_lastx=x;
/*
      rx->agc_gain=(double)(x-560)-20.0;
      if(rx->agc_gain>120.0) rx->agc_gain=120.0;
      if(rx->agc_gain<-20.0) rx->agc_gain=-20.0;
      SetRXAAGCTop(rx->channel, rx->agc_gain);
      update_vfo(rx);
*/
      break;
    default:
      switch(event->button) {
        case 1:  // LEFT
          break;
        case 3:  // RIGHT
          if(rx->dialog==NULL) {
            rx->dialog=create_receiver_dialog(rx);
          }
          break;
      }
      break;
  }
  update_vfo(rx);
  return TRUE;
}

static gboolean vfo_release_event_cb(GtkWidget *widget,GdkEventButton *event,gpointer data) {
  RECEIVER *rx=(RECEIVER *)data;
  int x=(int)event->x;
  int y=(int)event->y;

  switch(slider) {
    case SLIDER_AF:
      if(slider_moved) {
        int moved=x-slider_lastx;
        rx->volume+=(double)moved/100.0;
      } else {
        rx->volume=(double)(x-560)/100.0;
      }
      if(rx->volume>1.0) rx->volume=1.0;
      if(rx->volume<0.0) rx->volume=0.0;
      if(rx->volume==0.0) {
        SetRXAPanelRun(rx->channel,0);
      } else {
        SetRXAPanelRun(rx->channel,1);
        SetRXAPanelGain1(rx->channel,rx->volume);
      }
      slider=BUTTON_NONE;
      slider_moved=FALSE;
      update_vfo(rx);
      break;
    case SLIDER_AGC:
      if(slider_moved) {
        int moved=x-slider_lastx;
        rx->agc_gain+=(double)moved;
      } else {
        rx->agc_gain=(double)(x-560)-20.0;
      }
      if(rx->agc_gain>120.0) rx->agc_gain=120.0;
      if(rx->agc_gain<-20.0) rx->agc_gain=-20.0;
      SetRXAAGCTop(rx->channel, rx->agc_gain);
      slider=BUTTON_NONE;
      slider_moved=FALSE;
      update_vfo(rx);
      break;
  }

  return TRUE;
}

static gboolean vfo_scroll_event_cb(GtkWidget *widget,GdkEventScroll *event,gpointer data) {
  RECEIVER *rx=(RECEIVER *)data;
  CHOICE *choice;
  int x=(int)event->x;
  int y=(int)event->y;
  int i;

  switch(which_button(rx,x,y)) {
    case BUTTON_STEP:
      for(i=0;i<STEPS;i++) {
        if(steps[i]==rx->step) {
          if(event->direction==GDK_SCROLL_UP) {
            if(i<STEPS-1) {
              choice=g_new0(CHOICE,1);
              choice->rx=rx;
              choice->selection=steps[i+1];
              step_cb(widget,choice);
            }
          } else {
            if(i>0) {
              choice=g_new0(CHOICE,1);
              choice->rx=rx;
              choice->selection=steps[i-1];
              step_cb(widget,choice);
            }
          }
          break;
        }
      }
      break;
    case BUTTON_ZOOM:
      if(event->direction==GDK_SCROLL_UP) {
        if(rx->zoom<8) {
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=rx->zoom+1;
          zoom_cb(widget,choice);
        }
      } else {
        if(rx->zoom>1) {
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=rx->zoom-1;
          zoom_cb(widget,choice);
        }
      }
      break;
    case BUTTON_SPLIT:
      if(event->direction==GDK_SCROLL_UP) {
        if(rx->split<SPLIT_RSAT) {
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=rx->split+1;
          split_cb(widget,choice);
        }
      } else {
        if(rx->split>SPLIT_OFF) {
          choice=g_new0(CHOICE,1);
          choice->rx=rx;
          choice->selection=rx->split-1;
          split_cb(widget,choice);
        }
      }
      break;
    case BUTTON_RIT:
    case VALUE_RIT1:
    case VALUE_RIT2:
      if(rx->rit_enabled) {
        if(event->direction==GDK_SCROLL_UP) {
          rx->rit=rx->rit+rx->rit_step;
          if(rx->rit>10000) rx->rit=10000;
        } else {
          rx->rit=rx->rit-rx->rit_step;
          if(rx->rit<-10000) rx->rit=-10000;
        }
        frequency_changed(rx);
        update_frequency(rx);
      }
      break;
    case BUTTON_XIT:
    case VALUE_XIT1:
    case VALUE_XIT2:
      if(radio->transmitter->xit_enabled) {
        if(event->direction==GDK_SCROLL_UP) {
          radio->transmitter->xit=radio->transmitter->xit+radio->transmitter->xit_step;
          if(radio->transmitter->xit>10000) radio->transmitter->xit=10000;
        } else {
          radio->transmitter->xit=radio->transmitter->xit-radio->transmitter->xit_step;
          if(radio->transmitter->xit<-10000) radio->transmitter->xit=-10000;
        }
      }
      break;
    case SLIDER_AF:
      if(event->direction==GDK_SCROLL_UP) {
        rx->volume=rx->volume+0.01;
        if(rx->volume>1.0) rx->volume=1.0;
      } else {
        rx->volume=rx->volume-0.01;
        if(rx->volume<0.0) rx->volume=0.0;
      }
      if(rx->volume==0.0) {
        SetRXAPanelRun(rx->channel,0);
      } else {
        SetRXAPanelRun(rx->channel,1);
        SetRXAPanelGain1(rx->channel,rx->volume);
      }
      break;
    case SLIDER_AGC:
      if(event->direction==GDK_SCROLL_UP) {
        rx->agc_gain=rx->agc_gain+1.0;
        if(rx->agc_gain>120.0) rx->agc_gain=120.0;
      } else {
        rx->agc_gain=rx->agc_gain-1.0;
        if(rx->agc_gain<-20.0) rx->agc_gain=-20.0;
      }
      SetRXAAGCTop(rx->channel, rx->agc_gain);
      break;
    case BUTTON_VFO:
      if(x>=rx->vfo_a_x && x<(rx->vfo_a_x+rx->vfo_a_width)) {
        // VFO A 
        if(!rx->locked) {
          int digit=(x-rx->vfo_a_x)/(rx->vfo_a_width/rx->vfo_a_digits);
          long long step=0LL;
          if(digit<13) {
            step=ll_step[digit];
          }
          if(event->direction==GDK_SCROLL_DOWN && rx->ctun) {
            step=-step;
          }                    
          if(event->direction==GDK_SCROLL_UP && !rx->ctun) {
            step=-step;
          }                    
          receiver_move(rx,step,TRUE);
        }
      } else if(x>=rx->vfo_b_x && x<(rx->vfo_b_x+rx->vfo_b_width)) {
        // VFO B
        if(!rx->locked) {
          int digit=(x-rx->vfo_b_x)/(rx->vfo_b_width/rx->vfo_b_digits);
          long long step=0LL;
          if(digit<13) {
            step=ll_step[digit];
          }
          if (event->direction==GDK_SCROLL_DOWN) {
            step=-step;
          }
          //receiver_move_b(rx,step,FALSE);
          switch(rx->split) {
            case SPLIT_OFF:
            case SPLIT_ON:
              receiver_move_b(rx,step,TRUE);
              break;
            case SPLIT_SAT:
            case SPLIT_RSAT:
              receiver_move_b(rx,step,FALSE);
              break;
          }
        }
      }
      break;
  }
  update_vfo(rx);
  return TRUE;
}

static gboolean vfo_motion_notify_event_cb(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
  gint x=event->x;
  gint y=event->y;
  GdkModifierType state;
  RECEIVER *rx=(RECEIVER *)data;
  int digit;

  switch(slider) {
    case BUTTON_NONE:
      if(!rx->locked) {
        //gdk_window_get_device_position(event->window,event->device,&x,&y,&state);
        switch(which_button(rx,x,y)) {
          case BUTTON_VFO:
            if(!rx->locked) {
              if(x>=rx->vfo_a_x && x<(rx->vfo_a_x+rx->vfo_a_width)) {
                // VFO A
                digit=(x-rx->vfo_a_x)/(rx->vfo_a_width/rx->vfo_a_digits);
              } else if(x>=rx->vfo_b_x && x<(rx->vfo_b_x+rx->vfo_b_width)) {
                // VFO B
                digit=(x-rx->vfo_b_x)/(rx->vfo_b_width/rx->vfo_b_digits);
              } else {
                // between the vfo frequencies
                digit=-1;
              }
              if(digit>=0 && digit<13) {
                long long step=ll_step[digit];
                if(step==0LL) {
                  gdk_window_set_cursor(gtk_widget_get_window(widget),gdk_cursor_new(GDK_ARROW));
                } else {
                  gdk_window_set_cursor(gtk_widget_get_window(widget),gdk_cursor_new(GDK_DOUBLE_ARROW));
                }
              } else {
                gdk_window_set_cursor(gtk_widget_get_window(widget),gdk_cursor_new(GDK_ARROW));
                  }
            }
            break;
          default:
            gdk_window_set_cursor(gtk_widget_get_window(widget),gdk_cursor_new(GDK_ARROW));
            break;
        }
      }
      break;
    case SLIDER_AF:
      rx->volume+=(double)(x-slider_lastx)/100.0;
      if(rx->volume>1.0) rx->volume=1.0;
      if(rx->volume<0.0) rx->volume=0.0;
      if(rx->volume==0.0) {
        SetRXAPanelRun(rx->channel,0);
      } else {
        SetRXAPanelRun(rx->channel,1);
        SetRXAPanelGain1(rx->channel,rx->volume);
      }
      slider_moved=TRUE;
      slider_lastx=x;
      update_vfo(rx);
      break;
    case SLIDER_AGC:
      rx->agc_gain+=(double)(x-slider_lastx);
      rx->agc_gain=(double)(x-560)-20.0;
      if(rx->agc_gain>120.0) rx->agc_gain=120.0;
      if(rx->agc_gain<-20.0) rx->agc_gain=-20.0;
      SetRXAAGCTop(rx->channel, rx->agc_gain);
      slider_moved=TRUE;
      slider_lastx=x;
      update_vfo(rx);
      break;
  }
  return TRUE;
}

GtkWidget *create_vfo(RECEIVER *rx) {
  g_print("create_vfo: rx=%d\n",rx->channel);
  rx->vfo_surface=NULL;
  GtkWidget *vfo = gtk_drawing_area_new ();
  g_signal_connect (vfo,"configure-event",G_CALLBACK(vfo_configure_event_cb),(gpointer)rx);
  g_signal_connect (vfo,"draw",G_CALLBACK(vfo_draw_cb),(gpointer)rx);
  g_signal_connect(vfo,"motion-notify-event",G_CALLBACK(vfo_motion_notify_event_cb),rx);
  g_signal_connect (vfo,"button-press-event",G_CALLBACK(vfo_press_event_cb),(gpointer)rx);
  g_signal_connect (vfo,"button-release-event",G_CALLBACK(vfo_release_event_cb),(gpointer)rx);
  g_signal_connect(vfo,"scroll_event",G_CALLBACK(vfo_scroll_event_cb),(gpointer)rx);
  gtk_widget_set_events (vfo, gtk_widget_get_events (vfo)
                     | GDK_BUTTON_PRESS_MASK
                     | GDK_BUTTON_RELEASE_MASK
                     | GDK_SCROLL_MASK
                     | GDK_POINTER_MOTION_MASK
                     | GDK_POINTER_MOTION_HINT_MASK);
  return vfo;
}

void update_vfo(RECEIVER *rx) {
  char temp[32];
  cairo_t *cr;
  cairo_text_extents_t extents;

  if(rx!=NULL && rx->vfo_surface!=NULL) {
    cr = cairo_create (rx->vfo_surface);
    SetColour(cr, BACKGROUND);
    cairo_paint(cr);
    long long af=rx->ctun?rx->ctun_frequency:rx->frequency_a;
    long long bf=rx->frequency_b;

    cairo_select_font_face(cr, "Noto Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

    // ************* Draw buttons ******************
    // Lock
    if(rx->locked) {
      RoundedRectangle(cr, 8.0, 51.0, 22.0, 6.0, CLICK_ON);
    } 
    else {
      RoundedRectangle(cr, 8.0, 51.0, 22.0, 6.0, CLICK_OFF);       
    }
    // Mode
    RoundedRectangle(cr, 43.0, 51.0, 22.0, 6.0, CLICK_ON);    
    // Filter bandwidth
    RoundedRectangle(cr, 78.0, 51.0, 20.0, 6.0, CLICK_ON);
    // NB    
    if((rx->nb) || (rx->nb2)) {
      RoundedRectangle(cr, 110.0, 51.0, 22.0, 6.0, CLICK_ON);  
    }
    else {
      RoundedRectangle(cr, 110.0, 51.0, 22.0, 6.0, CLICK_OFF);      
    }
    // NR
    if((rx->nr) || (rx->nr2)) {
      RoundedRectangle(cr, 148.0, 51.0, 18.0, 6.0, CLICK_ON);   
    }  
    else {
      RoundedRectangle(cr, 148.0, 51.0, 18.0, 6.0, CLICK_OFF);
    }
    // SNB
    if(rx->snb) {
      RoundedRectangle(cr, 184.0, 51.0, 18.0, 6.0, CLICK_ON);
    }
    else {
      RoundedRectangle(cr, 184.0, 51.0, 18.0, 6.0, CLICK_OFF);      
    }
    // ANF
    if(rx->anf) {
      RoundedRectangle(cr, 218.0, 51.0, 18.0, 6.0, CLICK_ON);       
    } 
    else {
      RoundedRectangle(cr, 218.0, 51.0, 18.0, 6.0, CLICK_OFF); 
    }
    // AGC
    if(rx->agc == AGC_OFF) {
      RoundedRectangle(cr, 252.0, 51.0, 60.0, 6.0, CLICK_OFF);  
    }
    else {   
      RoundedRectangle(cr, 252.0, 51.0, 60.0, 6.0, CLICK_ON);
    }
    // BMK
    RoundedRectangle(cr, 325.0, 51.0, 18.0, 6.0, CLICK_OFF);      
    
    // CTUN
    if(rx->ctun) {    
      RoundedRectangle(cr, 360.0, 51.0, 35.0, 6.0, CLICK_ON);   
    }
    else {
      RoundedRectangle(cr, 360.0, 51.0, 35.0, 6.0, CLICK_OFF);       
    } 

    // DUP
    if(rx->duplex) {    
      RoundedRectangle(cr, 430.0, 51.0, 18.0, 6.0, CLICK_ON);   
    }
    else {
      RoundedRectangle(cr, 430.0, 51.0, 18.0, 6.0, CLICK_OFF);       
    } 

    // RIT
    if(rx->rit_enabled) {    
      RoundedRectangle(cr, 465.0, 51.0, 18.0, 6.0, CLICK_ON);   
    }
    else {
      RoundedRectangle(cr, 465.0, 51.0, 18.0, 6.0, CLICK_OFF);       
    } 

    // XIT
    if(radio->transmitter!=NULL) {
      if(radio->transmitter->xit_enabled) {    
        RoundedRectangle(cr, 570.0, 51.0, 18.0, 6.0, CLICK_ON);   
      }
      else {
        RoundedRectangle(cr, 570.0, 51.0, 18.0, 6.0, CLICK_OFF);       
      } 
    }

    if(rx->split!=SPLIT_OFF) {
      RoundedRectangle(cr, 200.0, 6.0, 30.0, 5.0, CLICK_ON);    
    }  
    else {
      RoundedRectangle(cr, 200.0, 6.0, 30.0, 5.0, CLICK_OFF);
    }


    //****************************** VFO A and B frequencies
    if(radio!=NULL && radio->transmitter!=NULL && rx==radio->transmitter->rx && radio->transmitter->rx->split!=SPLIT_OFF) {
      SetColour(cr, TEXT_B);
    } else {
      SetColour(cr, TEXT_B);
    }

    cairo_move_to(cr, 5, 12);
    cairo_set_font_size(cr, 12);
    cairo_show_text(cr, "VFO A");

    if(radio!=NULL && radio->transmitter!=NULL && rx==radio->transmitter->rx && radio->transmitter->rx->split==SPLIT_OFF && isTransmitting(radio)) {
      SetColour(cr, WARNING);
    } else {
      SetColour(cr, TEXT_B);
    }


    sprintf(temp,"%5lld.%03lld.%03lld",af/(long long)1000000,(af%(long long)1000000)/(long long)1000,af%(long long)1000);      
  
    cairo_select_font_face(cr, "Noto Mono", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    rx->vfo_a_x = 5;
    rx->vfo_a_digits=strlen(temp);
    cairo_set_font_size(cr, 28);
    cairo_text_extents(cr, temp, &extents);
    rx->vfo_a_width=(int)extents.x_advance;
        
    cairo_move_to(cr, rx->vfo_a_x, 38);
    cairo_show_text(cr, temp);
    
    cairo_select_font_face(cr, "Noto Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

    if(radio!=NULL && radio->transmitter!=NULL && rx==radio->transmitter->rx && radio->transmitter->rx->split!=SPLIT_OFF) {
      SetColour(cr, WARNING);
    } else {
      SetColour(cr, TEXT_C);
    }
    cairo_move_to(cr, 240, 12);
    cairo_set_font_size(cr, 12);
    cairo_show_text(cr, "VFO B");

    if(radio!=NULL && radio->transmitter!=NULL && rx==radio->transmitter->rx && radio->transmitter->rx->split!=SPLIT_OFF && isTransmitting(radio)) {
      SetColour(cr, WARNING);
    } else {
      SetColour(cr, TEXT_C);
    }
    
    sprintf(temp,"%5lld.%03lld.%03lld",bf/(long long)1000000,(bf%(long long)1000000)/(long long)1000,bf%(long long)1000);      

    cairo_select_font_face(cr, "Noto Mono", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 21);
    rx->vfo_b_x = 240;
    rx->vfo_b_digits=strlen(temp);
    cairo_text_extents(cr, temp, &extents);
    rx->vfo_b_width=(int)extents.x_advance;
    
    cairo_move_to(cr, rx->vfo_b_x, 38);
    cairo_show_text(cr, temp);


    //********************* Write text on buttons ***********************
    cairo_select_font_face(cr, "Noto Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    FILTER* band_filters=filters[rx->mode_a];
    cairo_set_font_size(cr, 12);

    int x=5;
    // NB
    cairo_move_to(cr, x, 58);  
    if(rx->locked) {
      SetColour(cr, OFF_WHITE);
    } else {      
      SetColour(cr, DARK_TEXT);
    }
    cairo_show_text(cr, "Lock");
    x+=35;

    cairo_move_to(cr, x, 58);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_show_text(cr, mode_string[rx->mode_a]);
    x+=35;

    cairo_move_to(cr, x, 58);
    if(rx->mode_a==FMN) {
      if(rx->deviation==2500) {
        cairo_show_text(cr, "8000");
      } else {
        cairo_show_text(cr, "16000");
      }
    } else {
      cairo_show_text(cr, band_filters[rx->filter_a].title);
    }
    x+=35;

    cairo_move_to(cr, x, 58);
    if(rx->nb) {
      SetColour(cr, OFF_WHITE);
      cairo_show_text(cr, "NB");
    } else if(rx->nb2) {
      SetColour(cr, OFF_WHITE);
      cairo_show_text(cr, "NB2");
    } else {
      SetColour(cr, DARK_TEXT);
      cairo_show_text(cr, "NB");
    }
    x+=35;

    cairo_move_to(cr, x, 58);
    if(rx->nr) {
      SetColour(cr, OFF_WHITE);
      cairo_show_text(cr, "NR");
    } else if(rx->nr2) {
      SetColour(cr, OFF_WHITE);
      cairo_show_text(cr, "NR2");
    } else {
      SetColour(cr, DARK_TEXT);
      cairo_show_text(cr, "NR");
    }
    x+=35;


    if(rx->snb) {
      SetColour(cr, OFF_WHITE);
    } else {
      SetColour(cr, DARK_TEXT);
    }
    cairo_move_to(cr, x, 58);
    cairo_show_text(cr, "SNB");
    x+=35;

    if(rx->anf) {
      SetColour(cr, OFF_WHITE);
    } else {
      SetColour(cr, DARK_TEXT);
    }
    cairo_move_to(cr, x, 58);
    cairo_show_text(cr, "ANF");
    x+=35;

    cairo_move_to(cr, x, 58);
    switch(rx->agc) {
      case AGC_OFF:
        SetColour(cr, DARK_TEXT);
        cairo_show_text(cr, "AGC OFF");
        break;
      case AGC_LONG:
        SetColour(cr, OFF_WHITE);
        cairo_show_text(cr, "AGC LONG");
        break;
      case AGC_SLOW:
        SetColour(cr, OFF_WHITE);
        cairo_show_text(cr, "AGC SLOW");
        break;
      case AGC_MEDIUM:
        SetColour(cr, OFF_WHITE);
        cairo_show_text(cr, "AGC MED");
        break;
      case AGC_FAST:
        SetColour(cr, OFF_WHITE);
        cairo_show_text(cr, "AGC FAST");
        break;
    }
    x+=35;
    x+=35;
    
    cairo_move_to(cr, x, 58);
    SetColour(cr, DARK_TEXT); 
    cairo_show_text(cr, "DIV");
    x+=35;

    cairo_move_to(cr,x,58);
    if(rx->ctun) {
      SetColour(cr, OFF_WHITE);
    } else {
      SetColour(cr, DARK_TEXT);
    }
    cairo_show_text(cr, "CTUN");
    x+=70;

    cairo_move_to(cr,x,58);
    if(rx->duplex) {
      SetColour(cr, OFF_WHITE);
    } else {
      SetColour(cr, DARK_TEXT);
    }
    cairo_show_text(cr, "DUP");
    x+=35;

    cairo_move_to(cr,x,58);
    if(rx->rit_enabled) {
      SetColour(cr, OFF_WHITE);
    } else {
      SetColour(cr, DARK_TEXT); 
    }
    cairo_show_text(cr, "RIT");
    x+=35;
 
    if(rx->rit_enabled) {
      cairo_move_to(cr,x,58);
      SetColour(cr, OFF_WHITE);
      sprintf(temp,"%ld",rx->rit);
      cairo_show_text(cr, temp);
    }
    x+=35;
    x+=35;

    cairo_move_to(cr,x,58);
    if(radio->transmitter!=NULL) {
      if(radio->transmitter->xit_enabled) {
        SetColour(cr, OFF_WHITE);
      } else {
        SetColour(cr, DARK_TEXT);
      }
      cairo_show_text(cr, "XIT");
      x+=35;

      if(radio->transmitter->xit_enabled) {
        cairo_move_to(cr,x,58);
        SetColour(cr, OFF_WHITE);
        sprintf(temp,"%ld",radio->transmitter->xit);
        cairo_show_text(cr, temp);
      }
      x+=35;
      x+=35;
    }

    
    x=70;
    SetColour(cr, OFF_WHITE);
    cairo_move_to(cr, x, 12);
    cairo_show_text(cr, "A>B");
    x+=35;

    cairo_move_to(cr, x, 12);
    cairo_show_text(cr, "A<B");
    x+=35;

    cairo_move_to(cr, x, 12);
    cairo_show_text(cr, "A<>B");
    x+=57;

    cairo_move_to(cr, x, 12);
    if(rx->split!=SPLIT_OFF) {
      SetColour(cr, OFF_WHITE);
    } else {
      SetColour(cr, DARK_TEXT); 
    }
    switch(rx->split) {
      case SPLIT_OFF:
      case SPLIT_ON:
        cairo_show_text(cr, "SPLIT");
        break;
      case SPLIT_SAT:
        cairo_show_text(cr, "SAT");
        break;
      case SPLIT_RSAT:
        cairo_show_text(cr, "RSAT");
        break;
    }
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr,285,12);
    sprintf(temp,"ZOOM x%d",rx->zoom);
    cairo_show_text(cr, temp);

    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr,355,12);
    sprintf(temp,"STEP %ld Hz",rx->step);
    cairo_show_text(cr, temp);

    
    if (radio!=NULL && radio->transmitter!=NULL) {
      if (radio->transmitter->rx==rx) {
        SetColour(cr, WARNING);
        cairo_move_to(cr,450,12);
        sprintf(temp,"ASSIGNED TX");
        cairo_show_text(cr, temp);
      }
    }
    
    x=500;
    // --------------------------------------- AF Gain control
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_font_size(cr,10);
    cairo_move_to(cr,x,30);
    cairo_show_text(cr,"AF GAIN");
    x+=60;

    SetColour(cr, TEXT_B);
    cairo_rectangle(cr,x,25,rx->volume*100,5);
    cairo_fill(cr);

    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr,x,30);
    cairo_line_to(cr,x+100,30);
    cairo_stroke(cr);

    cairo_move_to(cr,x,30);
    cairo_line_to(cr,x,24);
    cairo_stroke(cr);
    x+=25;
    cairo_move_to(cr,x,30);
    cairo_line_to(cr,x,27);
    cairo_stroke(cr);
    x+=25;
    cairo_move_to(cr,x,30);
    cairo_line_to(cr,x,24);
    cairo_stroke(cr);
    x+=25;
    cairo_move_to(cr,x,30);
    cairo_line_to(cr,x,27);
    cairo_stroke(cr);
    x+=25;
    cairo_move_to(cr,x,30);
    cairo_line_to(cr,x,24);
    cairo_stroke(cr);


    x=500;
    // --------------------------------------- AGC Gain control
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_font_size(cr,10);
    cairo_move_to(cr,x,45);
    cairo_show_text(cr,"AGC GAIN");
    x+=60;

    SetColour(cr, TEXT_C);
    cairo_rectangle(cr,x,40,rx->agc_gain+20.0,5);
    cairo_fill(cr);

    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_move_to(cr,x,45);
    cairo_line_to(cr,x+140,45);
    cairo_stroke(cr);

    cairo_move_to(cr,x,45);
    cairo_line_to(cr,x,42);
    cairo_stroke(cr);
    x+=20;
    cairo_move_to(cr,x,45);
    cairo_line_to(cr,x,39);
    cairo_stroke(cr);
    x+=20;
    cairo_move_to(cr,x,45);
    cairo_line_to(cr,x,42);
    cairo_stroke(cr);
    x+=20;
    cairo_move_to(cr,x,45);
    cairo_line_to(cr,x,42);
    cairo_stroke(cr);
    x+=20;
    cairo_move_to(cr,x,45);
    cairo_line_to(cr,x,42);
    cairo_stroke(cr);
    x+=20;
    cairo_move_to(cr,x,45);
    cairo_line_to(cr,x,42);
    cairo_stroke(cr);
    x+=20;
    cairo_move_to(cr,x,45);
    cairo_line_to(cr,x,42);
    cairo_stroke(cr);
    x+=20;
    cairo_move_to(cr,x,45);
    cairo_line_to(cr,x,39);
    cairo_stroke(cr);
    // --------------------------------------- House keeping
    cairo_destroy (cr);
    gtk_widget_queue_draw (rx->vfo);
  }
}

void RoundedRectangle(cairo_t *cr, double x, double y, double width, double height, int state) {
    double aspect        = 1.0;     
    double        corner_radius = height / 10.0;   

    double radius = corner_radius / aspect;
    double degrees = (22/7) / 180.0;           
           
           
    cairo_arc (cr, x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
    cairo_arc (cr, x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
    cairo_arc (cr, x + radius, y + height - radius, radius, 90 * degrees, 180 * degrees);
    cairo_arc (cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
    cairo_close_path (cr);

    switch(state) {
      case CLICK_ON : SetColour(cr, BOX_ON);
                      break;
      
      case CLICK_OFF : SetColour(cr, BOX_OFF);
                       break;
      
      case INFO_TRUE : SetColour(cr, INFO_ON);
                     break;      
      
      case INFO_FALSE : SetColour(cr, INFO_OFF);
                      break;          
            
      case WARNING_ON : SetColour(cr, WARNING);
                        break;                     
    }

    cairo_fill_preserve (cr);
    cairo_set_line_width (cr, 10.0);
    cairo_stroke (cr);    
}

void SetColour(cairo_t *cr, const int colour) {
    
  switch(colour) {
    case BACKGROUND: {
      cairo_set_source_rgb (cr, 0.1, 0.1, 0.1); 
      break;
    }
    case OFF_WHITE: {
      cairo_set_source_rgb (cr, 0.9, 0.9, 0.9);       
      break;
    }
    case BOX_ON: {
      cairo_set_source_rgb (cr, 0.624, 0.427, 0.690); 
      break;     
    }
    case BOX_OFF: {
      cairo_set_source_rgb (cr, 0.2, 0.2,	0.2);      
      break;
    }    
    case TEXT_A: {
      cairo_set_source_rgb(cr, 0.929, 0.616, 0.502);
      break;
    }
    case TEXT_B: {
      //light blue
      cairo_set_source_rgb(cr, 0.639, 0.800, 0.820);
      break;
    }
    case TEXT_C: {
      // Pale orange
      cairo_set_source_rgb(cr, 0.929,	0.616,	0.502);     
      break;
    }
    case WARNING: {
      // Pale red
        cairo_set_source_rgb (cr, 0.851, 0.271, 0.271);    
      break;
    }    
    case DARK_LINES: {
      // Dark grey
        cairo_set_source_rgb (cr, 0.3, 0.3, 0.3);   
      break;
    }  
    case DARK_TEXT: {
      cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
      break;
    }
    case INFO_ON: {
      cairo_set_source_rgb(cr, 0.15, 0.58, 0.6);
      break;      
    }
    case INFO_OFF: {
      cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
      break;      
    }    
  }
}
  
  
