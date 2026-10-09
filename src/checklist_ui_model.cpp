#include "checklist_ui_model.h"

#include <algorithm>

checklist_ui_model g_checklist_ui_model;

void checklist_ui_model::set_checklist(unsigned int size, const char *title,
                                       const checklist_item_desc_t items[],
                                       int index, int checklist_count)
{
  title_ = title ? title : "Checklist";
  checklist_index_ = index;
  checklist_count_ = checklist_count;
  active_item_ = -1;
  items_.clear();
  items_.reserve(size);

  for(unsigned int i = 0; i < size; ++i){
    checklist_ui_item_t item;
    item.text = items[i].text ? items[i].text : "";
    item.suffix = items[i].suffix ? items[i].suffix : "";
    item.copilot_controlled = items[i].copilot_controlled;
    item.info_only = items[i].info_only;
    item.item_void = items[i].item_void;
    item.checked = false;
    items_.push_back(item);
  }
}

void checklist_ui_model::set_checklist_names(
  const std::vector<std::string>& names, const std::vector<int>& indexes)
{
  checklist_names_ = names;
  checklist_indexes_ = indexes;
}

void checklist_ui_model::set_checked(int item, bool checked)
{
  if(item >= 0 && static_cast<size_t>(item) < items_.size()){
    items_[item].checked = checked;
    const int actionable = actionable_items();
    if(actionable > 0 && completed_items() == actionable){
      completed_checklists_.insert(checklist_index_);
    }else{
      completed_checklists_.erase(checklist_index_);
    }
  }
}

void checklist_ui_model::set_active(int item)
{
  active_item_ = item;
}

void checklist_ui_model::reset_completions()
{
  completed_checklists_.clear();
}

bool checklist_ui_model::checklist_completed(int index) const
{
  return completed_checklists_.count(index) != 0;
}

int checklist_ui_model::completed_items() const
{
  return static_cast<int>(std::count_if(items_.begin(), items_.end(),
    [](const checklist_ui_item_t& item){ return !item.item_void && item.checked; }));
}

int checklist_ui_model::actionable_items() const
{
  return static_cast<int>(std::count_if(items_.begin(), items_.end(),
    [](const checklist_ui_item_t& item){ return !item.item_void; }));
}
