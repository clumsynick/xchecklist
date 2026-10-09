#ifndef CHECKLIST_UI_MODEL_H
#define CHECKLIST_UI_MODEL_H

#include "interface.h"

#include <string>
#include <unordered_set>
#include <vector>

struct checklist_ui_item_t {
  std::string text;
  std::string suffix;
  bool copilot_controlled;
  bool info_only;
  bool item_void;
  bool checked;
};

class checklist_ui_model {
public:
  void set_checklist(unsigned int size, const char *title,
                     const checklist_item_desc_t items[], int index,
                     int checklist_count);
  void set_checklist_names(const std::vector<std::string>& names,
                           const std::vector<int>& indexes);
  void set_checked(int item, bool checked);
  void set_active(int item);
  void reset_completions();

  const std::string& title() const { return title_; }
  const std::vector<checklist_ui_item_t>& items() const { return items_; }
  const std::vector<std::string>& checklist_names() const { return checklist_names_; }
  const std::vector<int>& checklist_indexes() const { return checklist_indexes_; }
  int active_item() const { return active_item_; }
  int checklist_index() const { return checklist_index_; }
  int checklist_count() const { return checklist_count_; }
  int completed_items() const;
  int actionable_items() const;
  bool checklist_completed(int index) const;

private:
  std::string title_;
  std::vector<checklist_ui_item_t> items_;
  std::vector<std::string> checklist_names_;
  std::vector<int> checklist_indexes_;
  int active_item_ = -1;
  int checklist_index_ = 0;
  int checklist_count_ = 0;
  std::unordered_set<int> completed_checklists_;
};

extern checklist_ui_model g_checklist_ui_model;

#endif
