// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notes/note_remote_updates_handler.h"

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "base/uuid.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "notes/note_node.h"
#include "notes/notes_model.h"
#include "sync/notes/note_specifics_conversions.h"
#include "sync/notes/synced_note_tracker_entity.h"

namespace sync_notes {

namespace {

// Recursive method to traverse a forest created by ReorderValidUpdates() to
// emit updates in top-down order. |ordered_updates| must not be null because
// traversed updates are appended to |*ordered_updates|.
void TraverseAndAppendChildren(
    const base::Uuid& node_uuid,
    const std::unordered_multimap<base::Uuid,
                                  const syncer::UpdateResponseData*,
                                  base::UuidHash>& uuid_to_updates,
    const std::unordered_map<base::Uuid,
                             std::vector<base::Uuid>,
                             base::UuidHash>& node_to_children,
    std::vector<const syncer::UpdateResponseData*>* ordered_updates) {
  // If no children to traverse, we are done.
  if (node_to_children.count(node_uuid) == 0) {
    return;
  }
  // Recurse over all children.
  for (const base::Uuid& child : node_to_children.at(node_uuid)) {
    auto [begin, end] = uuid_to_updates.equal_range(child);
    DCHECK(begin != end);
    for (auto it = begin; it != end; ++it) {
      ordered_updates->push_back(it->second);
    }
    TraverseAndAppendChildren(child, uuid_to_updates, node_to_children,
                              ordered_updates);
  }
}

syncer::UniquePosition ComputeUniquePositionForTrackedNoteNode(
    const SyncedNoteTracker* note_tracker,
    const vivaldi::NoteNode* note_node) {
  DCHECK(note_tracker);

  const SyncedNoteTrackerEntity* child_entity =
      note_tracker->GetEntityForNoteNode(note_node);
  DCHECK(child_entity);
  // TODO(crbug.com/1113139): precompute UniquePosition to prevent its
  // calculation on each remote update.
  return syncer::UniquePosition::FromProto(
      child_entity->metadata().unique_position());
}

size_t ComputeChildNodeIndex(const vivaldi::NoteNode* parent,
                             const sync_pb::UniquePosition& unique_position,
                             const SyncedNoteTracker* note_tracker) {
  DCHECK(parent);
  DCHECK(note_tracker);

  const syncer::UniquePosition position =
      syncer::UniquePosition::FromProto(unique_position);

  auto iter = base::ranges::partition_point(
      parent->children(), [note_tracker, &position](
                              const std::unique_ptr<vivaldi::NoteNode>& child) {
        // Return true for all |parent|'s children whose position is less than
        // |position|.
        return !position.LessThan(
            ComputeUniquePositionForTrackedNoteNode(note_tracker, child.get()));
      });

  return iter - parent->children().begin();
}

bool IsPermanentNodeUpdate(const syncer::EntityData& update_entity) {
  return !update_entity.server_defined_unique_tag.empty();
}

// Checks that the |update_entity| is valid and returns false otherwise. It is
// used to verify non-deletion updates. |update| must not be a deletion and a
// permanent node (they are processed in a different way).
bool IsValidUpdate(const syncer::EntityData& update_entity) {
  DCHECK(!update_entity.is_deleted());
  DCHECK(!IsPermanentNodeUpdate(update_entity));

  if (!IsValidNotesSpecifics(update_entity.specifics.notes())) {
    // Ignore updates with invalid specifics.
    DLOG(ERROR) << "Couldn't process an update note with an invalid specifics.";
    return false;
  }

  if (!HasExpectedNoteGuid(update_entity.specifics.notes(),
                           update_entity.client_tag_hash,
                           update_entity.originator_cache_guid,
                           update_entity.originator_client_item_id)) {
    // Ignore updates with an unexpected UUID.
    DLOG(ERROR) << "Couldn't process an update note with unexpected Uuid: "
                << update_entity.specifics.notes().guid();
    return false;
  }

  return true;
}

// Determines the parent's UUID included in |update_entity|. |update_entity|
// must be a valid update as defined in IsValidUpdate().
base::Uuid GetParentUuidInUpdate(const syncer::EntityData& update_entity) {
  DCHECK(IsValidUpdate(update_entity));
  base::Uuid parent_uuid =
      base::Uuid::ParseLowercase(update_entity.specifics.notes().parent_guid());
  DCHECK(parent_uuid.is_valid());
  return parent_uuid;
}

void ApplyRemoteUpdate(const syncer::UpdateResponseData& update,
                       const SyncedNoteTrackerEntity* tracked_entity,
                       const SyncedNoteTrackerEntity* new_parent_tracked_entity,
                       vivaldi::NotesModel* model,
                       SyncedNoteTracker* tracker) {
  const syncer::EntityData& update_entity = update.entity;
  DCHECK(!update_entity.is_deleted());
  DCHECK(tracked_entity);
  DCHECK(tracked_entity->note_node());
  DCHECK(new_parent_tracked_entity);
  DCHECK(model);
  DCHECK(tracker);
  DCHECK_EQ(tracked_entity->note_node()->uuid(),
            base::Uuid::ParseLowercase(update_entity.specifics.notes().guid()));

  const vivaldi::NoteNode* node = tracked_entity->note_node();
  const vivaldi::NoteNode* old_parent = node->parent();
  const vivaldi::NoteNode* new_parent = new_parent_tracked_entity->note_node();

  DCHECK(old_parent);
  DCHECK(new_parent);
  DCHECK((old_parent->is_folder() && !node->is_attachment()) ||
         (old_parent->is_note() && node->is_attachment()));
  DCHECK((new_parent->is_folder() && !node->is_attachment()) ||
         (new_parent->is_note() && node->is_attachment()));

  if (update_entity.specifics.notes().special_node_type() !=
      GetProtoTypeFromNoteNode(node)) {
    DLOG(ERROR) << "Could not update note node due to conflicting types";
    return;
  }

  UpdateNoteNodeFromSpecifics(update_entity.specifics.notes(), node, model);
  // Compute index information before updating the |tracker|.
  const size_t old_index = old_parent->GetIndexOf(node).value();
  const size_t new_index = ComputeChildNodeIndex(
      new_parent, update_entity.specifics.notes().unique_position(), tracker);
  tracker->Update(tracked_entity, update.response_version,
                  update_entity.modification_time, update_entity.specifics);

  if (new_parent == old_parent &&
      (new_index == old_index || new_index == old_index + 1)) {
    // Node hasn't moved. No more work to do.
    return;
  }
  // Node has moved to another position under the same parent. Update the model.
  // NotesModel takes care of placing the node in the correct position if the
  // node is move to the left. (i.e. no need to subtract one from |new_index|).
  model->Move(node, new_parent, new_index);
}

}  // namespace

NoteRemoteUpdatesHandler::NoteRemoteUpdatesHandler(
    vivaldi::NotesModel* notes_model,
    SyncedNoteTracker* note_tracker)
    : notes_model_(notes_model), note_tracker_(note_tracker) {
  DCHECK(notes_model);
  DCHECK(note_tracker);
}

void NoteRemoteUpdatesHandler::Process(
    const syncer::UpdateResponseDataList& updates,
    bool got_new_encryption_requirements) {
  note_tracker_->CheckAllNodesTracked(notes_model_);
  TRACE_EVENT0("sync", "NoteRemoteUpdatesHandler::Process");

  // If new encryption requirements come from the server, the entities that are
  // in |updates| will be recorded here so they can be ignored during the
  // re-encryption phase at the end.
  std::unordered_set<std::string> entities_with_up_to_date_encryption;

  for (const syncer::UpdateResponseData* update :
       ReorderValidUpdates(&updates)) {
    const syncer::EntityData& update_entity = update->entity;

    DCHECK(!IsPermanentNodeUpdate(update_entity));
    DCHECK(update_entity.is_deleted() || IsValidUpdate(update_entity));

    bool should_ignore_update = false;
    const SyncedNoteTrackerEntity* tracked_entity =
        DetermineLocalTrackedEntityToUpdate(update_entity,
                                            &should_ignore_update);

    if (should_ignore_update) {
      continue;
    }

    // Filter out permanent nodes once again (in case the server tag wasn't
    // populated and yet the entity ID points to a permanent node). This case
    // shoudn't be possible with a well-behaving server.
    if (tracked_entity && tracked_entity->note_node() &&
        tracked_entity->note_node()->is_permanent_node()) {
      DLOG(ERROR) << "Ignoring update to permanent node without server defined "
                     "unique tag for ID "
                  << update_entity.id;
      continue;
    }

    // Ignore updates that have already been seen according to the version.
    if (tracked_entity && tracked_entity->metadata().server_version() >=
                              update->response_version) {
      if (update_entity.id == tracked_entity->metadata().server_id()) {
        // Seen this update before. This update may be a reflection and may have
        // missing the UUID in specifics. Next reupload will populate UUID in
        // specifics and this codepath will not repeat indefinitely. This logic
        // is needed for the case when there is only one device and hence the
        // UUID will not be set by other devices.
        ReuploadEntityIfNeeded(update_entity, tracked_entity);
      }
      continue;
    }

    // The server ID has changed for a tracked entity (matched via client tag).
    // This can happen if a commit succeeds, but the response does not come back
    // fast enough(e.g. before shutdown or crash), then the |note_tracker_|
    // might assume that it was never committed. The server will track the
    // client that sent up the original commit and return this in a get updates
    // response. This also may happen due to duplicate UUIDs. In this case it's
    // better to update to the latest server ID.
    if (tracked_entity) {
      note_tracker_->UpdateSyncIdIfNeeded(tracked_entity,
                                          /*sync_id=*/update_entity.id);
    }

    if (tracked_entity && tracked_entity->IsUnsynced()) {
      tracked_entity = ProcessConflict(*update, tracked_entity);
      if (!tracked_entity) {
        // During conflict resolution, the entity could be dropped in case of
        // a conflict between local and remote deletions. We shouldn't worry
        // about changes to the encryption in that case.
        continue;
      }
    } else if (update_entity.is_deleted()) {
      ProcessDelete(update_entity, tracked_entity);
      // If the local entity has been deleted, no need to check for out of date
      // encryption. Therefore, we can go ahead and process the next update.
      continue;
    } else if (!tracked_entity) {
      tracked_entity = ProcessCreate(*update);
      if (!tracked_entity) {
        // If no new node has been tracked, we shouldn't worry about changes to
        // the encryption.
        continue;
      }
      DCHECK_EQ(tracked_entity,
                note_tracker_->GetEntityForSyncId(update_entity.id));
    } else {
      ProcessUpdate(*update, tracked_entity);
      DCHECK_EQ(tracked_entity,
                note_tracker_->GetEntityForSyncId(update_entity.id));
    }

    // If the received entity has out of date encryption, we schedule another
    // commit to fix it.
    if (note_tracker_->model_type_state().encryption_key_name() !=
        update->encryption_key_name) {
      DVLOG(2) << "Notes: Requesting re-encrypt commit "
               << update->encryption_key_name << " -> "
               << note_tracker_->model_type_state().encryption_key_name();
      note_tracker_->IncrementSequenceNumber(tracked_entity);
    }

    if (got_new_encryption_requirements) {
      entities_with_up_to_date_encryption.insert(update_entity.id);
    }
  }

  // Recommit entities with out of date encryption.
  if (got_new_encryption_requirements) {
    std::vector<const SyncedNoteTrackerEntity*> all_entities =
        note_tracker_->GetAllEntities();
    for (const SyncedNoteTrackerEntity* entity : all_entities) {
      // No need to recommit tombstones and permanent nodes.
      if (entity->metadata().is_deleted()) {
        continue;
      }
      DCHECK(entity->note_node());
      if (entity->note_node()->is_permanent_node()) {
        continue;
      }
      if (entities_with_up_to_date_encryption.count(
              entity->metadata().server_id()) != 0) {
        continue;
      }
      note_tracker_->IncrementSequenceNumber(entity);
    }
  }
  note_tracker_->CheckAllNodesTracked(notes_model_);
}

// static
std::vector<const syncer::UpdateResponseData*>
NoteRemoteUpdatesHandler::ReorderValidUpdatesForTest(
    const syncer::UpdateResponseDataList* updates) {
  return ReorderValidUpdates(updates);
}

// static
size_t NoteRemoteUpdatesHandler::ComputeChildNodeIndexForTest(
    const vivaldi::NoteNode* parent,
    const sync_pb::UniquePosition& unique_position,
    const SyncedNoteTracker* notetracker) {
  return ComputeChildNodeIndex(parent, unique_position, notetracker);
}

// static
std::vector<const syncer::UpdateResponseData*>
NoteRemoteUpdatesHandler::ReorderValidUpdates(
    const syncer::UpdateResponseDataList* updates) {
  // This method sorts the remote updates according to the following rules:
  // 1. Creations and updates come before deletions.
  // 2. Parent creation/update should come before child creation/update.
  // 3. No need to further order deletions. Parent deletions can happen before
  //    child deletions. This is safe because all updates (e.g. moves) should
  //    have been processed already.

  // The algorithm works by constructing a forest of all non-deletion updates
  // and then traverses each tree in the forest recursively: Forest
  // Construction:
  // 1. Iterate over all updates and construct the |parent_to_children| map and
  //    collect all parents in |roots|.
  // 2. Iterate over all updates again and drop any parent that has a
  //    coressponding update. What's left in |roots| are the roots of the
  //    forest.
  // 3. Start at each root in |roots|, emit the update and recurse over its
  //    children.

  // Normally there shouldn't be multiple updates for the same UUID, but let's
  // avoiding dedupping here just in case (e.g. the could in theory be a
  // combination of client-tagged and non-client-tagged updated that
  // ModelTypeWorker failed to deduplicate.
  std::unordered_multimap<base::Uuid, const syncer::UpdateResponseData*,
                          base::UuidHash>
      uuid_to_updates;

  // Add only valid, non-deletions to |uuid_to_updates|.
  int invalid_updates_count = 0;
  int root_node_updates_count = 0;
  for (const syncer::UpdateResponseData& update : *updates) {
    const syncer::EntityData& update_entity = update.entity;
    // Ignore updates to root nodes.
    if (IsPermanentNodeUpdate(update_entity)) {
      ++root_node_updates_count;
      continue;
    }
    if (update_entity.is_deleted()) {
      continue;
    }
    if (!IsValidUpdate(update_entity)) {
      ++invalid_updates_count;
      continue;
    }
    base::Uuid uuid =
        base::Uuid::ParseLowercase(update_entity.specifics.notes().guid());
    DCHECK(uuid.is_valid());
    uuid_to_updates.emplace(std::move(uuid), &update);
  }

  // Iterate over |uuid_to_updates| and construct |roots| and
  // |parent_to_children|.
  std::set<base::Uuid> roots;
  std::unordered_map<base::Uuid, std::vector<base::Uuid>, base::UuidHash>
      parent_to_children;
  for (const auto& [uuid, update] : uuid_to_updates) {
    base::Uuid parent_uuid = GetParentUuidInUpdate(update->entity);
    base::Uuid child_uuid =
        base::Uuid::ParseLowercase(update->entity.specifics.notes().guid());
    DCHECK(child_uuid.is_valid());

    parent_to_children[parent_uuid].emplace_back(std::move(child_uuid));
    // If this entity's parent has no pending update, add it to |roots|.
    if (uuid_to_updates.count(parent_uuid) == 0) {
      roots.insert(std::move(parent_uuid));
    }
  }
  // |roots| contains only root of all trees in the forest all of which are
  // ready to be processed because none has a pending update.
  std::vector<const syncer::UpdateResponseData*> ordered_updates;
  for (const base::Uuid& root : roots) {
    TraverseAndAppendChildren(root, uuid_to_updates, parent_to_children,
                              &ordered_updates);
  }
  // Add deletions.
  for (const syncer::UpdateResponseData& update : *updates) {
    const syncer::EntityData& update_entity = update.entity;
    if (!IsPermanentNodeUpdate(update_entity) && update_entity.is_deleted()) {
      ordered_updates.push_back(&update);
    }
  }
  // All non root updates should have been included in |ordered_updates|.
  DCHECK_EQ(updates->size(), ordered_updates.size() + root_node_updates_count +
                                 invalid_updates_count);
  return ordered_updates;
}

const SyncedNoteTrackerEntity*
NoteRemoteUpdatesHandler::DetermineLocalTrackedEntityToUpdate(
    const syncer::EntityData& update_entity,
    bool* should_ignore_update) {
  *should_ignore_update = false;

  // If there's nothing other than a server ID to issue a lookup, just do that
  // and return immediately. This is the case for permanent nodes and possibly
  // tombstones (at least the LoopbackServer only sets the server ID).
  if (update_entity.originator_client_item_id.empty() &&
      update_entity.client_tag_hash.value().empty()) {
    return note_tracker_->GetEntityForSyncId(update_entity.id);
  }

  // Parse the client tag hash in the update or infer it from the originator
  // information (all of which are immutable properties of a sync entity).
  const syncer::ClientTagHash client_tag_hash_in_update =
      !update_entity.client_tag_hash.value().empty()
          ? update_entity.client_tag_hash
          : SyncedNoteTracker::GetClientTagHashFromUuid(
                InferGuidFromLegacyOriginatorId(
                    update_entity.originator_cache_guid,
                    update_entity.originator_client_item_id));

  const SyncedNoteTrackerEntity* const tracked_entity_by_client_tag =
      note_tracker_->GetEntityForClientTagHash(client_tag_hash_in_update);
  const SyncedNoteTrackerEntity* const tracked_entity_by_sync_id =
      note_tracker_->GetEntityForSyncId(update_entity.id);

  // The most common scenario is that both lookups, client-tag-based and
  // server-ID-based, refer to the same tracked entity or both lookups fail. In
  // that case there's nothing to reconcile and the function can return
  // trivially.
  if (tracked_entity_by_client_tag == tracked_entity_by_sync_id) {
    return tracked_entity_by_client_tag;
  }

  // Client-tags (UUIDs) are known at all times and immutable (as opposed to
  // server IDs which get a temp value for local creations), so they cannot have
  // changed.
  if (tracked_entity_by_sync_id &&
      tracked_entity_by_sync_id->GetClientTagHash() !=
          client_tag_hash_in_update) {
    // The client tag has changed for an already-tracked entity, which is a
    // protocol violation. This should be practically unreachable, but guard
    // against misbehaving servers.
    DLOG(ERROR) << "Ignoring remote note update with protocol violation: "
                   "Uuid must be immutable";
    *should_ignore_update = true;
    return nullptr;
  }

  // At this point |tracked_entity_by_client_tag| must be non-null because
  // otherwise one of the two codepaths above would have returned early.
  DCHECK(tracked_entity_by_client_tag);
  DCHECK(!tracked_entity_by_sync_id);

  return tracked_entity_by_client_tag;
}

const SyncedNoteTrackerEntity* NoteRemoteUpdatesHandler::ProcessCreate(
    const syncer::UpdateResponseData& update) {
  const syncer::EntityData& update_entity = update.entity;
  DCHECK(!update_entity.is_deleted());
  DCHECK(!IsPermanentNodeUpdate(update_entity));
  DCHECK(IsValidNotesSpecifics(update_entity.specifics.notes()));

  const vivaldi::NoteNode* parent_node = GetParentNode(update_entity);
  if (!parent_node) {
    // If we cannot find the parent, we can do nothing.
    note_tracker_->RecordIgnoredServerUpdateDueToMissingParent(
        update.response_version);
    return nullptr;
  }
  if (!((parent_node->is_folder() &&
         update_entity.specifics.notes().special_node_type() !=
             sync_pb::NotesSpecifics::ATTACHMENT) ||
        (parent_node->is_note() &&
         update_entity.specifics.notes().special_node_type() ==
             sync_pb::NotesSpecifics::ATTACHMENT))) {
    return nullptr;
  }
  const vivaldi::NoteNode* note_node = CreateNoteNodeFromSpecifics(
      update_entity.specifics.notes(), parent_node,
      ComputeChildNodeIndex(parent_node,
                            update_entity.specifics.notes().unique_position(),
                            note_tracker_),
      notes_model_);
  DCHECK(note_node);
  const SyncedNoteTrackerEntity* entity =
      note_tracker_->Add(note_node, update_entity.id, update.response_version,
                         update_entity.creation_time, update_entity.specifics);
  ReuploadEntityIfNeeded(update_entity, entity);
  return entity;
}

void NoteRemoteUpdatesHandler::ProcessUpdate(
    const syncer::UpdateResponseData& update,
    const SyncedNoteTrackerEntity* tracked_entity) {
  const syncer::EntityData& update_entity = update.entity;
  // Can only update existing nodes.
  DCHECK(tracked_entity);
  DCHECK(tracked_entity->note_node());
  DCHECK(!tracked_entity->note_node()->is_permanent_node());
  DCHECK_EQ(tracked_entity,
            note_tracker_->GetEntityForSyncId(update_entity.id));
  // Must not be a deletion.
  DCHECK(!update_entity.is_deleted());
  DCHECK(!IsPermanentNodeUpdate(update_entity));
  DCHECK(IsValidNotesSpecifics(update_entity.specifics.notes()));
  DCHECK(!tracked_entity->IsUnsynced());

  const vivaldi::NoteNode* node = tracked_entity->note_node();
  const vivaldi::NoteNode* old_parent = node->parent();
  DCHECK(old_parent);
  DCHECK((old_parent->is_folder() && !node->is_attachment()) ||
         (old_parent->is_note() && node->is_attachment()));

  const SyncedNoteTrackerEntity* new_parent_entity =
      note_tracker_->GetEntityForUuid(GetParentUuidInUpdate(update_entity));
  if (!new_parent_entity) {
    return;
  }
  const vivaldi::NoteNode* new_parent = new_parent_entity->note_node();
  if (!new_parent) {
    return;
  }
  if (!((new_parent->is_folder() && !node->is_attachment()) ||
        (new_parent->is_note() && node->is_attachment()))) {
    return;
  }
  // Node update could be either in the node data (e.g. title or
  // unique_position), or it could be that the node has moved under another
  // parent without any data change.
  if (tracked_entity->MatchesData(update_entity)) {
    DCHECK_EQ(new_parent, old_parent);
    note_tracker_->Update(tracked_entity, update.response_version,
                          update_entity.modification_time,
                          update_entity.specifics);
    ReuploadEntityIfNeeded(update_entity, tracked_entity);
    return;
  }
  ApplyRemoteUpdate(update, tracked_entity, new_parent_entity, notes_model_,
                    note_tracker_);
  ReuploadEntityIfNeeded(update_entity, tracked_entity);
}

void NoteRemoteUpdatesHandler::ProcessDelete(
    const syncer::EntityData& update_entity,
    const SyncedNoteTrackerEntity* tracked_entity) {
  DCHECK(update_entity.is_deleted());

  DCHECK_EQ(tracked_entity,
            note_tracker_->GetEntityForSyncId(update_entity.id));

  // Handle corner cases first.
  if (tracked_entity == nullptr) {
    // Process deletion only if the entity is still tracked. It could have
    // been recursively deleted already with an earlier deletion of its
    // parent.
    DVLOG(1) << "Received remote delete for a non-existing item.";
    return;
  }

  const vivaldi::NoteNode* node = tracked_entity->note_node();
  DCHECK(node);
  // Changes to permanent nodes have been filtered out earlier.
  DCHECK(!node->is_permanent_node());
  // Remove the entities of |node| and its children.
  RemoveEntityAndChildrenFromTracker(node);
  // Remove the node and its children from the model.
  notes_model_->Remove(node);
}

// This method doesn't explicitly handle conflicts as a result of re-encryption:
// remote update wins even if there wasn't a real change in specifics. However,
// this scenario is very unlikely and hence the implementation is less
// sophisticated than in ClientTagBasedModelTypeProcessor (it would require
// introducing base hash specifics to track remote changes).
const SyncedNoteTrackerEntity* NoteRemoteUpdatesHandler::ProcessConflict(
    const syncer::UpdateResponseData& update,
    const SyncedNoteTrackerEntity* tracked_entity) {
  const syncer::EntityData& update_entity = update.entity;

  // Can only conflict with existing nodes.
  DCHECK(tracked_entity);
  DCHECK_EQ(tracked_entity,
            note_tracker_->GetEntityForSyncId(update_entity.id));
  DCHECK(!tracked_entity->note_node() ||
         !tracked_entity->note_node()->is_permanent_node());
  DCHECK(!IsPermanentNodeUpdate(update_entity));

  if (tracked_entity->metadata().is_deleted() && update_entity.is_deleted()) {
    // Both have been deleted, delete the corresponding entity from the tracker.
    note_tracker_->Remove(tracked_entity);
    DLOG(WARNING) << "Conflict: CHANGES_MATCH";
    return nullptr;
  }

  if (update_entity.is_deleted()) {
    // Only remote has been deleted. Local wins. Record that we received the
    // update from the server but leave the pending commit intact.
    note_tracker_->UpdateServerVersion(tracked_entity, update.response_version);
    DLOG(WARNING) << "Conflict: USE_LOCAL";
    return tracked_entity;
  }

  DCHECK(IsValidNotesSpecifics(update_entity.specifics.notes()));

  if (tracked_entity->metadata().is_deleted()) {
    // Only local node has been deleted. It should be restored from the server
    // data as a remote creation.
    note_tracker_->Remove(tracked_entity);
    DLOG(WARNING) << "Conflict: USE_REMOTE";
    return ProcessCreate(update);
  }

  // No deletions, there are potentially conflicting updates.
  const vivaldi::NoteNode* node = tracked_entity->note_node();
  const vivaldi::NoteNode* old_parent = node->parent();
  DCHECK(old_parent);
  DCHECK((old_parent->is_folder() && !node->is_attachment()) ||
         (old_parent->is_note() && node->is_attachment()));

  const SyncedNoteTrackerEntity* new_parent_entity =
      note_tracker_->GetEntityForUuid(GetParentUuidInUpdate(update_entity));
  // The |new_parent_entity| could be null in some racy conditions.  For
  // example, when a client A moves a node and deletes the old parent and
  // commits, and then updates the node again, and at the same time client B
  // updates before receiving the move updates. The client B update will arrive
  // at client A after the parent entity has been deleted already.
  if (!new_parent_entity) {
    return tracked_entity;
  }
  const vivaldi::NoteNode* new_parent = new_parent_entity->note_node();
  // |new_parent| would be null if the parent has been deleted locally and not
  // committed yet. Deletions are executed recursively, so a parent deletions
  // entails child deletion, and if this child has been updated on another
  // client, this would cause conflict.
  if (!new_parent) {
    return tracked_entity;
  }
  // Either local and remote data match or server wins, and in both cases we
  // should squash any pending commits.
  note_tracker_->AckSequenceNumber(tracked_entity);

  // Node update could be either in the node data (e.g. title or
  // unique_position), or it could be that the node has moved under another
  // parent without any data change.
  if (tracked_entity->MatchesData(update_entity)) {
    DCHECK_EQ(new_parent, old_parent);
    note_tracker_->Update(tracked_entity, update.response_version,
                          update_entity.modification_time,
                          update_entity.specifics);

    // The changes are identical so there isn't a real conflict.
    DLOG(WARNING) << "Conflict: CHANGES_MATCH";
  } else {
    // Conflict where data don't match and no remote deletion, and hence server
    // wins. Update the model from server data.
    DLOG(WARNING) << "Conflict: USE_REMOTE";
    ApplyRemoteUpdate(update, tracked_entity, new_parent_entity, notes_model_,
                      note_tracker_);
  }
  ReuploadEntityIfNeeded(update_entity, tracked_entity);
  return tracked_entity;
}

void NoteRemoteUpdatesHandler::RemoveEntityAndChildrenFromTracker(
    const vivaldi::NoteNode* node) {
  DCHECK(node);
  DCHECK(!node->is_permanent_node());

  const SyncedNoteTrackerEntity* entity =
      note_tracker_->GetEntityForNoteNode(node);
  DCHECK(entity);
  note_tracker_->Remove(entity);

  for (const auto& child : node->children()) {
    RemoveEntityAndChildrenFromTracker(child.get());
  }
}

const vivaldi::NoteNode* NoteRemoteUpdatesHandler::GetParentNode(
    const syncer::EntityData& update_entity) const {
  DCHECK(IsValidNotesSpecifics(update_entity.specifics.notes()));

  const SyncedNoteTrackerEntity* parent_entity =
      note_tracker_->GetEntityForUuid(GetParentUuidInUpdate(update_entity));
  if (!parent_entity) {
    return nullptr;
  }
  return parent_entity->note_node();
}

void NoteRemoteUpdatesHandler::ReuploadEntityIfNeeded(
    const syncer::EntityData& entity_data,
    const SyncedNoteTrackerEntity* tracked_entity) {
  DCHECK(tracked_entity);
  DCHECK_EQ(tracked_entity->metadata().server_id(), entity_data.id);
  DCHECK(!tracked_entity->note_node() ||
         !tracked_entity->note_node()->is_permanent_node());

  // Do not initiate reupload if the local entity is a tombstone.
  const bool is_reupload_needed =
      tracked_entity->note_node() && IsNoteEntityReuploadNeeded(entity_data);
  if (is_reupload_needed) {
    note_tracker_->IncrementSequenceNumber(tracked_entity);
  }
}

}  // namespace sync_notes
