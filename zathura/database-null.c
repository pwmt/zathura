/* SPDX-License-Identifier: Zlib */

#include <girara/datastructures.h>
#include <girara/input-history.h>

#include "database-null.h"
#include "utils.h"

static bool add_bookmark(zathura_database_t* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(file),
                         zathura_bookmark_t* GIRARA_UNUSED(bookmark)) {
  return true;
}

static bool remove_bookmark(zathura_database_t* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(file),
                            const char* GIRARA_UNUSED(id)) {
  return true;
}

static girara_list_t* load_list(zathura_database_t* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(file)) {
  return girara_list_new();
}

static bool save_list(zathura_database_t* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(file),
                      girara_list_t* GIRARA_UNUSED(jumplist)) {
  return true;
}

static bool set_fileinfo(zathura_database_t* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(file),
                         const uint8_t* GIRARA_UNUSED(hash_sha256), zathura_fileinfo_t* GIRARA_UNUSED(file_info)) {
  return true;
}

static bool get_fileinfo(zathura_database_t* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(file),
                         const uint8_t* GIRARA_UNUSED(hash_sha256), zathura_fileinfo_t* GIRARA_UNUSED(file_info)) {
  return false;
}

static void io_append(GiraraInputHistoryIO* GIRARA_UNUSED(db), const char* GIRARA_UNUSED(input)) {}

static girara_list_t* io_read(GiraraInputHistoryIO* GIRARA_UNUSED(db)) {
  return girara_list_new();
}

static girara_list_t* get_recent_files(zathura_database_t* GIRARA_UNUSED(db), int GIRARA_UNUSED(max),
                                       const char* GIRARA_UNUSED(basepath)) {
  return girara_list_new();
}

static void db_interface_init(ZathuraDatabaseInterface* iface) {
  /* initialize interface */
  iface->add_bookmark     = add_bookmark;
  iface->remove_bookmark  = remove_bookmark;
  iface->load_bookmarks   = load_list;
  iface->load_jumplist    = load_list;
  iface->save_jumplist    = save_list;
  iface->set_fileinfo     = set_fileinfo;
  iface->get_fileinfo     = get_fileinfo;
  iface->get_recent_files = get_recent_files;
  iface->load_quickmarks  = load_list;
  iface->save_quickmarks  = save_list;
}

static void io_interface_init(GiraraInputHistoryIOInterface* iface) {
  /* initialize interface */
  iface->append = io_append;
  iface->read   = io_read;
}

static void zathura_nulldatabase_class_init(ZathuraNullDatabaseClass* GIRARA_UNUSED(class)) {}

static void zathura_nulldatabase_init(ZathuraNullDatabase* GIRARA_UNUSED(db)) {}

G_DEFINE_TYPE_WITH_CODE(ZathuraNullDatabase, zathura_nulldatabase, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(ZATHURA_TYPE_DATABASE, db_interface_init)
                            G_IMPLEMENT_INTERFACE(GIRARA_TYPE_INPUT_HISTORY_IO, io_interface_init))

zathura_database_t* zathura_nulldatabase_new(void) {
  zathura_database_t* db = g_object_new(ZATHURA_TYPE_NULLDATABASE, NULL);
  return db;
}
