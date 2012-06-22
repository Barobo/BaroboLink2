#include "RoboMancer.h"
#include "RobotManager.h"
#include "thread_macros.h"
#include <mobot.h>

bool g_isPlaying = false;
bool g_haltPlayFlag = false;
int g_dndInsertIndex = 0;
bool g_dndInsertIndexValid = false;
int g_dndRemoveIndex = 0;
bool g_dndRemoveIndexValid = false;
bool g_dnd = true;

void teachingDialog_refreshRecordedMotions(int currentMotion)
{
  g_dnd = false;
  int i;
  GtkTreeIter iter;
  /* Clear the liststore */
  GtkListStore* ls = GTK_LIST_STORE(gtk_builder_get_object(g_builder, "liststore_recordedMotions"));
  gtk_list_store_clear(ls);
  /* Populate it with motions */
  recordMobot_t* mobot;
  mobot = g_robotManager->getMobot(0);
  if(mobot == NULL) {return;}
  for(i = 0; i < mobot->numMotions; i++) {
    gtk_list_store_append(ls, &iter);
    if(i == currentMotion) {
      gtk_list_store_set(ls, &iter, 
          0, mobot->motions[i]->name,
          1, GTK_STOCK_YES,
          -1);
    } else {
      gtk_list_store_set(ls, &iter, 
          0, mobot->motions[i]->name,
          1, GTK_STOCK_ABOUT,
          -1);
    }
  }
  g_dnd = true;
}

void on_button_recordPos_clicked(GtkWidget*w, gpointer data)
{
  g_robotManager->record();
  teachingDialog_refreshRecordedMotions(-1);
}

void on_button_addDelay_clicked(GtkWidget*w, gpointer data)
{
  /* Get the text from the entry */
  const gchar* str;
  GtkEntry *entry;
  double seconds;
  entry = GTK_ENTRY(gtk_builder_get_object(g_builder, "entry_delaySeconds"));
  str = gtk_entry_get_text(entry);
  if(strlen(str) == 0) {
    return;
  }
  sscanf(str, "%lf", &seconds);
  g_robotManager->addDelay(seconds);
  teachingDialog_refreshRecordedMotions(-1);
}

void on_button_deleteRecordedPos_clicked(GtkWidget*w, gpointer data)
{
  recordMobot_t* m;
  int i, index;
  /* Get the selected position */
  GtkWidget* view =  GTK_WIDGET(gtk_builder_get_object(g_builder, "treeview_recordedMotions"));
  GtkTreeModel* model = GTK_TREE_MODEL(gtk_builder_get_object(g_builder, "liststore_recordedMotions"));
  GtkTreeSelection* selection = gtk_tree_view_get_selection((GTK_TREE_VIEW(view)));
  GList* list = gtk_tree_selection_get_selected_rows(selection, &model);
  if(list == NULL) return;
  gint* paths = gtk_tree_path_get_indices((GtkTreePath*)list->data);
  index = paths[0];
  g_list_foreach(list, (GFunc) gtk_tree_path_free, NULL);
  g_list_free(list);

  m = g_robotManager->getMobot(0);
  for(i = 1; m != NULL; i++) {
    RecordMobot_removeMotion(m, index, true);
    m = g_robotManager->getMobot(i);
  }
  teachingDialog_refreshRecordedMotions(-1);
}

void on_button_clearRecordedPositions_clicked(GtkWidget*w, gpointer data)
{
  recordMobot_t* m;
  int i;
  m = g_robotManager->getMobot(0);
  for(i = 1; m != NULL; i++) {
    RecordMobot_clearAllMotions(m);
    m = g_robotManager->getMobot(i);
  }
  teachingDialog_refreshRecordedMotions(-1);
}

void on_button_saveToProgram_clicked(GtkWidget*w, gpointer data)
{
}

gboolean playTimeout(gpointer userdata)
{
	CRobotManager* robotManager;
	robotManager = g_robotManager;
	static int i=0, j=0, done=0;
  GtkWidget *w;
  gboolean loop;

  /* Get the looped motion check button */
  w = GTK_WIDGET(gtk_builder_get_object(g_builder, "checkbutton_playLooped"));
	//for(i = 0; !done ; i++) {
		for(j = 0; j < robotManager->numConnected(); j++) {
			if(RecordMobot_getMotionType(robotManager->getMobot(j), i) == MOTION_SLEEP) {
				RecordMobot_play(robotManager->getMobot(j), i);
				break;
			}
			if(RecordMobot_play(robotManager->getMobot(j), i)) {
        /* Get the looped checkbutton state */
        loop = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
				if(loop) {
					i = -1;
				}	else {
					done = 1;
				}
			}
		}
		for(j = 0; j < robotManager->numConnected(); j++) {
		  Mobot_moveWait((mobot_t*)robotManager->getMobot(j));
		}
    if(g_haltPlayFlag) {
      g_haltPlayFlag = 0;

      for(j = 0; j < robotManager->numConnected(); j++) {
        Mobot_stop((mobot_t*)robotManager->getMobot(j));
      }
      g_isPlaying = false;
      w = GTK_WIDGET(gtk_builder_get_object(g_builder, "button_playRecorded"));
      gtk_widget_set_sensitive(w, TRUE);
      teachingDialog_refreshRecordedMotions(-1);
      done = 0; i = 0; j = 0;
      return false;
    }
    i++;
    if(!done) {
      /* highlight the next motion */
      teachingDialog_refreshRecordedMotions(i);
      return true;
    }
	//}
	for(j = 0; j < robotManager->numConnected(); j++) {
		Mobot_stop((mobot_t*)robotManager->getMobot(j));
	}
  g_isPlaying = false;
  w = GTK_WIDGET(gtk_builder_get_object(g_builder, "button_playRecorded"));
  gtk_widget_set_sensitive(w, TRUE);
	teachingDialog_refreshRecordedMotions(-1);
  done = i = j = 0;
	return false;
}

void on_button_playRecorded_clicked(GtkWidget*button, gpointer data) 
{
  GtkWidget *w;
  /* Disable the play button */
  w = GTK_WIDGET(gtk_builder_get_object(g_builder, "button_playRecorded"));
  gtk_widget_set_sensitive(w, FALSE);
  teachingDialog_refreshRecordedMotions(0);
  /* Start the play thread */
  g_haltPlayFlag = false;
  g_idle_add(playTimeout, NULL);
}

void on_button_stopRecorded_clicked(GtkWidget*w, gpointer data)
{
  g_haltPlayFlag = true;
}

gboolean refreshTimeout(gpointer userdata)
{
  teachingDialog_refreshRecordedMotions(-1);
  return false;
}

void on_mobotButtonPress(void* data, int button, int buttonDown)
{
  /* Button A: Record Motion 
   * Button B: Playback / Stop
   * Both Buttons: Reset motions */
  mobot_t* mobot = (mobot_t*)data;
  static unsigned int lastState = 0;
  unsigned int newState;
  static bool debounce = false;
  int i;

  /* Calculate the new state */
  if(buttonDown) {
    Mobot_blinkLED(mobot, 0.1, 2);
    newState = lastState | (1<<button);
  } else {
    newState = lastState & ~(1<<button);
  }
  if(debounce) {
    /* Must wait until all buttons are up */
    if(newState == 0) {
      debounce = false;
      lastState = newState;
      return;
    } else {
      lastState = newState;
      return;
    }
  }

  /* If a button release event is detected, use the last state to determine the action */
  if(buttonDown == 0) {
    if(lastState == 0x01) {
      /* Button A press/release */
      for(i = 0; i < g_robotManager->numConnected(); i++) {
		    RecordMobot_record(g_robotManager->getMobot(i));
	    }
	    //teachingDialog_refreshRecordedMotions(-1);
      g_idle_add(refreshTimeout, NULL);
    }
    if(lastState == 0x02) {
      /* Button B press/release */
      /* If it is not playing play. Otherwise, stop. */
      if(g_isPlaying) {
        g_haltPlayFlag = 1;
      } else {
        on_button_playRecorded_clicked(NULL, NULL);
      }
    }
    if(lastState == 0x03) {
      /* Buttons A/B Pressed, one released */
      debounce = true;
      /* Clear all recorded motions */
      recordMobot_t* m;
      int i;
      m = g_robotManager->getMobot(0);
      for(i = 1; m != NULL; i++) {
        RecordMobot_clearAllMotions(m);
        m = g_robotManager->getMobot(i);
      }
      //teachingDialog_refreshRecordedMotions(-1);
      g_idle_add(refreshTimeout, NULL);
    }
  }
  lastState = newState;
}

void on_notebook1_switch_page(GtkNotebook* notebook, gpointer page, guint page_num, gpointer userdata)
{
  int i;
  mobot_t* mobot;
  /* If the teaching dialog gets selected, we should initialize all connected
   * Mobots to use our custom button handler. */
  if(page_num == 2) {
    /* Enable all button handlers */
    for(i = 0; i < g_robotManager->numConnected(); i++) {
      mobot = (mobot_t*)g_robotManager->getMobot(i);
      Mobot_enableButtonCallback(mobot, mobot, on_mobotButtonPress);
    }
  } else {
    /* Disable all button callbacks */
    for(i = 0; i < g_robotManager->numConnected(); i++) {
      mobot = (mobot_t*)g_robotManager->getMobot(i);
      Mobot_disableButtonCallback(mobot);
    }
  }
}

void on_liststore_recordedMotions_rows_reordered(
    GtkTreeModel* model, 
    GtkTreePath* path,
    GtkTreeIter* iter,
    gpointer _new_order,
    gpointer user_data)
{
  int *new_order = (int*)_new_order;
  int i;
  for(i = 0; i < g_robotManager->getMobot(0)->numMotions; i++) {
    printf("%d : %d\n", i, new_order[i]);
  }
}

void on_cellrenderertext_recordedMotionName_edited(
    GtkCellRendererText* renderer,
    gchar* path,
    gchar* new_text,
    gpointer user_data)
{
  //printf("Edited cell: %s\n", path);
  int index;
  sscanf(path, "%d", &index);
  recordMobot_t* m;
  m = g_robotManager->getMobot(0);
  for(int i = 1; m != NULL; i++) {
    RecordMobot_setMotionName(m, index, new_text);
    m = g_robotManager->getMobot(i);
  }
  teachingDialog_refreshRecordedMotions(-1);
}

void on_liststore_recordedMotions_row_deleted(
    GtkTreeModel* model,
    GtkTreePath* path,
    gpointer user_data)
{
  gint* indices;
  int depth;
  if(g_dnd) {
    indices = gtk_tree_path_get_indices_with_depth(path, &depth);
    g_dndRemoveIndex = indices[0];
    g_dndRemoveIndexValid = true;
    //printf("Row Deleted: %d\n", indices[0]);
    printf("%d %d\n", g_dndRemoveIndexValid, g_dndInsertIndexValid);

    if(g_dndRemoveIndexValid && g_dndInsertIndexValid) {
      g_dndRemoveIndexValid = false;
      g_dndInsertIndexValid = false;
      if(g_dndInsertIndex < g_dndRemoveIndex) {
        g_dndRemoveIndex--;
      } else {
        g_dndInsertIndex--;
      }
      recordMobot_t *m;
      m = g_robotManager->getMobot(0);
      for(int i = 1; m != NULL; i++) {
        RecordMobot_moveMotion(m, g_dndRemoveIndex, g_dndInsertIndex);
        m = g_robotManager->getMobot(i);
      }
      teachingDialog_refreshRecordedMotions(-1);
    }
  }
}

void on_liststore_recordedMotions_row_inserted(
    GtkTreeModel* model,
    GtkTreePath* path,
    GtkTreeIter* iter,
    gpointer user_data)
{
  gint* indices;
  int depth;
  if(g_dnd) {
    indices = gtk_tree_path_get_indices_with_depth(path, &depth);
    //printf("Row Inserted: %d\n", indices[0]);
    g_dndInsertIndex = indices[0];
    g_dndInsertIndexValid = true;

    printf("%d %d\n", g_dndRemoveIndexValid, g_dndInsertIndexValid);
    if(g_dndRemoveIndexValid && g_dndInsertIndexValid) {
      g_dndRemoveIndexValid = false;
      g_dndInsertIndexValid = false;
      recordMobot_t *m;
      m = g_robotManager->getMobot(0);
      for(int i = 1; m != NULL; i++) {
        RecordMobot_moveMotion(m, g_dndRemoveIndex, g_dndInsertIndex);
        m = g_robotManager->getMobot(i);
      }
      teachingDialog_refreshRecordedMotions(-1);
    }
  }
}
    