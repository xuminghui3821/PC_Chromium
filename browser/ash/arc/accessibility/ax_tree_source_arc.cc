// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"

#include <stack>
#include <string>
#include <utility>

#include "ash/public/cpp/external_arc/message_center/arc_notification_surface.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_node_info_data_wrapper.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_window_info_data_wrapper.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/ash/arc/accessibility/auto_complete_handler.h"
#include "chrome/browser/ash/arc/accessibility/drawer_layout_handler.h"
#include "components/exo/input_method_surface.h"
#include "components/exo/wm_helper.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/common/extension_messages.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace arc {

using AXEventData = mojom::AccessibilityEventData;
using AXEventType = mojom::AccessibilityEventType;
using AXIntProperty = mojom::AccessibilityIntProperty;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXWindowBooleanProperty = mojom::AccessibilityWindowBooleanProperty;
using AXWindowInfoData = mojom::AccessibilityWindowInfoData;
using AXWindowIntListProperty = mojom::AccessibilityWindowIntListProperty;

AXTreeSourceArc::AXTreeSourceArc(Delegate* delegate)
    : current_tree_serializer_(new AXTreeArcSerializer(this)),
      is_notification_(false),
      is_input_method_window_(false),
      delegate_(delegate) {}

AXTreeSourceArc::~AXTreeSourceArc() {
  Reset();
}

void AXTreeSourceArc::NotifyAccessibilityEvent(AXEventData* event_data) {
  root_id_.reset();
  DCHECK(event_data);

  NotifyAccessibilityEventInternal(*event_data);

  // Clear maps in order to prevent invalid access from dead pointers.
  tree_map_.clear();
  parent_map_.clear();
  computed_bounds_.clear();
}

void AXTreeSourceArc::NotifyActionResult(const ui::AXActionData& data,
                                         bool result) {
  GetAutomationEventRouter()->DispatchActionResult(data, result);
}

void AXTreeSourceArc::NotifyGetTextLocationDataResult(
    const ui::AXActionData& data,
    const base::Optional<gfx::Rect>& rect) {
  GetAutomationEventRouter()->DispatchGetTextLocationDataResult(data, rect);
}

bool AXTreeSourceArc::UseFullFocusMode() const {
  return delegate_->UseFullFocusMode();
}

void AXTreeSourceArc::InvalidateTree() {
  current_tree_serializer_->Reset();
}

bool AXTreeSourceArc::IsRootOfNodeTree(int32_t id) const {
  const auto& node_it = tree_map_.find(id);
  if (node_it == tree_map_.end())
    return false;

  if (!node_it->second->IsNode())
    return false;

  const auto& parent_it = parent_map_.find(id);
  if (parent_it == parent_map_.end())
    return true;

  const auto& parent_tree_it = tree_map_.find(parent_it->second);
  CHECK(parent_tree_it != tree_map_.end());
  return !parent_tree_it->second->IsNode();
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetFirstImportantAncestor(
    AccessibilityInfoDataWrapper* info_data) const {
  AccessibilityInfoDataWrapper* parent = GetParent(info_data);
  while (parent && parent->IsNode() && !parent->IsImportantInAndroid()) {
    parent = GetParent(parent);
  }
  return parent;
}

bool AXTreeSourceArc::GetTreeData(ui::AXTreeData* data) const {
  data->tree_id = ax_tree_id();
  if (android_focused_id_.has_value())
    data->focus_id = *android_focused_id_;
  return true;
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetRoot() const {
  return root_id_.has_value() ? GetFromId(*root_id_) : nullptr;
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetFromId(int32_t id) const {
  auto it = tree_map_.find(id);
  if (it == tree_map_.end())
    return nullptr;
  return it->second.get();
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetParent(
    AccessibilityInfoDataWrapper* info_data) const {
  if (!info_data)
    return nullptr;
  auto it = parent_map_.find(info_data->GetId());
  if (it != parent_map_.end())
    return GetFromId(it->second);
  return nullptr;
}

void AXTreeSourceArc::SerializeNode(AccessibilityInfoDataWrapper* info_data,
                                    ui::AXNodeData* out_data) const {
  if (!info_data)
    return;

  info_data->Serialize(out_data);

  const auto& itr = hooks_.find(info_data->GetId());
  if (itr != hooks_.end())
    itr->second->PostSerializeNode(out_data);
}

aura::Window* AXTreeSourceArc::GetWindow() const {
  if (is_notification_) {
    if (!notification_key_.has_value())
      return nullptr;

    auto* surface_manager = ash::ArcNotificationSurfaceManager::Get();
    if (!surface_manager)
      return nullptr;

    ash::ArcNotificationSurface* surface =
        surface_manager->GetArcSurface(notification_key_.value());
    if (!surface)
      return nullptr;

    return surface->GetWindow();
  } else if (is_input_method_window_) {
    exo::InputMethodSurface* input_method_surface =
        exo::InputMethodSurface::GetInputMethodSurface();
    if (!input_method_surface)
      return nullptr;

    return input_method_surface->host_window();
  } else if (exo::WMHelper::HasInstance()) {
    // TODO(b/173658482): Support non-active windows.
    return FindArcWindow(exo::WMHelper::GetInstance()->GetFocusedWindow());
  }
  return nullptr;
}

void AXTreeSourceArc::NotifyAccessibilityEventInternal(
    const AXEventData& event_data) {
  if (window_id_ != event_data.window_id) {
    android_focused_id_.reset();
    window_id_ = event_data.window_id;
  }
  is_notification_ = event_data.notification_key.has_value();
  if (is_notification_)
    notification_key_ = event_data.notification_key;
  is_input_method_window_ = event_data.is_input_method_window;

  // Prepare the wrapper objects of mojom data from Android.
  CHECK(event_data.window_data);
  root_id_ = event_data.window_data->at(0)->window_id;
  for (size_t i = 0; i < event_data.window_data->size(); ++i) {
    int32_t window_id = event_data.window_data->at(i)->window_id;
    int32_t root_node_id = event_data.window_data->at(i)->root_node_id;
    AXWindowInfoData* window = event_data.window_data->at(i).get();
    if (root_node_id)
      parent_map_[root_node_id] = window_id;

    tree_map_[window_id] =
        std::make_unique<AccessibilityWindowInfoDataWrapper>(this, window);

    std::vector<int32_t> children;
    if (GetProperty(window->int_list_properties,
                    AXWindowIntListProperty::CHILD_WINDOW_IDS, &children)) {
      for (const int32_t child : children) {
        DCHECK(child != root_id_);
        parent_map_[child] = window_id;
      }
    }
  }

  for (size_t i = 0; i < event_data.node_data.size(); ++i) {
    int32_t node_id = event_data.node_data[i]->id;
    AXNodeInfoData* node = event_data.node_data[i].get();
    tree_map_[node_id] =
        std::make_unique<AccessibilityNodeInfoDataWrapper>(this, node);

    std::vector<int32_t> children;
    if (GetProperty(event_data.node_data[i].get()->int_list_properties,
                    AXIntListProperty::CHILD_NODE_IDS, &children)) {
      for (const int32_t child : children)
        parent_map_[child] = node_id;
    }
  }

  // Compute each node's bounds, based on its descendants.
  // Assuming |nodeData| is in pre-order, compute cached bounds in post-order to
  // avoid an O(n^2) amount of work as the computed bounds uses descendant
  // bounds.
  for (int i = event_data.node_data.size() - 1; i >= 0; --i) {
    int32_t id = event_data.node_data[i]->id;
    computed_bounds_[id] = ComputeEnclosingBounds(tree_map_[id].get());
  }
  for (int i = event_data.window_data->size() - 1; i >= 0; --i) {
    int32_t id = event_data.window_data->at(i)->window_id;
    computed_bounds_[id] = ComputeEnclosingBounds(tree_map_[id].get());
  }

  if (!UpdateAndroidFocusedId(event_data)) {
    // Exit this function if the focused node doesn't exist nor isn't visible.
    return;
  }

  std::vector<int32_t> update_ids = ProcessHooksOnEvent(event_data);

  // Prep the event and send it to automation.
  AccessibilityInfoDataWrapper* focused_node =
      android_focused_id_.has_value() ? GetFromId(*android_focused_id_)
                                      : nullptr;
  std::vector<ui::AXEvent> events;
  ui::AXEvent event;
  event.event_type = ToAXEvent(
      event_data.event_type,
      GetPropertyOrNull(
          event_data.int_list_properties,
          arc::mojom::AccessibilityEventIntListProperty::CONTENT_CHANGE_TYPES),
      GetFromId(event_data.source_id), focused_node);
  event.id = event_data.source_id;

  if (HasProperty(event_data.int_properties,
                  arc::mojom::AccessibilityEventIntProperty::ACTION)) {
    event.event_from = ax::mojom::EventFrom::kAction;
  }

  events.push_back(std::move(event));

  HandleLiveRegions(&events);

  // Force the tree, to update, so unignored fields get updated.
  // On event type of WINDOW_STATE_CHANGED, update the entire tree so that
  // window location is correctly calculated.
  int32_t node_id_to_clear =
      (event_data.event_type == AXEventType::WINDOW_STATE_CHANGED)
          ? *root_id_
          : event_data.source_id;

  update_ids.push_back(node_id_to_clear);

  std::vector<ui::AXTreeUpdate> updates;
  for (const int32_t update_root : update_ids) {
    ui::AXTreeUpdate update;
    update.node_id_to_clear = update_root;
    current_tree_serializer_->InvalidateSubtree(GetFromId(update_root));
    current_tree_serializer_->SerializeChanges(GetFromId(update_root), &update);

    updates.push_back(std::move(update));
  }

  GetAutomationEventRouter()->DispatchAccessibilityEvents(
      ax_tree_id(), std::move(updates), gfx::Point(), std::move(events));
}

extensions::AutomationEventRouterInterface*
AXTreeSourceArc::GetAutomationEventRouter() const {
  return extensions::AutomationEventRouter::GetInstance();
}

gfx::Rect AXTreeSourceArc::ComputeEnclosingBounds(
    AccessibilityInfoDataWrapper* info_data) const {
  DCHECK(info_data);
  gfx::Rect computed_bounds;
  // Exit early if the node or window is invisible.
  if (!info_data->IsVisibleToUser())
    return computed_bounds;

  ComputeEnclosingBoundsInternal(info_data, &computed_bounds);
  return computed_bounds;
}

void AXTreeSourceArc::ComputeEnclosingBoundsInternal(
    AccessibilityInfoDataWrapper* info_data,
    gfx::Rect* computed_bounds) const {
  DCHECK(computed_bounds);
  auto cached_bounds = computed_bounds_.find(info_data->GetId());
  if (cached_bounds != computed_bounds_.end()) {
    computed_bounds->Union(cached_bounds->second);
    return;
  }

  if (!info_data->IsVisibleToUser())
    return;
  if (info_data->IsFocusableInFullFocusMode()) {
    // Only consider nodes that can possibly be accessibility focused.
    computed_bounds->Union(info_data->GetBounds());
    return;
  }
  std::vector<AccessibilityInfoDataWrapper*> children;
  info_data->GetChildren(&children);
  if (children.empty())
    return;
  for (AccessibilityInfoDataWrapper* child : children)
    ComputeEnclosingBoundsInternal(child, computed_bounds);
  return;
}

AccessibilityInfoDataWrapper*
AXTreeSourceArc::FindFirstFocusableNodeInFullFocusMode(
    AccessibilityInfoDataWrapper* info_data) const {
  if (!IsValid(info_data))
    return nullptr;

  if (info_data->IsVisibleToUser() && info_data->IsFocusableInFullFocusMode())
    return info_data;

  std::vector<AccessibilityInfoDataWrapper*> children;
  GetChildren(info_data, &children);
  for (AccessibilityInfoDataWrapper* child : children) {
    AccessibilityInfoDataWrapper* candidate =
        FindFirstFocusableNodeInFullFocusMode(child);
    if (candidate)
      return candidate;
  }

  return nullptr;
}

bool AXTreeSourceArc::UpdateAndroidFocusedId(const AXEventData& event_data) {
  AccessibilityInfoDataWrapper* source_node = GetFromId(event_data.source_id);
  if (source_node) {
    AccessibilityInfoDataWrapper* source_window =
        GetFromId(source_node->GetWindowId());
    if (!source_window ||
        !GetBooleanProperty(source_window->GetWindow(),
                            AXWindowBooleanProperty::FOCUSED)) {
      // Don't update focus in this task for events from non-focused window.
      return true;
    }
  }

  // TODO(hirokisato): Handle CLEAR_ACCESSIBILITY_FOCUS event.
  if (event_data.event_type == AXEventType::VIEW_FOCUSED) {
    if (source_node && source_node->IsVisibleToUser()) {
      // Sometimes Android sets focus on unfocusable node, e.g. ListView.
      AccessibilityInfoDataWrapper* adjusted_node =
          UseFullFocusMode()
              ? FindFirstFocusableNodeInFullFocusMode(source_node)
              : source_node;
      if (IsValid(adjusted_node))
        android_focused_id_ = adjusted_node->GetId();
    }
  } else if (event_data.event_type == AXEventType::VIEW_ACCESSIBILITY_FOCUSED &&
             UseFullFocusMode()) {
    if (source_node && source_node->IsVisibleToUser())
      android_focused_id_ = source_node->GetId();
  } else if (event_data.event_type == AXEventType::VIEW_SELECTED) {
    // In Android, VIEW_SELECTED event is dispatched in the two cases below:
    // 1. Changing a value in ProgressBar or TimePicker in ARC P.
    // 2. Selecting an item in the context of an AdapterView.
    if (!source_node || !source_node->IsNode())
      return false;

    AXNodeInfoData* node_info = source_node->GetNode();
    DCHECK(node_info);

    bool is_range_change = !node_info->range_info.is_null();
    if (!is_range_change) {
      AccessibilityInfoDataWrapper* selected_node =
          GetSelectedNodeInfoFromAdapterViewEvent(event_data, source_node);
      if (!selected_node || !selected_node->IsVisibleToUser())
        return false;

      android_focused_id_ = selected_node->GetId();
    }
  } else if (event_data.event_type == AXEventType::WINDOW_STATE_CHANGED) {
    // When accessibility window changed, a11y event of WINDOW_CONTENT_CHANGED
    // is fired from Android multiple times.
    // The event of WINDOW_STATE_CHANGED is fired only once for each window
    // change and use it as a trigger to move the a11y focus to the first node.
    AccessibilityInfoDataWrapper* new_focus = nullptr;

    // If the current window has ever been visited in the current task, try
    // focus on the last focus node in this window.
    // We do it for WINDOW_STATE_CHANGED event from a window or a root node.
    bool from_root_or_window = (source_node && !source_node->IsNode()) ||
                               IsRootOfNodeTree(event_data.source_id);
    auto itr = window_id_to_last_focus_node_id_.find(event_data.window_id);
    if (from_root_or_window && itr != window_id_to_last_focus_node_id_.end())
      new_focus = GetFromId(itr->second);

    // Otherwise, try focus on the first focusable node.
    if (!IsValid(new_focus) && UseFullFocusMode())
      new_focus = FindFirstFocusableNodeInFullFocusMode(
          GetFromId(event_data.source_id));

    if (IsValid(new_focus))
      android_focused_id_ = new_focus->GetId();
  }

  if (!android_focused_id_ || !GetFromId(*android_focused_id_)) {
    AccessibilityInfoDataWrapper* root = GetRoot();
    DCHECK(IsValid(root));
    android_focused_id_ = root_id_;
  }

  if (android_focused_id_.has_value()) {
    window_id_to_last_focus_node_id_[event_data.window_id] =
        *android_focused_id_;
  } else {
    window_id_to_last_focus_node_id_.erase(event_data.window_id);
  }

  AccessibilityInfoDataWrapper* focused_node =
      android_focused_id_.has_value() ? GetFromId(*android_focused_id_)
                                      : nullptr;

  // Ensure that the focused node correctly gets focus.
  while (focused_node && !focused_node->IsImportantInAndroid()) {
    AccessibilityInfoDataWrapper* parent = GetParent(focused_node);
    if (parent) {
      android_focused_id_ = parent->GetId();
      focused_node = parent;
    } else {
      break;
    }
  }

  return true;
}

std::vector<int32_t> AXTreeSourceArc::ProcessHooksOnEvent(
    const AXEventData& event_data) {
  base::EraseIf(hooks_, [this](const auto& it) {
    return this->GetFromId(it.first) == nullptr;
  });

  std::vector<int32_t> serialization_needed_ids;
  for (const auto& modifier : hooks_) {
    if (modifier.second->PreDispatchEvent(this, event_data))
      serialization_needed_ids.push_back(modifier.first);
  }

  // Add new hook implementations if necessary.
  auto drawer_layout_hook =
      DrawerLayoutHandler::CreateIfNecessary(this, event_data);
  if (drawer_layout_hook.has_value())
    hooks_.insert(std::move(*drawer_layout_hook));

  auto auto_complete_hooks =
      AutoCompleteHandler::CreateIfNecessary(this, event_data);
  for (auto& modifier : auto_complete_hooks) {
    if (hooks_.count(modifier.first) == 0)
      hooks_.insert(std::move(modifier));
  }

  return serialization_needed_ids;
}

void AXTreeSourceArc::HandleLiveRegions(std::vector<ui::AXEvent>* events) {
  std::map<int32_t, std::string> new_live_region_map;

  // Cache current live region's name.
  for (auto const& it : tree_map_) {
    if (!it.second->IsNode())
      continue;

    AccessibilityInfoDataWrapper* node_info = it.second.get();
    int32_t live_region_type_int = 0;
    if (!GetProperty(node_info->GetNode()->int_properties,
                     AXIntProperty::LIVE_REGION, &live_region_type_int))
      continue;

    mojom::AccessibilityLiveRegionType live_region_type =
        static_cast<mojom::AccessibilityLiveRegionType>(live_region_type_int);
    if (live_region_type == mojom::AccessibilityLiveRegionType::NONE)
      continue;

    // |node_info| has a live region property.
    std::stack<AccessibilityInfoDataWrapper*> stack;
    stack.push(node_info);
    while (!stack.empty()) {
      AccessibilityInfoDataWrapper* node = stack.top();
      stack.pop();
      DCHECK(node);
      DCHECK(node->IsNode());
      static_cast<AccessibilityNodeInfoDataWrapper*>(node)
          ->set_container_live_status(live_region_type);

      new_live_region_map[node->GetId()] = node->ComputeAXName(true);

      std::vector<int32_t> children;
      if (GetProperty(node->GetNode()->int_list_properties,
                      AXIntListProperty::CHILD_NODE_IDS, &children)) {
        for (const int32_t child : children)
          stack.push(GetFromId(child));
      }
    }
  }

  // Compare to the previous one, and add an event if needed.
  for (const auto& it : new_live_region_map) {
    auto prev_it = previous_live_region_name_.find(it.first);
    if (prev_it == previous_live_region_name_.end())
      continue;

    if (prev_it->second != it.second) {
      events->emplace_back();
      ui::AXEvent& event = events->back();
      event.event_type = ax::mojom::Event::kLiveRegionChanged;
      event.id = it.first;
    }
  }

  std::swap(previous_live_region_name_, new_live_region_map);
}

void AXTreeSourceArc::Reset() {
  tree_map_.clear();
  parent_map_.clear();
  computed_bounds_.clear();
  current_tree_serializer_.reset(new AXTreeArcSerializer(this));
  root_id_.reset();
  window_id_.reset();
  android_focused_id_.reset();
  extensions::AutomationEventRouterInterface* router =
      GetAutomationEventRouter();
  if (!router)
    return;

  router->DispatchTreeDestroyedEvent(ax_tree_id(), nullptr);
}

int32_t AXTreeSourceArc::GetId(AccessibilityInfoDataWrapper* info_data) const {
  if (!info_data)
    return ui::kInvalidAXNodeID;
  return info_data->GetId();
}

void AXTreeSourceArc::GetChildren(
    AccessibilityInfoDataWrapper* info_data,
    std::vector<AccessibilityInfoDataWrapper*>* out_children) const {
  if (!info_data)
    return;

  info_data->GetChildren(out_children);
  if (out_children->empty())
    return;

  if (info_data->IsVirtualNode())
    return;

  std::map<int32_t, size_t> id_to_index;
  for (size_t i = 0; i < out_children->size(); i++) {
    if (out_children->at(i)->IsVirtualNode())
      return;
    id_to_index[out_children->at(i)->GetId()] = i;
  }

  // Sort children based on their enclosing bounding rectangles, based on their
  // descendants.
  std::sort(
      out_children->begin(), out_children->end(),
      [this, &id_to_index](auto left, auto right) {
        auto left_bounds = ComputeEnclosingBounds(left);
        auto right_bounds = ComputeEnclosingBounds(right);

        if (left_bounds.IsEmpty() || right_bounds.IsEmpty()) {
          return id_to_index.at(left->GetId()) < id_to_index.at(right->GetId());
        }

        // Top to bottom sort (non-overlapping).
        if (!left_bounds.Intersects(right_bounds))
          return left_bounds.y() < right_bounds.y();

        // Overlapping
        // Left to right.
        int left_difference = left_bounds.x() - right_bounds.x();
        if (left_difference != 0)
          return left_difference < 0;

        // Top to bottom.
        int top_difference = left_bounds.y() - right_bounds.y();
        if (top_difference != 0)
          return top_difference < 0;

        // Larger to smaller.
        int height_difference = left_bounds.height() - right_bounds.height();
        if (height_difference != 0)
          return height_difference > 0;

        int width_difference = left_bounds.width() - right_bounds.width();
        if (width_difference != 0)
          return width_difference > 0;

        // The rects are equal.
        return id_to_index.at(left->GetId()) < id_to_index.at(right->GetId());
      });
}

bool AXTreeSourceArc::IsIgnored(AccessibilityInfoDataWrapper* info_data) const {
  return false;
}

bool AXTreeSourceArc::IsValid(AccessibilityInfoDataWrapper* info_data) const {
  return info_data;
}

bool AXTreeSourceArc::IsEqual(AccessibilityInfoDataWrapper* info_data1,
                              AccessibilityInfoDataWrapper* info_data2) const {
  if (!info_data1 || !info_data2)
    return false;
  return info_data1->GetId() == info_data2->GetId();
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetNull() const {
  return nullptr;
}

void AXTreeSourceArc::PerformAction(const ui::AXActionData& data) {
  delegate_->OnAction(data);
}

}  // namespace arc
