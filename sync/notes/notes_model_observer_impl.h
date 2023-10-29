// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTES_NOTES_MODEL_OBSERVER_IMPL_H_
#define SYNC_NOTES_NOTES_MODEL_OBSERVER_IMPL_H_

#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "notes/note_node.h"
#include "notes/notes_model_observer.h"
#include "url/gurl.h"

namespace sync_pb {
class EntitySpecifics;
}

namespace syncer {
class UniquePosition;
}

namespace sync_notes {

class SyncedNoteTracker;

// Class for listening to local changes in the notes model and updating
// metadata in SyncedNoteTracker, such that ultimately the processor exposes
// those local changes to the sync engine.
class NotesModelObserverImpl : public vivaldi::NotesModelObserver {
 public:
  // |note_tracker_| must not be null and must outlive this object.
  NotesModelObserverImpl(const base::RepeatingClosure& nudge_for_commit_closure,
                         base::OnceClosure on_notes_model_being_deleted_closure,
                         SyncedNoteTracker* note_tracker);

  NotesModelObserverImpl(const NotesModelObserverImpl&) = delete;
  NotesModelObserverImpl& operator=(const NotesModelObserverImpl&) = delete;

  ~NotesModelObserverImpl() override;

  //  vivaldi::NotesModelObserver:
  void NotesModelLoaded(vivaldi::NotesModel* model,
                        bool ids_reassigned) override;
  void NotesModelBeingDeleted(vivaldi::NotesModel* model) override;
  void NotesNodeMoved(vivaldi::NotesModel* model,
                      const vivaldi::NoteNode* old_parent,
                      size_t old_index,
                      const vivaldi::NoteNode* new_parent,
                      size_t new_index) override;
  void NotesNodeAdded(vivaldi::NotesModel* model,
                      const vivaldi::NoteNode* parent,
                      size_t index) override;
  void OnWillRemoveNotes(vivaldi::NotesModel* model,
                         const vivaldi::NoteNode* parent,
                         size_t old_index,
                         const vivaldi::NoteNode* node) override;
  void NotesNodeRemoved(vivaldi::NotesModel* model,
                        const vivaldi::NoteNode* parent,
                        size_t old_index,
                        const vivaldi::NoteNode* node) override;
  void NotesNodeChanged(vivaldi::NotesModel* model,
                        const vivaldi::NoteNode* node) override;
  void NotesNodeChildrenReordered(vivaldi::NotesModel* model,
                                  const vivaldi::NoteNode* node) override;
  void OnWillRemoveAllNotes(vivaldi::NotesModel* model) override;
  void NotesAllNodesRemoved(vivaldi::NotesModel* model) override;

 private:
  syncer::UniquePosition ComputePosition(const vivaldi::NoteNode& parent,
                                         size_t index,
                                         const std::string& sync_id);

  // Processes the deletion of a note node and updates the
  // |note_tracker_| accordingly. If |node| is a note, it gets marked
  // as deleted and that it requires a commit. If it's a folder, it recurses
  // over all children before processing the folder itself.
  void ProcessDelete(const vivaldi::NoteNode* node);

  // Returns current unique_position from sync metadata for the tracked |node|.
  syncer::UniquePosition GetUniquePositionForNode(
      const vivaldi::NoteNode* node) const;

  // Updates the unique position in sync metadata for the tracked |node| and
  // returns the new position. A new position is generated based on the left and
  // right node's positions. At least one of |prev| and |next| must be valid.
  syncer::UniquePosition UpdateUniquePositionForNode(
      const vivaldi::NoteNode* node,
      vivaldi::NotesModel* model,
      const syncer::UniquePosition& prev,
      const syncer::UniquePosition& next);

  // Updates unique positions for all children from |parent| starting from
  // |start_index| (must not be 0).
  void UpdateAllUniquePositionsStartingAt(const vivaldi::NoteNode* parent,
                                          vivaldi::NotesModel* notes_model,
                                          size_t start_index);

  // Points to the tracker owned by the processor. It keeps the mapping between
  // note nodes and corresponding sync server entities.
  const raw_ptr<SyncedNoteTracker> note_tracker_;

  // The callback used to inform the sync engine that there are local changes to
  // be committed.
  const base::RepeatingClosure nudge_for_commit_closure_;

  // The callback used to inform the processor that the note is getting
  // deleted.
  base::OnceClosure on_notes_model_being_deleted_closure_;
};

}  // namespace sync_notes

#endif  // SYNC_NOTES_NOTES_MODEL_OBSERVER_IMPL_H_
