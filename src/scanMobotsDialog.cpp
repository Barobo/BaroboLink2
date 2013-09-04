/*
   Copyright 2013 Barobo, Inc.

   This file is part of BaroboLink.

   BaroboLink is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   BaroboLink is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with BaroboLink.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "BaroboLink.h"
#include "thread_macros.h"

int g_scanMobotsActive = 0;

gint scanMobotsCompareFunc(
    GtkTreeModel *model,
    GtkTreeIter *a,
    GtkTreeIter *b,
    gpointer data)
{
  const char *astr, *bstr;
  gtk_tree_model_get(model, a, 0, &astr, -1);
  gtk_tree_model_get(model, b, 0, &bstr, -1);
  return strcmp(astr, bstr);
}

void initScanMobotsDialog()
{
  /* Initialize the tree view */
  GtkTreeSelection* s;
  GtkWidget *w;
  w = GTK_WIDGET(gtk_builder_get_object(g_builder, "treeview_scanMobots"));
  s = gtk_tree_view_get_selection(GTK_TREE_VIEW(w));
  gtk_tree_selection_set_mode(s, GTK_SELECTION_MULTIPLE);
  /*
  GtkListStore *ls = GTK_LIST_STORE(gtk_builder_get_object(g_builder, "liststore_scannedRobots"));
  gtk_tree_sortable_set_sort_func(
      GTK_TREE_SORTABLE(ls), 
      0,
      scanMobotsCompareFunc,
      NULL,
      NULL);
      */
  GtkTreeModel* sortModel;
  sortModel = gtk_tree_model_sort_new_with_model(
      GTK_TREE_MODEL( gtk_builder_get_object(g_builder, "liststore_scannedRobots")));
  gtk_tree_view_set_model(GTK_TREE_VIEW(w), sortModel);
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(sortModel), 0, GTK_SORT_ASCENDING);
}

void* scanMobotsThread(void* arg)
{
  g_scanMobotsActive = 1;
  Mobot_clearQueriedAddresses((mobot_t*)g_mobotParent);
  Mobot_queryAddresses((mobot_t*)g_mobotParent);
#ifndef _WIN32
  sleep(3);
#else
  Sleep(3000);
#endif
  g_scanMobotsActive = 0;
  return NULL;
}

bool liststore_contains(GtkListStore *w, int column, const char* str)
{
  /* Check to see if "str" exists in a liststore */
  GtkTreeIter iter;
  gboolean rc;
  char* modelstr;
  rc = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(w), &iter);
  if(rc == FALSE) {
    return FALSE;
  }
  while(1) {
    gtk_tree_model_get(GTK_TREE_MODEL(w), &iter, column, &modelstr, -1);
    if(!strcmp(modelstr, str)) {
      return TRUE;
    }
    rc = gtk_tree_model_iter_next(GTK_TREE_MODEL(w), &iter);
    if(rc == FALSE) {
      return FALSE;
    }
  }
}

gboolean scanMobotsTimeout(gpointer data)
{
  GtkWidget *w;
  mobotInfo_t* mobotInfo;
  GtkListStore *liststore;
  GtkTreeIter iter;
  int numScanned, i;
  if(g_scanMobotsActive) {
    /* Pulse the progress bar */
    w = GTK_WIDGET(gtk_builder_get_object(g_builder, "progressbar_scanMobots"));
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(w));
    /* Update the liststore */
    /* First, get the scanned mobots data */
    if(Mobot_getChildrenInfo((mobot_t*)g_mobotParent, &mobotInfo, &numScanned)) {
      /* Something bad happened... */
      return false;
    }
    /* For each one of the scanned mobots, see if it is already in the
     * liststore. If not, append it to the liststore. */
    liststore = GTK_LIST_STORE(gtk_builder_get_object(g_builder, "liststore_scannedRobots"));
    for(i = 0; i < numScanned; i++) {
      if(!liststore_contains(GTK_LIST_STORE(liststore), 0, mobotInfo[i].serialID)) {
        gtk_list_store_append(GTK_LIST_STORE(liststore), &iter);
        gtk_list_store_set(GTK_LIST_STORE(liststore), &iter, 0, mobotInfo[i].serialID, 1, mobotInfo[i].zigbeeAddr, -1);
      }
    }
    return true;
  } else {
    /* The thread as ended. Change the text on the progress bar */
    w = GTK_WIDGET(gtk_builder_get_object(g_builder, "progressbar_scanMobots"));
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(w), 0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(w), "Done.");
    return false;
  }
}

void showScanMobotsDialog()
{
  on_button_scanMobotsRefresh_clicked(NULL, NULL);
  gtk_widget_show( GTK_WIDGET(gtk_builder_get_object(g_builder, "dialog_scanMobots")));
}

void hideScanMobotsDialog()
{
  gtk_widget_hide( GTK_WIDGET(gtk_builder_get_object(g_builder, "dialog_scanMobots")));
}

void scanmobots_foreach_callback(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
  /* Get each serial ID and check to see if it is already in the RobotManager */
  char* str;
  gtk_tree_model_get(model, iter, 0, &str, -1);
  if(!g_robotManager->entryExists(str)) {
    g_robotManager->addEntry(str);
    g_robotManager->write();
    refreshConnectDialog();
  }
  free(str);
}

void on_button_scanMobotsAdd_clicked(GtkWidget *w, gpointer data)
{
  /* Go through each selection and add it to the robotmanager */
  GtkTreeView* treeview;
  treeview  = GTK_TREE_VIEW(gtk_builder_get_object(g_builder, "treeview_scanMobots"));
  GtkTreeSelection* selection;
  selection = gtk_tree_view_get_selection(treeview);
  gtk_tree_selection_selected_foreach(selection, scanmobots_foreach_callback, NULL);
}

void on_button_scanMobotsOK_clicked(GtkWidget *w, gpointer data)
{
  on_button_scanMobotsAdd_clicked(w, data);
  gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(g_builder, "dialog_scanMobots")));
}

void on_button_scanMobotsCancel_clicked(GtkWidget *w, gpointer data)
{
  gtk_widget_hide(GTK_WIDGET(gtk_builder_get_object(g_builder, "dialog_scanMobots")));
}

void on_button_scanMobotsRefresh_clicked(GtkWidget *w, gpointer data)
{
  /* If there is no dongle connected, ask to connect a dongle */
  if( (g_mobotParent == NULL) || (((mobot_t*)g_mobotParent)->connected == 0)) {
    askConnectDongle();
    return;
  }

  if(!g_scanMobotsActive) {
    /* If we're not currently scanning for mobots, start a scan */
    /* First, clear the liststore */
    GtkListStore *l = GTK_LIST_STORE(gtk_builder_get_object(g_builder, "liststore_scannedRobots"));
    gtk_list_store_clear(l);
    /* First, add the dongle */
    GtkTreeIter iter;
    gtk_list_store_append(l, &iter);
    gtk_list_store_set(l, &iter, 
        0, ((mobot_t*)g_mobotParent)->serialID, 
        1, ((mobot_t*)g_mobotParent)->zigbeeAddr, -1);
    /* Change the text on the progress bar */
    GtkWidget *w = GTK_WIDGET(gtk_builder_get_object(g_builder, "progressbar_scanMobots"));
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(w), "Scanning...");
    THREAD_T thread;
    g_scanMobotsActive = 1;
    THREAD_CREATE(&thread, scanMobotsThread, NULL);
    g_timeout_add(500, scanMobotsTimeout, NULL);
  }
}
