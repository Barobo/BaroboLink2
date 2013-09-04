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

#include <gtk/gtk.h>
#include <string.h>
#define PLAT_GTK 1
#define GTK
#include "BaroboLink.h"
#include "RobotManager.h"
#ifdef __MACH__
#include <sys/types.h>
#include <unistd.h>
#include <gtk-mac-integration.h>
#define MAX_PATH 4096
#endif
#include <sys/stat.h>
#include "thread_macros.h"
#ifdef _MSYS
#include <windows.h>

#endif

GtkBuilder *g_builder;
GtkWidget *g_window;
GtkWidget *g_scieditor;
GtkWidget *g_scieditor_ext;

CRobotManager *g_robotManager;

char *g_interfaceFiles[512] = {
  "interface/interface.glade",
  "interface.glade",
  "../share/BaroboLink/interface.glade",
  "/usr/share/BaroboLink/interface.glade",
  NULL,
  NULL
};

char *g_interfaceDir;

int main(int argc, char* argv[])
{
  GError *error = NULL;

#ifdef _WIN32
  /* Make sure there isn't another instance of BaroboLink running by checking
   * for the existance of a named mutex. */
  HANDLE hMutex;
  hMutex = CreateMutex(NULL, TRUE, TEXT("Global\\RoboMancerMutex"));
  DWORD dwerror = GetLastError();
#endif
 
  gtk_init(&argc, &argv);

  /* Create the GTK Builder */
  g_builder = gtk_builder_new();

#ifdef _WIN32
  if(dwerror == ERROR_ALREADY_EXISTS) {
    GtkWidget* d = gtk_message_dialog_new(
        GTK_WINDOW(gtk_builder_get_object(g_builder, "window1")),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_OK,
        "Another instance of BaroboLink is already running. Please terminate the other process and try again.");
    int rc = gtk_dialog_run(GTK_DIALOG(d));
    exit(0);
  }
#endif
#ifdef __MACH__
  char *datadir = getenv("XDG_DATA_DIRS");
  if(datadir != NULL) {
    g_interfaceFiles[3] = (char*)malloc(sizeof(char)*512);
    sprintf(g_interfaceFiles[3], "%s/BaroboLink/interface.glade", datadir);
  }
  g_interfaceDir = strdup(datadir);
#elif defined _WIN32
  g_interfaceDir = strdup("interface");
#else
  g_interfaceDir = strdup("/usr/share/BaroboLink");
#endif

  /* Load the UI */
  /* Find ther interface file */
  struct stat s;
  int err;
  int i;
  for(i = 0; g_interfaceFiles[i] != NULL; i++) {
    err = stat(g_interfaceFiles[i], &s);
    if(err == 0) {
      if( ! gtk_builder_add_from_file(g_builder, g_interfaceFiles[i], &error) )
      {
        g_warning("%s", error->message);
        //g_free(error);
        return -1;
      } else {
        break;
      }
    }
  }

  if(g_interfaceFiles[i] == NULL) {
    /* Could not find the interface file */
    g_warning("Could not find interface file.");
    return -1;
  }
  /* Initialize the subsystem */
  initialize();

  /* Get the main window */
  g_window = GTK_WIDGET( gtk_builder_get_object(g_builder, "window1"));
  /* Connect signals */
  gtk_builder_connect_signals(g_builder, NULL);

#ifdef __MACH__
  //g_signal_connect(GtkOSXMacmenu, "NSApplicationBlockTermination",
      //G_CALLBACK(app_should_quit_cb), NULL);
  GtkWidget* quititem = GTK_WIDGET(gtk_builder_get_object(g_builder, "imagemenuitem5"));
  //gtk_mac_menu_set_quit_menu_item(GTK_MENU_ITEM(quititem));
#endif

  /* Hide the Program dialog */
  GtkWidget* w = GTK_WIDGET(gtk_builder_get_object(g_builder, "notebook1"));
  gtk_notebook_remove_page(GTK_NOTEBOOK(w), 3);

  /* Show the window */
  gtk_widget_show(g_window);
  gtk_main();
  return 0;
}

void initialize()
{
  g_mobotParent = NULL;
  g_robotManager = new CRobotManager();
  /* Read the configuration file */
  g_robotManager->read( Mobot_getConfigFilePath() );

  refreshConnectDialog();
  //g_timeout_add(1000, connectDialogPulse, NULL);

  g_notebookRoot = GTK_NOTEBOOK(gtk_builder_get_object(g_builder, "notebook_root"));
  g_reflashConnectSpinner = GTK_SPINNER(gtk_builder_get_object(g_builder, "spinner_reflashConnect"));
  initControlDialog();
  initScanMobotsDialog();
  initializeComms();

  /* Try to connect to a DOF dongle if possible */
  const char *dongle;
  int i, rc;
  GtkLabel* l = GTK_LABEL(gtk_builder_get_object(g_builder, "label_connectDongleCurrentPort"));
  for(
      i = 0, dongle = g_robotManager->getDongle(i); 
      dongle != NULL; 
      i++, dongle = g_robotManager->getDongle(i)
     ) 
  {
    g_mobotParent = (recordMobot_t*)malloc(sizeof(recordMobot_t));
    RecordMobot_init(g_mobotParent, "DONGLE");
    rc = Mobot_connectWithTTY((mobot_t*)g_mobotParent, dongle);
    if(rc == 0) {
      Mobot_setDongleMobot((mobot_t*)g_mobotParent);
      gtk_label_set_text(l, dongle);
      break;
    } 
  }
  if(rc) {
    gtk_label_set_text(l, "Not connected");
  }
}

int getIterModelFromTreeSelection(GtkTreeView *treeView, GtkTreeModel **model, GtkTreeIter *iter)
{
  GtkTreeSelection *treeSelection;

  treeSelection = gtk_tree_view_get_selection( treeView );
  gtk_tree_selection_set_mode(treeSelection, GTK_SELECTION_BROWSE);

  bool success =
    gtk_tree_selection_get_selected( treeSelection, model, iter );
  if(!success) {
    return -1;
  }
  return 0;
}

void on_menuitem_help_activate(GtkWidget *widget, gpointer data)
{
#ifdef _MSYS
  /* Get the install path of BaroboLink from the registry */
  DWORD size;
  char path[1024];
  HKEY key;
  RegOpenKeyEx(
      HKEY_LOCAL_MACHINE,
      "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\BaroboLink.exe",
      0,
      KEY_QUERY_VALUE,
      &key);

  RegQueryValueEx(
      key,
      "PATH",
      NULL,
      NULL,
      (LPBYTE)path,
      &size);
  path[size] = '\0';

  strcat(path, "\\docs\\index.html");

  ShellExecute(
      NULL,
      "open",
      path,
      NULL,
      NULL,
      SW_SHOW);
#else
  
#endif
}

void on_menuitem_demos_activate(GtkWidget *widget, gpointer data)
{
#ifdef _MSYS
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  memset(&si, 0, sizeof(STARTUPINFO));
  memset(&pi, 0, sizeof(PROCESS_INFORMATION));
  CreateProcess(
	"C:\\ch\\bin\\chide.exe",
	"-d C:\\ch\\package\\chmobot\\demos",
	NULL,
	NULL,
	FALSE,
	NORMAL_PRIORITY_CLASS,
	NULL,
	NULL,
	&si,
	&pi);
	
/*
  ShellExecute(
      NULL,
      "open",
      "C:\\chide",
      "-d C:\\Ch\\package\\chmobot\\demos",
      NULL,
      0);
*/
#endif
}

#define Q(x) QUOTE(x)
#define QUOTE(x) #x

void on_imagemenuitem_about_activate(GtkWidget *widget, gpointer data)
{
  /* Find the about dialog and show it */
  GtkWidget *w;
  w = GTK_WIDGET(gtk_builder_get_object(g_builder, "aboutdialog"));
  gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(w), Q(BAROBOLINK_VERSION));
  gtk_dialog_run(GTK_DIALOG(w));
}

void on_menuitem_installLinkbotDriver_activate(GtkWidget *widget, gpointer data)
{
  /* Get the install path of BaroboLink from the registry */
#ifdef _MSYS
  DWORD size;
  char path[1024];
  HKEY key;
  RegOpenKeyEx(
      HKEY_LOCAL_MACHINE,
      "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\BaroboLink.exe",
      0,
      KEY_QUERY_VALUE,
      &key);

  RegQueryValueEx(
      key,
      "PATH",
      NULL,
      NULL,
      (LPBYTE)path,
      &size);
  path[size] = '\0';
  strcat(path, "\\Drivers");
  ShellExecute(NULL, "open", path, NULL, NULL, SW_SHOWDEFAULT);
#endif

#if 0
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  memset(&si, 0, sizeof(STARTUPINFO));
  memset(&pi, 0, sizeof(PROCESS_INFORMATION));
  /*
  CreateProcess(
	path,
	"",
	NULL,
	NULL,
	FALSE,
	0x0200,
	NULL,
	NULL,
	&si,
	&pi);
  */
  system(path);
#endif
}

void on_aboutdialog_activate_link(GtkAboutDialog *label, gchar* uri, gpointer data)
{
#ifdef _MSYS
  ShellExecuteA(
      NULL,
      "open",
      uri,
      NULL,
      NULL,
      0);
#elif defined (__MACH__) 
  char command[MAX_PATH];
  sprintf(command, "open %s", uri);
  system(command); 
#endif
}

void on_aboutdialog_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
  gtk_widget_hide(GTK_WIDGET(dialog));
}

void on_aboutdialog_close(GtkDialog *dialog, gpointer user_data)
{
  gtk_widget_hide(GTK_WIDGET(dialog));
}

volatile gboolean g_disconnectThreadDone = 0;
void* disconnectThread(void* arg)
{
  g_robotManager->disconnectAll();
  g_disconnectThreadDone = TRUE;
}

gboolean exitTimeout(gpointer data)
{
  static int i = 0;
  if((i >= 6) || g_disconnectThreadDone) {
    gtk_main_quit();
    return FALSE;
  } else {
    i++;
    return TRUE;
  }
}

gboolean on_window1_delete_event(GtkWidget *w)
{
  /* First, disable the active robot, if there is one */
  MUTEX_LOCK(&g_activeMobotLock);
  g_activeMobot = NULL;
  MUTEX_UNLOCK(&g_activeMobotLock);
  /* Disconnect from all connected robots */
  THREAD_T thread;
  g_disconnectThreadDone = 0;
  THREAD_CREATE(&thread, disconnectThread, NULL);
  GtkWidget *d = gtk_message_dialog_new(
      GTK_WINDOW(gtk_builder_get_object(g_builder, "window1")),
      GTK_DIALOG_MODAL,
      GTK_MESSAGE_INFO,
      GTK_BUTTONS_NONE,
      "BaroboLink shutting down..."
      );
  gtk_window_set_decorated(
      GTK_WINDOW(d),
      false);
  gtk_widget_show_all(d);
  g_timeout_add(500, exitTimeout, NULL);
}

double normalizeAngleRad(double radians)
{
  while(radians > M_PI) {
    radians -= 2*M_PI;
  }
  while(radians < -M_PI) {
    radians += 2*M_PI;
  }
  return radians;
}
